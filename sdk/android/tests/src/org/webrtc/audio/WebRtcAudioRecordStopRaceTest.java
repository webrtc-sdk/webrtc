/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.audio;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.os.Build;
import androidx.test.runner.AndroidJUnit4;
import java.lang.reflect.Field;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.CyclicBarrier;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Supplier;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

/**
 * Tests for the WebRtcAudioRecord stop-path contract: a stop must never release the AudioRecord
 * while the reader thread may still be inside a blocking AudioRecord.read() — on a device that
 * corrupts the platform AudioRecord client proxy and aborts the process ("releaseBuffer:
 * mUnreleased out of range"). A stop whose reader join times out hands release ownership to the
 * reader instead; a leaked reader must confine its exit cleanup to the record it was reading and
 * never touch a successor session's record; and a shutdown-stopped record must not be misreported
 * as a start failure, which would permanently disable useAudioRecord and silence later sessions.
 *
 * <p>The AudioRecord is a Mockito fake with a deterministic state machine whose read() blocks like
 * a stalled capture HAL: a STALLED read returns only once the record leaves the recording state
 * (the platform behavior for a blocking read), while a WEDGED read ignores stop() entirely and
 * returns only when explicitly unstuck, forcing the stop path's join to time out. The record and
 * capture buffer are injected by reflection because initRecordingImpl needs a direct ByteBuffer
 * with a backing array, which exists on ART but not on the test JVM; the start/stop machinery
 * under test runs unmodified.
 */
@RunWith(AndroidJUnit4.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.O)
public class WebRtcAudioRecordStopRaceTest {
  private static final int CAPTURE_BUFFER_BYTES = 960;
  private static final long AWAIT_TIMEOUT_MS = 5000;
  private static final long DRAIN_TIMEOUT_MS = 3000;
  private static final int SAMPLE_RATE = 48000;
  private static final int CHANNELS = 1;

  /** Deterministic AudioRecord fake whose read() blocks like a stalled capture HAL. */
  private static class FakeRecord {
    /** read() returns once the record leaves the recording state, like a blocking HAL read. */
    static final int STALLED = 0;
    /** read() ignores stop() and returns only when unstuck; forces the stop-path join timeout. */
    static final int WEDGED = 1;

    final AudioRecord record = mock(AudioRecord.class);
    final AtomicBoolean recording = new AtomicBoolean(false);
    final AtomicBoolean released = new AtomicBoolean(false);
    final AtomicInteger releaseCount = new AtomicInteger(0);
    final AtomicInteger readsStarted = new AtomicInteger(0);
    final AtomicBoolean inRead = new AtomicBoolean(false);
    final AtomicBoolean releasedDuringRead = new AtomicBoolean(false);
    final AtomicBoolean stoppedCleanly = new AtomicBoolean(false);
    final CountDownLatch unstick = new CountDownLatch(1);

    FakeRecord(int readBehavior) {
      when(record.getState())
          .thenAnswer(invocation
              -> released.get() ? AudioRecord.STATE_UNINITIALIZED : AudioRecord.STATE_INITIALIZED);
      when(record.getRecordingState())
          .thenAnswer(invocation
              -> recording.get() ? AudioRecord.RECORDSTATE_RECORDING
                                 : AudioRecord.RECORDSTATE_STOPPED);
      doAnswer(invocation -> {
        recording.set(true);
        return null;
      }).when(record).startRecording();
      doAnswer(invocation -> {
        recording.set(false);
        return null;
      }).when(record).stop();
      doAnswer(invocation -> {
        recording.set(false);
        released.set(true);
        releaseCount.incrementAndGet();
        if (inRead.get()) {
          // On a device this is the releaseBuffer abort: release() landed while a reader was
          // still inside read().
          releasedDuringRead.set(true);
        }
        return null;
      }).when(record).release();
      when(record.read(any(ByteBuffer.class), anyInt())).thenAnswer(invocation -> {
        readsStarted.incrementAndGet();
        inRead.set(true);
        try {
          if (readBehavior == WEDGED) {
            unstick.await();
            return 0;
          }
          while (recording.get() && !released.get()) {
            Thread.sleep(1);
          }
          if (!released.get()) {
            stoppedCleanly.set(true);
          }
          return 0;
        } finally {
          inRead.set(false);
        }
      });
    }
  }

  private ScheduledExecutorService scheduler;
  private final List<FakeRecord> fakes = new ArrayList<>();

  @Before
  public void setUp() {
    scheduler = Executors.newSingleThreadScheduledExecutor();
  }

  @After
  public void tearDown() throws InterruptedException {
    // A failed assertion mid-test must not leak a blocked reader into the next test: unblock
    // every fake read and drain before shutting down.
    for (FakeRecord fake : fakes) {
      fake.recording.set(false);
      fake.unstick.countDown();
    }
    long deadline = System.currentTimeMillis() + DRAIN_TIMEOUT_MS;
    while (readerThreadCount() > 0 && System.currentTimeMillis() < deadline) {
      Thread.sleep(10);
    }
    scheduler.shutdownNow();
  }

  @Test
  public void stopDuringStalledRead_neverReleasesRecordUnderTheReader() throws Exception {
    FakeRecord fake = newFake(FakeRecord.STALLED);
    WebRtcAudioRecord webRtcAudioRecord = createWebRtcAudioRecord(null);
    injectRecordingState(webRtcAudioRecord, fake.record);
    assertTrue(webRtcAudioRecord.startRecordingIfNeeded());
    awaitTrue("reader never entered read()", () -> fake.readsStarted.get() > 0);

    long stopStartedAtMs = System.currentTimeMillis();
    assertTrue(webRtcAudioRecord.stopRecordingIfNeeded());
    long stopDurationMs = System.currentTimeMillis() - stopStartedAtMs;

    awaitTrue("reader thread leaked past stop", () -> readerThreadCount() == 0);
    assertFalse("AudioRecord.release() was invoked while a read was in flight — on a device this"
            + " is the releaseBuffer abort",
        fake.releasedDuringRead.get());
    // The stop must unblock the stalled read via AudioRecord.stop() instead of burning the 2s
    // join timeout and abandoning the thread.
    assertTrue("stopRecordingIfNeeded took " + stopDurationMs
            + "ms — it hit the join timeout instead of stopping the record to unblock the read",
        stopDurationMs < 1500);
    assertTrue("read did not observe a clean stop", fake.stoppedCleanly.get());
    assertEquals("record must be released exactly once", 1, fake.releaseCount.get());
  }

  @Test
  public void stopThenImmediateRestart_neverLeaksReadersOrReleasesUnderRead() throws Exception {
    WebRtcAudioRecord webRtcAudioRecord = createWebRtcAudioRecord(null);
    for (int cycle = 0; cycle < 5; cycle++) {
      FakeRecord fake = newFake(FakeRecord.STALLED);
      injectRecordingState(webRtcAudioRecord, fake.record);
      assertTrue("start cycle=" + cycle, webRtcAudioRecord.startRecordingIfNeeded());
      Thread.sleep(30);
      assertTrue("stop cycle=" + cycle, webRtcAudioRecord.stopRecordingIfNeeded());
      awaitTrue("leaked reader threads after cycle=" + cycle, () -> readerThreadCount() == 0);
    }
    for (FakeRecord fake : fakes) {
      assertFalse("release-during-read violation", fake.releasedDuringRead.get());
      assertEquals("record must be released exactly once", 1, fake.releaseCount.get());
    }
  }

  /**
   * The join-timeout branch: when not even a stopped record unblocks the read (wedged HAL), the
   * stop path must return without releasing the record under the in-flight read — release
   * ownership transfers to the reader, which releases the orphan on exit.
   */
  @Test
  public void joinTimeout_handsRecordToReaderInsteadOfReleasingUnderIt() throws Exception {
    FakeRecord fake = newFake(FakeRecord.WEDGED);
    WebRtcAudioRecord webRtcAudioRecord = createWebRtcAudioRecord(null);
    injectRecordingState(webRtcAudioRecord, fake.record);
    assertTrue(webRtcAudioRecord.startRecordingIfNeeded());
    awaitTrue("reader never entered read()", () -> fake.readsStarted.get() > 0);

    long stopStartedAtMs = System.currentTimeMillis();
    assertTrue(webRtcAudioRecord.stopRecordingIfNeeded());
    long stopDurationMs = System.currentTimeMillis() - stopStartedAtMs;

    // A wedged read cannot be unblocked, so this stop legitimately burns the full join timeout;
    // the duration proves the timeout branch actually ran.
    assertTrue("stop returned in " + stopDurationMs
            + "ms — the wedged read should have forced the join timeout",
        stopDurationMs >= 1900);
    assertTrue("reader should still be wedged inside read()", readerThreadCount() > 0);
    assertFalse("record was released under the wedged read", fake.releasedDuringRead.get());
    assertEquals("record must stay unreleased while the reader may still be inside read()", 0,
        fake.releaseCount.get());

    fake.unstick.countDown();
    awaitTrue("reader did not exit after the read unwedged", () -> readerThreadCount() == 0);
    assertFalse(fake.releasedDuringRead.get());
    assertEquals(
        "reader must release the orphaned record exactly once on exit", 1, fake.releaseCount.get());
  }

  /**
   * A leaked (join-timed-out) reader overlapping a successor session must confine its exit
   * cleanup to the record it was reading: the successor's record keeps recording, untouched.
   */
  @Test
  public void leakedReaderExit_neverTouchesSuccessorRecord() throws Exception {
    FakeRecord wedged = newFake(FakeRecord.WEDGED);
    FakeRecord successor = newFake(FakeRecord.STALLED);
    WebRtcAudioRecord webRtcAudioRecord = createWebRtcAudioRecord(null);
    injectRecordingState(webRtcAudioRecord, wedged.record);
    assertTrue(webRtcAudioRecord.startRecordingIfNeeded());
    awaitTrue("first reader never entered read()", () -> wedged.readsStarted.get() > 0);

    // The wedged read forces the join timeout; the reader leaks by design.
    assertTrue(webRtcAudioRecord.stopRecordingIfNeeded());
    assertEquals("expected exactly the wedged reader alive", 1, readerThreadCount());

    // A successor session starts while the old reader is still inside read().
    injectRecordingState(webRtcAudioRecord, successor.record);
    assertTrue(webRtcAudioRecord.startRecordingIfNeeded());
    awaitTrue("successor reader never entered read()", () -> successor.readsStarted.get() > 0);
    assertEquals(2, readerThreadCount());

    wedged.unstick.countDown();
    awaitTrue("wedged reader did not exit", () -> readerThreadCount() == 1);
    assertEquals("old reader must release its orphaned record exactly once", 1,
        wedged.releaseCount.get());
    verify(successor.record, never()).stop();
    assertEquals("old reader must not stop the successor record",
        AudioRecord.RECORDSTATE_RECORDING, successor.record.getRecordingState());
    assertEquals("old reader must not release the successor record", 0,
        successor.releaseCount.get());
    assertFalse(successor.releasedDuringRead.get());

    assertTrue(webRtcAudioRecord.stopRecordingIfNeeded());
    awaitTrue("successor reader did not exit on clean stop", () -> readerThreadCount() == 0);
    assertFalse(successor.releasedDuringRead.get());
    assertTrue("successor read did not observe a clean stop", successor.stoppedCleanly.get());
    assertEquals(1, successor.releaseCount.get());
  }

  /**
   * Startup-window races: a stop landing immediately after start must not crash the reader
   * preamble, must not report a start/read error for a record the shutdown itself stopped, and
   * must leave capture usable — misreporting shutdown as a start failure permanently disabled
   * useAudioRecord, silencing every later session.
   */
  @Test
  public void immediateStopAfterStart_neverMisreportsErrorOrDisablesCapture() throws Exception {
    List<String> errors = new CopyOnWriteArrayList<>();
    JavaAudioDeviceModule.AudioRecordErrorCallback errorCallback =
        new JavaAudioDeviceModule.AudioRecordErrorCallback() {
          @Override
          public void onWebRtcAudioRecordInitError(String errorMessage) {
            errors.add("init: " + errorMessage);
          }

          @Override
          public void onWebRtcAudioRecordStartError(
              JavaAudioDeviceModule.AudioRecordStartErrorCode errorCode, String errorMessage) {
            errors.add("start " + errorCode + ": " + errorMessage);
          }

          @Override
          public void onWebRtcAudioRecordError(String errorMessage) {
            errors.add("runtime: " + errorMessage);
          }
        };

    WebRtcAudioRecord webRtcAudioRecord = createWebRtcAudioRecord(errorCallback);
    for (int cycle = 0; cycle < 20; cycle++) {
      FakeRecord fake = newFake(FakeRecord.STALLED);
      injectRecordingState(webRtcAudioRecord, fake.record);
      assertTrue("start cycle=" + cycle, webRtcAudioRecord.startRecordingIfNeeded());
      assertTrue("stop cycle=" + cycle, webRtcAudioRecord.stopRecordingIfNeeded());
      awaitTrue("reader leaked after racing cycle=" + cycle, () -> readerThreadCount() == 0);
    }

    // Capture must still work after every racing shutdown.
    FakeRecord finalFake = newFake(FakeRecord.STALLED);
    injectRecordingState(webRtcAudioRecord, finalFake.record);
    assertTrue(webRtcAudioRecord.startRecordingIfNeeded());
    awaitTrue("capture no longer reads after racing stop cycles — useAudioRecord was disabled",
        () -> finalFake.readsStarted.get() > 0);
    assertTrue(webRtcAudioRecord.stopRecordingIfNeeded());
    awaitTrue("reader leaked after final cycle", () -> readerThreadCount() == 0);

    for (FakeRecord fake : fakes) {
      assertFalse("release-during-read violation", fake.releasedDuringRead.get());
    }
    assertTrue("errors reported for clean shutdown races: " + errors, errors.isEmpty());
  }

  /**
   * stopRecordingIfNeededImpl runs outside audioRecordStateLock, making concurrent stop callers
   * a legal interleaving: both must succeed, with one clean release.
   */
  @Test
  public void concurrentStops_bothSucceedWithSingleCleanRelease() throws Exception {
    for (int cycle = 0; cycle < 10; cycle++) {
      FakeRecord fake = newFake(FakeRecord.STALLED);
      WebRtcAudioRecord webRtcAudioRecord = createWebRtcAudioRecord(null);
      injectRecordingState(webRtcAudioRecord, fake.record);
      assertTrue("start cycle=" + cycle, webRtcAudioRecord.startRecordingIfNeeded());
      awaitTrue("cycle=" + cycle + " reader never entered read()",
          () -> fake.readsStarted.get() > 0);

      CyclicBarrier barrier = new CyclicBarrier(2);
      Boolean[] results = new Boolean[2];
      List<Throwable> failures = new CopyOnWriteArrayList<>();
      Thread[] stoppers = new Thread[2];
      for (int i = 0; i < 2; i++) {
        final int index = i;
        stoppers[i] = new Thread(() -> {
          try {
            barrier.await();
            results[index] = webRtcAudioRecord.stopRecordingIfNeeded();
          } catch (Throwable t) {
            failures.add(t);
          }
        });
        stoppers[i].start();
      }
      for (Thread stopper : stoppers) {
        stopper.join(5000);
        assertFalse("cycle=" + cycle + " stopper thread did not finish", stopper.isAlive());
      }
      assertTrue("cycle=" + cycle + " stop callers threw: " + failures, failures.isEmpty());
      assertEquals("cycle=" + cycle + " first stop must succeed", Boolean.TRUE, results[0]);
      assertEquals("cycle=" + cycle + " second stop must succeed", Boolean.TRUE, results[1]);
      awaitTrue("cycle=" + cycle + " reader leaked", () -> readerThreadCount() == 0);
      assertFalse("cycle=" + cycle + " release-during-read", fake.releasedDuringRead.get());
      assertEquals("cycle=" + cycle + " record must be released exactly once", 1,
          fake.releaseCount.get());
    }
  }

  private FakeRecord newFake(int readBehavior) {
    FakeRecord fake = new FakeRecord(readBehavior);
    fakes.add(fake);
    return fake;
  }

  private WebRtcAudioRecord createWebRtcAudioRecord(
      JavaAudioDeviceModule.AudioRecordErrorCallback errorCallback) {
    Context context = RuntimeEnvironment.getApplication();
    AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
    return new WebRtcAudioRecord(context, scheduler, audioManager,
        WebRtcAudioRecord.DEFAULT_AUDIO_SOURCE, WebRtcAudioRecord.DEFAULT_AUDIO_FORMAT,
        errorCallback, null /* stateCallback */, null /* audioSamplesReadyCallback */,
        null /* audioBufferCallback */, false /* isAcousticEchoCancelerSupported */,
        false /* isNoiseSuppressorSupported */, SAMPLE_RATE, CHANNELS);
  }

  private static void injectRecordingState(WebRtcAudioRecord webRtcAudioRecord, AudioRecord record)
      throws Exception {
    ByteBuffer byteBuffer = ByteBuffer.allocate(CAPTURE_BUFFER_BYTES);
    setField(webRtcAudioRecord, "audioRecord", record);
    setField(webRtcAudioRecord, "byteBuffer", byteBuffer);
    setField(webRtcAudioRecord, "emptyBytes", new byte[byteBuffer.capacity()]);
  }

  private static void setField(WebRtcAudioRecord instance, String name, Object value)
      throws Exception {
    Field field = WebRtcAudioRecord.class.getDeclaredField(name);
    field.setAccessible(true);
    field.set(instance, value);
  }

  private static int readerThreadCount() {
    int count = 0;
    for (Thread thread : Thread.getAllStackTraces().keySet()) {
      if (thread.isAlive() && "AudioRecordJavaThread".equals(thread.getName())) {
        count++;
      }
    }
    return count;
  }

  private static void awaitTrue(String message, Supplier<Boolean> condition)
      throws InterruptedException {
    long deadline = System.currentTimeMillis() + AWAIT_TIMEOUT_MS;
    while (!condition.get() && System.currentTimeMillis() < deadline) {
      Thread.sleep(5);
    }
    assertTrue(message, condition.get());
  }
}

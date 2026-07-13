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
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.MediaRecorder.AudioSource;
import android.os.Build;
import androidx.test.runner.AndroidJUnit4;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.webrtc.audio.JavaAudioDeviceModule.AudioBufferCallback;
import org.webrtc.audio.JavaAudioDeviceModule.AudioRecordStateCallback;

@RunWith(AndroidJUnit4.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.M)
public class WebRtcAudioRecordTest {
  @Test
  public void stopRecordingDoesNotHoldStateLocksWhileJoining() throws Exception {
    Context context = RuntimeEnvironment.getApplication();
    AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
    TestWebRtcAudioRecord audioRecord = new TestWebRtcAudioRecord(context, audioManager);
    prepareRecordinglessInput(audioRecord);

    assertTrue(audioRecord.prewarmRecordingIfNeeded());
    assertTrue(audioRecord.stopRecordingIfNeeded());

    assertFalse(audioRecord.audioRecordStateLockWasHeldWhileJoining);
    assertFalse(audioRecord.audioThreadStateLockWasHeldWhileJoining);
    assertNull(getField(audioRecord, "audioThread"));
  }

  @Test
  public void startRecordingReusesResourcesAfterTimedOutStopThreadExits() throws Exception {
    Context context = RuntimeEnvironment.getApplication();
    AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
    TimeoutWebRtcAudioRecord audioRecord = new TimeoutWebRtcAudioRecord(context, audioManager);
    AudioRecord platformAudioRecord = prepareMockedAudioRecord(audioRecord);

    assertTrue(audioRecord.prewarmRecordingIfNeeded());
    Thread stoppedThread = (Thread) getField(audioRecord, "audioThread");
    assertFalse(audioRecord.stopRecordingIfNeeded());

    assertSame(stoppedThread, getField(audioRecord, "audioThread"));
    assertFalse(stoppedThread.isAlive());
    verify(platformAudioRecord, never()).release();

    assertTrue(audioRecord.startRecordingIfNeeded());
    Thread restartedThread = (Thread) getField(audioRecord, "audioThread");
    assertNotNull(restartedThread);
    assertNotSame(stoppedThread, restartedThread);
    assertTrue(restartedThread.isAlive());
    verify(platformAudioRecord, times(2)).startRecording();
    verify(platformAudioRecord, never()).release();

    assertTrue(audioRecord.stopRecordingIfNeeded());
    assertNull(getField(audioRecord, "audioThread"));
    verify(platformAudioRecord).release();
  }

  @Test
  public void prewarmReusesResourcesAfterTimedOutStopThreadExits() throws Exception {
    Context context = RuntimeEnvironment.getApplication();
    AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
    TimeoutWebRtcAudioRecord audioRecord = new TimeoutWebRtcAudioRecord(context, audioManager);
    AudioRecord platformAudioRecord = prepareMockedAudioRecord(audioRecord);

    assertTrue(audioRecord.prewarmRecordingIfNeeded());
    Thread stoppedThread = (Thread) getField(audioRecord, "audioThread");
    assertFalse(audioRecord.stopRecordingIfNeeded());

    assertSame(stoppedThread, getField(audioRecord, "audioThread"));
    assertFalse(stoppedThread.isAlive());
    verify(platformAudioRecord, never()).release();

    assertTrue(audioRecord.prewarmRecordingIfNeeded());
    Thread restartedThread = (Thread) getField(audioRecord, "audioThread");
    assertNotNull(restartedThread);
    assertNotSame(stoppedThread, restartedThread);
    assertTrue(restartedThread.isAlive());
    verify(platformAudioRecord, times(2)).startRecording();
    verify(platformAudioRecord, never()).release();

    assertTrue(audioRecord.stopRecordingIfNeeded());
    assertNull(getField(audioRecord, "audioThread"));
    verify(platformAudioRecord).release();
  }

  @Test
  public void stopCallbackCanRequestRestartWithoutDeadlockingJoin() throws Exception {
    Context context = RuntimeEnvironment.getApplication();
    AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
    RestartOnStopCallback callback = new RestartOnStopCallback();
    CallbackWebRtcAudioRecord audioRecord =
        new CallbackWebRtcAudioRecord(context, audioManager, callback);
    callback.audioRecord = audioRecord;
    prepareRecordinglessInput(audioRecord);

    assertTrue(audioRecord.prewarmRecordingIfNeeded());
    Thread stoppedThread = (Thread) getField(audioRecord, "audioThread");
    assertTrue(audioRecord.stopRecordingIfNeeded());

    assertTrue(callback.restartResult);
    Thread restartedThread = (Thread) getField(audioRecord, "audioThread");
    assertNotNull(restartedThread);
    assertNotSame(stoppedThread, restartedThread);
    assertTrue(restartedThread.isAlive());

    assertTrue(audioRecord.stopRecordingIfNeeded());
    assertNull(getField(audioRecord, "audioThread"));
  }

  @Test
  public void audioRecordThreadKeepsBufferGenerationPairedDuringReplacement() throws Exception {
    Context context = RuntimeEnvironment.getApplication();
    AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
    BlockingAudioBufferCallback callback = new BlockingAudioBufferCallback();
    GenerationWebRtcAudioRecord audioRecord =
        new GenerationWebRtcAudioRecord(context, audioManager, callback);
    prepareRecordinglessInput(audioRecord);

    assertTrue(audioRecord.prewarmRecordingIfNeeded());
    assertTrue(callback.firstBufferReady.await(5, TimeUnit.SECONDS));
    Thread oldThread = (Thread) getField(audioRecord, "audioThread");
    AtomicReference<Throwable> threadFailure = new AtomicReference<>();
    oldThread.setUncaughtExceptionHandler((thread, error) -> threadFailure.set(error));
    try {
      // Simulate overlapping publication of a smaller next-generation buffer. The running thread
      // must continue to use the immutable buffer/silence pair it captured at construction.
      setField(audioRecord, "byteBuffer", ByteBuffer.allocate(480));
      callback.continueFirstBuffer.countDown();

      assertTrue(callback.secondBufferReady.await(5, TimeUnit.SECONDS));
      assertNull(threadFailure.get());
    } finally {
      callback.continueFirstBuffer.countDown();
      setField(audioRecord, "emptyBytes", new byte[480]);
      audioRecord.stopRecordingIfNeeded();
    }
  }

  @Test
  public void audioRecordThreadKeepsMutedBufferGenerationPairedDuringReplacement()
      throws Exception {
    Context context = RuntimeEnvironment.getApplication();
    AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
    BlockingAudioBufferCallback callback = new BlockingAudioBufferCallback();
    GenerationWebRtcAudioRecord audioRecord =
        new GenerationWebRtcAudioRecord(context, audioManager, callback);
    prepareMockedAudioRecord(audioRecord);
    audioRecord.setMicrophoneMute(true);

    assertTrue(audioRecord.prewarmRecordingIfNeeded());
    assertTrue(callback.firstBufferReady.await(5, TimeUnit.SECONDS));
    Thread oldThread = (Thread) getField(audioRecord, "audioThread");
    AtomicReference<Throwable> threadFailure = new AtomicReference<>();
    oldThread.setUncaughtExceptionHandler((thread, error) -> threadFailure.set(error));
    try {
      setField(audioRecord, "byteBuffer", ByteBuffer.allocate(480));
      callback.continueFirstBuffer.countDown();

      assertTrue(callback.secondBufferReady.await(5, TimeUnit.SECONDS));
      assertNull(threadFailure.get());
    } finally {
      callback.continueFirstBuffer.countDown();
      setField(audioRecord, "emptyBytes", new byte[480]);
      audioRecord.stopRecordingIfNeeded();
    }
  }

  @Test
  public void nativeInitDuringPrewarmCachesRunningThreadBufferGeneration() throws Exception {
    Context context = RuntimeEnvironment.getApplication();
    AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
    BlockingAudioBufferCallback callback = new BlockingAudioBufferCallback();
    GenerationWebRtcAudioRecord audioRecord =
        new GenerationWebRtcAudioRecord(context, audioManager, callback);
    ByteBuffer prewarmBuffer = prepareRecordinglessInput(audioRecord);

    assertTrue(audioRecord.prewarmRecordingIfNeeded());
    AtomicReference<Boolean> stopResult = new AtomicReference<>();
    Thread stopper = new Thread(() -> stopResult.set(audioRecord.stopRecordingIfNeeded()));
    try {
      assertTrue(callback.firstBufferReady.await(5, TimeUnit.SECONDS));
      assertSame(prewarmBuffer, callback.firstBuffer.get());

      assertEquals(480, invokeInitRecording(audioRecord, 48000, 1));
      assertSame(prewarmBuffer, audioRecord.cachedBuffer.get());

      stopper.start();
      assertTrue(audioRecord.joinStarted.await(5, TimeUnit.SECONDS));
      callback.continueFirstBuffer.countDown();
      stopper.join(TimeUnit.SECONDS.toMillis(5));

      assertFalse(stopper.isAlive());
      assertEquals(Boolean.TRUE, stopResult.get());
      assertNull(getField(audioRecord, "audioThread"));
    } finally {
      if (stopper.getState() == Thread.State.NEW) {
        stopper.start();
        audioRecord.joinStarted.await(5, TimeUnit.SECONDS);
      }
      callback.continueFirstBuffer.countDown();
      stopper.join(TimeUnit.SECONDS.toMillis(5));
      audioRecord.stopRecordingIfNeeded();
    }
  }

  @Test
  public void nativeInitDuringPrewarmRejectsDifferentBufferConfiguration() throws Exception {
    Context context = RuntimeEnvironment.getApplication();
    AudioManager audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
    BlockingAudioBufferCallback callback = new BlockingAudioBufferCallback();
    GenerationWebRtcAudioRecord audioRecord =
        new GenerationWebRtcAudioRecord(context, audioManager, callback);
    ByteBuffer prewarmBuffer = prepareRecordinglessInput(audioRecord);

    assertTrue(audioRecord.prewarmRecordingIfNeeded());
    AtomicReference<Boolean> stopResult = new AtomicReference<>();
    Thread stopper = new Thread(() -> stopResult.set(audioRecord.stopRecordingIfNeeded()));
    try {
      assertTrue(callback.firstBufferReady.await(5, TimeUnit.SECONDS));

      assertEquals(-1, invokeInitRecording(audioRecord, 24000, 1));
      assertNull(audioRecord.cachedBuffer.get());
      assertSame(prewarmBuffer, getField(audioRecord, "byteBuffer"));
      assertFalse(
          ((AtomicBoolean) getField(audioRecord, "nativeCalledInitRecording")).get());

      stopper.start();
      assertTrue(audioRecord.joinStarted.await(5, TimeUnit.SECONDS));
      callback.continueFirstBuffer.countDown();
      stopper.join(TimeUnit.SECONDS.toMillis(5));

      assertFalse(stopper.isAlive());
      assertEquals(Boolean.TRUE, stopResult.get());
    } finally {
      if (stopper.getState() == Thread.State.NEW) {
        stopper.start();
        audioRecord.joinStarted.await(5, TimeUnit.SECONDS);
      }
      callback.continueFirstBuffer.countDown();
      stopper.join(TimeUnit.SECONDS.toMillis(5));
      audioRecord.stopRecordingIfNeeded();
    }
  }

  private static ByteBuffer prepareRecordinglessInput(WebRtcAudioRecord audioRecord)
      throws Exception {
    ByteBuffer byteBuffer = ByteBuffer.allocate(960);
    setField(audioRecord, "byteBuffer", byteBuffer);
    setField(audioRecord, "emptyBytes", new byte[960]);
    setField(audioRecord, "sampleRate", 48000);
    setField(audioRecord, "channelCount", 1);
    audioRecord.setUseAudioRecord(false);
    return byteBuffer;
  }

  private static AudioRecord prepareMockedAudioRecord(WebRtcAudioRecord audioRecord)
      throws Exception {
    AudioRecord platformAudioRecord = mock(AudioRecord.class);
    when(platformAudioRecord.getRecordingState()).thenReturn(AudioRecord.RECORDSTATE_RECORDING);
    when(platformAudioRecord.read(any(ByteBuffer.class), anyInt()))
        .thenAnswer(invocation -> ((ByteBuffer) invocation.getArgument(0)).capacity());
    setField(audioRecord, "byteBuffer", ByteBuffer.allocate(960));
    setField(audioRecord, "emptyBytes", new byte[960]);
    setField(audioRecord, "sampleRate", 48000);
    setField(audioRecord, "channelCount", 1);
    setField(audioRecord, "audioRecord", platformAudioRecord);
    return platformAudioRecord;
  }

  private static int invokeInitRecording(
      WebRtcAudioRecord audioRecord, int sampleRate, int channelCount) throws Exception {
    Method method =
        WebRtcAudioRecord.class.getDeclaredMethod("initRecording", int.class, int.class);
    method.setAccessible(true);
    return (int) method.invoke(audioRecord, sampleRate, channelCount);
  }

  private static Object getField(Object target, String name) throws Exception {
    Field field = WebRtcAudioRecord.class.getDeclaredField(name);
    field.setAccessible(true);
    return field.get(target);
  }

  private static void setField(Object target, String name, Object value) throws Exception {
    Field field = WebRtcAudioRecord.class.getDeclaredField(name);
    field.setAccessible(true);
    field.set(target, value);
  }

  private static final class TestWebRtcAudioRecord extends WebRtcAudioRecord {
    private final Object audioRecordStateLock;
    private final Object audioThreadStateLock;
    private boolean audioRecordStateLockWasHeldWhileJoining;
    private boolean audioThreadStateLockWasHeldWhileJoining;

    TestWebRtcAudioRecord(Context context, AudioManager audioManager) throws Exception {
      super(
          context,
          WebRtcAudioRecord.newDefaultScheduler(),
          audioManager,
          AudioSource.VOICE_COMMUNICATION,
          AudioFormat.ENCODING_PCM_16BIT,
          null,
          null,
          null,
          null,
          false,
          false,
          48000,
          1);
      audioRecordStateLock = getField(this, "audioRecordStateLock");
      audioThreadStateLock = getField(this, "audioThreadStateLock");
    }

    @Override
    boolean joinAudioRecordThread(Thread thread) {
      audioRecordStateLockWasHeldWhileJoining = Thread.holdsLock(audioRecordStateLock);
      audioThreadStateLockWasHeldWhileJoining = Thread.holdsLock(audioThreadStateLock);
      return super.joinAudioRecordThread(thread);
    }
  }

  private static final class TimeoutWebRtcAudioRecord extends WebRtcAudioRecord {
    private boolean simulateTimeout = true;

    TimeoutWebRtcAudioRecord(Context context, AudioManager audioManager) {
      super(
          context,
          WebRtcAudioRecord.newDefaultScheduler(),
          audioManager,
          AudioSource.VOICE_COMMUNICATION,
          AudioFormat.ENCODING_PCM_16BIT,
          null,
          null,
          null,
          null,
          false,
          false,
          48000,
          1);
    }

    @Override
    boolean joinAudioRecordThread(Thread thread) {
      boolean joined = super.joinAudioRecordThread(thread);
      if (simulateTimeout) {
        simulateTimeout = false;
        assertTrue(joined);
        return false;
      }
      return joined;
    }
  }

  private static final class CallbackWebRtcAudioRecord extends WebRtcAudioRecord {
    CallbackWebRtcAudioRecord(
        Context context, AudioManager audioManager, AudioRecordStateCallback stateCallback) {
      super(
          context,
          WebRtcAudioRecord.newDefaultScheduler(),
          audioManager,
          AudioSource.VOICE_COMMUNICATION,
          AudioFormat.ENCODING_PCM_16BIT,
          null,
          stateCallback,
          null,
          null,
          false,
          false,
          48000,
          1);
    }
  }

  private static final class GenerationWebRtcAudioRecord extends WebRtcAudioRecord {
    private final AtomicReference<ByteBuffer> cachedBuffer = new AtomicReference<>();
    private final CountDownLatch joinStarted = new CountDownLatch(1);

    GenerationWebRtcAudioRecord(
        Context context, AudioManager audioManager, AudioBufferCallback audioBufferCallback) {
      super(
          context,
          WebRtcAudioRecord.newDefaultScheduler(),
          audioManager,
          AudioSource.VOICE_COMMUNICATION,
          AudioFormat.ENCODING_PCM_16BIT,
          null,
          null,
          null,
          audioBufferCallback,
          false,
          false,
          48000,
          1);
    }

    @Override
    void cacheDirectBufferAddress(ByteBuffer byteBuffer) {
      cachedBuffer.set(byteBuffer);
    }

    @Override
    boolean joinAudioRecordThread(Thread thread) {
      joinStarted.countDown();
      return super.joinAudioRecordThread(thread);
    }
  }

  private static final class BlockingAudioBufferCallback implements AudioBufferCallback {
    private final AtomicInteger bufferCount = new AtomicInteger();
    private final AtomicReference<ByteBuffer> firstBuffer = new AtomicReference<>();
    private final CountDownLatch firstBufferReady = new CountDownLatch(1);
    private final CountDownLatch continueFirstBuffer = new CountDownLatch(1);
    private final CountDownLatch secondBufferReady = new CountDownLatch(1);

    @Override
    public long onBuffer(
        ByteBuffer buffer,
        int audioFormat,
        int channelCount,
        int sampleRate,
        int bytesRead,
        long captureTimeNs) {
      int count = bufferCount.incrementAndGet();
      if (count == 1) {
        firstBuffer.set(buffer);
        firstBufferReady.countDown();
        try {
          continueFirstBuffer.await(5, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
          Thread.currentThread().interrupt();
        }
      } else if (count == 2) {
        secondBufferReady.countDown();
      }
      return captureTimeNs;
    }
  }

  private static final class RestartOnStopCallback implements AudioRecordStateCallback {
    private WebRtcAudioRecord audioRecord;
    private boolean shouldRestart = true;
    private boolean restartResult;

    @Override
    public void onWebRtcAudioRecordStart() {}

    @Override
    public void onWebRtcAudioRecordStop() {
      if (shouldRestart) {
        shouldRestart = false;
        restartResult = audioRecord.startRecordingIfNeeded();
      }
    }
  }
}

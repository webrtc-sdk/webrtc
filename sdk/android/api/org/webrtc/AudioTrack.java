/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import androidx.annotation.Nullable;
import java.util.IdentityHashMap;
import org.webrtc.audio.AudioProcessingOptions;
import org.webrtc.audio.AudioProcessingOptionsResult;
import org.webrtc.audio.JavaAudioDeviceModule;

/** Java wrapper for a C++ AudioTrackInterface */
public class AudioTrack extends MediaStreamTrack {
  private final IdentityHashMap<AudioTrackSink, Long> sinks = new IdentityHashMap<AudioTrackSink, Long>();
  @Nullable private final AudioProcessingPlatformPolicy audioProcessingPlatformPolicy;

  static final class AudioProcessingPlatformPolicy {
    final boolean isEchoCancellationPlatformAvailable;
    final boolean isNoiseSuppressionPlatformAvailable;

    AudioProcessingPlatformPolicy(
        boolean isEchoCancellationPlatformAvailable, boolean isNoiseSuppressionPlatformAvailable) {
      this.isEchoCancellationPlatformAvailable = isEchoCancellationPlatformAvailable;
      this.isNoiseSuppressionPlatformAvailable = isNoiseSuppressionPlatformAvailable;
    }

    static AudioProcessingPlatformPolicy fromJavaAudioDeviceModule(
        JavaAudioDeviceModule audioDeviceModule) {
      JavaAudioDeviceModule.PlatformAudioProcessingState state =
          audioDeviceModule.getPlatformAudioProcessingState();
      return new AudioProcessingPlatformPolicy(
          state.echoCancellation.isAvailable, state.noiseSuppression.isAvailable);
    }
  }

  public AudioTrack(long nativeTrack) {
    this(nativeTrack, null);
  }

  AudioTrack(long nativeTrack, @Nullable AudioProcessingPlatformPolicy audioProcessingPlatformPolicy) {
    super(nativeTrack);
    this.audioProcessingPlatformPolicy = audioProcessingPlatformPolicy;
  }

  /** Sets the volume for the underlying MediaSource. Volume is a gain value in the range
   *  0 to 10.
   */
  public void setVolume(double volume) {
    nativeSetVolume(getNativeAudioTrack(), volume);
  }

  /** Gets the volume for the underlying MediaSource. Volume is a gain value in the range
   *  0 to 10.
   */
  public double getVolume() {
    return nativeGetVolume(getNativeAudioTrack());
  }

  /**
   * Updates the audio processing options stored on this local audio track's source.
   *
   * <p>This does not restart capture or change Android AudioRecord configuration. If the track is
   * already being sent, active senders observe the track update and reapply the updated options.
   * The effective audio processing module configuration is shared by the voice engine/channel, so
   * conflicting updates from multiple local tracks are not isolated per track.
   *
   * @return result code describing whether the request was accepted or why it was rejected. {@code
   *     STORED} means the request was accepted and stored. Rejections mean the options were not
   *     stored.
   */
  public AudioProcessingOptionsResult setAudioProcessingOptions(AudioProcessingOptions options) {
    if (options == null) {
      throw new IllegalArgumentException("AudioProcessingOptions is not allowed to be null");
    }
    // Factory-created local tracks carry the Java ADM policy, which can be
    // stricter than static device support when hardware AEC/NS was disabled by
    // the ADM builder. Tracks without that context fall back to device support.
    boolean isEchoCancellationPlatformAvailable = audioProcessingPlatformPolicy == null
        ? JavaAudioDeviceModule.isBuiltInAcousticEchoCancelerSupported()
        : audioProcessingPlatformPolicy.isEchoCancellationPlatformAvailable;
    boolean isNoiseSuppressionPlatformAvailable = audioProcessingPlatformPolicy == null
        ? JavaAudioDeviceModule.isBuiltInNoiseSuppressorSupported()
        : audioProcessingPlatformPolicy.isNoiseSuppressionPlatformAvailable;
    return AudioProcessingOptionsResult.fromNativeResult(nativeSetAudioProcessingOptions(
        getNativeAudioTrack(), options.echoCancellation,
        options.noiseSuppression, options.autoGainControl, options.highPassFilter,
        isEchoCancellationPlatformAvailable, isNoiseSuppressionPlatformAvailable,
        options.echoCancellationMode.ordinal(), options.noiseSuppressionMode.ordinal(),
        options.autoGainControlMode.ordinal(), options.highPassFilterMode.ordinal()));
  }

  /**
   * Adds an AudioTrackSink to the track. This callback is only
   * called for remote audio tracks.
   * 
   * Repeated addSink calls will not add the sink multiple times.
   */
  public void addSink(AudioTrackSink sink) {
    if (sink == null) {
      throw new IllegalArgumentException("The AudioTrackSink is not allowed to be null");
    }
    if (!sinks.containsKey(sink)) {
      final long nativeSink = nativeWrapSink(sink);
      sinks.put(sink, nativeSink);
      nativeAddSink(getNativeMediaStreamTrack(), nativeSink);
    }
  }

  /**
   * Removes an AudioTrackSink from the track.
   *
   * If the AudioTrackSink was not attached to the track, this is a no-op.
   */
  public void removeSink(AudioTrackSink sink) {
    final Long nativeSink = sinks.remove(sink);
    if (nativeSink != null) {
      nativeRemoveSink(getNativeMediaStreamTrack(), nativeSink);
      nativeFreeSink(nativeSink);
    }
  }

  @Override
  public void dispose() {
    for (long nativeSink : sinks.values()) {
      nativeRemoveSink(getNativeMediaStreamTrack(), nativeSink);
      nativeFreeSink(nativeSink);
    }
    sinks.clear();
    super.dispose();
  }

  /** Returns a pointer to webrtc::AudioTrackInterface. */
  long getNativeAudioTrack() {
    return getNativeMediaStreamTrack();
  }

  private static native void nativeSetVolume(long track, double volume);
  private static native double nativeGetVolume(long track);
  private static native String nativeSetAudioProcessingOptions(long track, boolean echoCancellation,
      boolean noiseSuppression, boolean autoGainControl, boolean highPassFilter,
      boolean isEchoCancellationPlatformAvailable, boolean isNoiseSuppressionPlatformAvailable,
      int echoCancellationMode, int noiseSuppressionMode, int autoGainControlMode,
      int highPassFilterMode);
  private static native void nativeAddSink(long track, long nativeSink);
  private static native void nativeRemoveSink(long track, long nativeSink);
  private static native long nativeWrapSink(AudioTrackSink sink);
  private static native void nativeFreeSink(long sink);
}

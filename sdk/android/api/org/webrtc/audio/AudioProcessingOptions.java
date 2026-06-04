/*
 *  Copyright 2026 LiveKit. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

package org.webrtc.audio;

/** Audio processing options for native local audio capture and tracks. */
public final class AudioProcessingOptions {
  public final boolean echoCancellation;
  public final boolean noiseSuppression;
  public final boolean autoGainControl;
  public final boolean highPassFilter;
  public final AudioProcessingMode echoCancellationMode;
  public final AudioProcessingMode noiseSuppressionMode;
  public final AudioProcessingMode autoGainControlMode;
  public final AudioProcessingMode highPassFilterMode;

  public AudioProcessingOptions(boolean echoCancellation, boolean noiseSuppression,
      boolean autoGainControl, boolean highPassFilter) {
    this(echoCancellation, noiseSuppression, autoGainControl, highPassFilter,
        AudioProcessingMode.AUTOMATIC, AudioProcessingMode.AUTOMATIC,
        AudioProcessingMode.AUTOMATIC, AudioProcessingMode.AUTOMATIC);
  }

  public AudioProcessingOptions(boolean echoCancellation, boolean noiseSuppression,
      boolean autoGainControl, boolean highPassFilter, AudioProcessingMode echoCancellationMode,
      AudioProcessingMode noiseSuppressionMode, AudioProcessingMode autoGainControlMode,
      AudioProcessingMode highPassFilterMode) {
    this.echoCancellation = echoCancellation;
    this.noiseSuppression = noiseSuppression;
    this.autoGainControl = autoGainControl;
    this.highPassFilter = highPassFilter;
    this.echoCancellationMode = checkNotNull(echoCancellationMode, "echoCancellationMode");
    this.noiseSuppressionMode = checkNotNull(noiseSuppressionMode, "noiseSuppressionMode");
    this.autoGainControlMode = checkNotNull(autoGainControlMode, "autoGainControlMode");
    this.highPassFilterMode = checkNotNull(highPassFilterMode, "highPassFilterMode");
  }

  public static AudioProcessingOptions communication() {
    return new AudioProcessingOptions(true, true, true, true);
  }

  public static AudioProcessingOptions raw() {
    return new AudioProcessingOptions(false, false, false, false);
  }

  private static <T> T checkNotNull(T value, String name) {
    if (value == null) {
      throw new IllegalArgumentException(name + " is not allowed to be null");
    }
    return value;
  }
}

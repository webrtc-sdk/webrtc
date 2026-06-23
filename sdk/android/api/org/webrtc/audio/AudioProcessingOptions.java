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
    this(new AudioProcessingComponentOptions(
             echoCancellation, AudioProcessingMode.AUTOMATIC),
        new AudioProcessingComponentOptions(
            noiseSuppression, AudioProcessingMode.AUTOMATIC),
        new AudioProcessingComponentOptions(
            autoGainControl, AudioProcessingMode.AUTOMATIC),
        new AudioProcessingComponentOptions(
            highPassFilter, AudioProcessingMode.AUTOMATIC));
  }

  public AudioProcessingOptions(AudioProcessingComponentOptions echoCancellationOptions,
      AudioProcessingComponentOptions noiseSuppressionOptions,
      AudioProcessingComponentOptions autoGainControlOptions,
      AudioProcessingComponentOptions highPassFilterOptions) {
    echoCancellationOptions = checkNotNull(echoCancellationOptions, "echoCancellationOptions");
    noiseSuppressionOptions = checkNotNull(noiseSuppressionOptions, "noiseSuppressionOptions");
    autoGainControlOptions = checkNotNull(autoGainControlOptions, "autoGainControlOptions");
    highPassFilterOptions = checkNotNull(highPassFilterOptions, "highPassFilterOptions");
    this.echoCancellation = echoCancellationOptions.isEnabled;
    this.noiseSuppression = noiseSuppressionOptions.isEnabled;
    this.autoGainControl = autoGainControlOptions.isEnabled;
    this.highPassFilter = highPassFilterOptions.isEnabled;
    this.echoCancellationMode = echoCancellationOptions.mode;
    this.noiseSuppressionMode = noiseSuppressionOptions.mode;
    this.autoGainControlMode = autoGainControlOptions.mode;
    this.highPassFilterMode = highPassFilterOptions.mode;
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

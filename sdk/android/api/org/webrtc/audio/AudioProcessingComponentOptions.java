/*
 *  Copyright 2026 LiveKit. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

package org.webrtc.audio;

/** Enabled flag and implementation mode for one audio processing component. */
public final class AudioProcessingComponentOptions {
  public final boolean enabled;
  public final AudioProcessingMode mode;

  public AudioProcessingComponentOptions(boolean enabled) {
    this(enabled, AudioProcessingMode.AUTOMATIC);
  }

  public AudioProcessingComponentOptions(boolean enabled, AudioProcessingMode mode) {
    this.enabled = enabled;
    this.mode = checkNotNull(mode, "mode");
  }

  private static <T> T checkNotNull(T value, String name) {
    if (value == null) {
      throw new IllegalArgumentException(name + " is not allowed to be null");
    }
    return value;
  }
}

/*
 *  Copyright 2026 LiveKit. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

package org.webrtc.audio;

import org.webrtc.CalledByNative;

/** Enabled flag and implementation mode for one audio processing component. */
public final class AudioProcessingComponentOptions {
  public final boolean isEnabled;
  public final AudioProcessingMode mode;

  public AudioProcessingComponentOptions(boolean isEnabled) {
    this(isEnabled, AudioProcessingMode.AUTOMATIC);
  }

  @CalledByNative
  public AudioProcessingComponentOptions(boolean isEnabled, AudioProcessingMode mode) {
    this.isEnabled = isEnabled;
    this.mode = checkNotNull(mode, "mode");
  }

  private static <T> T checkNotNull(T value, String name) {
    if (value == null) {
      throw new IllegalArgumentException(name + " is not allowed to be null");
    }
    return value;
  }
}

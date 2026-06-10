/*
 *  Copyright 2026 LiveKit. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

package org.webrtc.audio;

import org.webrtc.CalledByNative;

/**
 * Selects the implementation for one enabled audio processing component.
 *
 * <p>Disabled components do not use platform or software processing regardless of mode. Android
 * exposes platform acoustic echo cancellation and noise suppression when the device supports them.
 * Android does not expose platform automatic gain control or high-pass filter through this path
 * today, so those platform requests are rejected by {@code AudioTrack}.
 *
 * <p>Order must match {@code webrtc::AudioProcessingMode} because JNI passes ordinal values.
 */
public enum AudioProcessingMode {
  /**
   * Uses platform processing when available and otherwise falls back to WebRTC software processing.
   */
  AUTOMATIC,

  /** Uses only platform processing. Unavailable platform requests are rejected. */
  PLATFORM,

  /** Disables the matching platform effect and uses WebRTC software processing. */
  SOFTWARE;

  @CalledByNative
  static AudioProcessingMode fromNativeIndex(int nativeIndex) {
    return values()[nativeIndex];
  }
}

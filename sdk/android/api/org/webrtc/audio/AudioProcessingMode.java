/*
 *  Copyright 2026 LiveKit. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

package org.webrtc.audio;

/**
 * Selects the implementation for one enabled audio processing component.
 *
 * <p>Disabled components do not use platform or software processing regardless of mode. Some audio
 * device modules expose coupled platform effects, such as Apple Voice Processing I/O for AEC and
 * NS. High-pass filter has no platform implementation today, so platform HPF resolves to disabled.
 */
public enum AudioProcessingMode {
  /**
   * Uses platform processing when available and otherwise falls back to WebRTC software processing.
   */
  AUTOMATIC,

  /** Uses only platform processing. If platform support is unavailable, the component is disabled. */
  PLATFORM,

  /** Disables the matching platform effect and uses WebRTC software processing. */
  SOFTWARE
}

/*
 *  Copyright 2026 LiveKit. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

package org.webrtc.audio;

/** Selects whether an audio processing component uses platform or WebRTC software processing. */
public enum AudioProcessingMode {
  AUTOMATIC,
  PLATFORM,
  SOFTWARE
}

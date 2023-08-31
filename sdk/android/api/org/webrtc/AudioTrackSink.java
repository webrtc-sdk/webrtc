/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

/**
 * Java version of rtc::AudioTrackSinkInterface.
 */
public interface AudioTrackSink {
  /**
   * 
   */
  @CalledByNative 
  void onData(const void* audioData, int bitsPerSample, int sampleRate,
      int numberOfChannels, int numberOfFrames, 
      absl::optional<int64_t> absoluteCaptureTimestampMs);
}

/*
 * Copyright 2026 LiveKit, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.webrtc;

import androidx.annotation.Nullable;

/**
 * Per-frame values carried in a LiveKit packet trailer. For send, attach them to a VideoFrame. For
 * receive, read them with FrameMetadataTransformer.lookup().
 */
public class FrameMetadata {
  private final long userTimestampUs;
  private final long frameId;
  @Nullable private final byte[] userData;

  /**
   * @param userTimestampUs Sent as an unsigned 64-bit value. 0 means unset.
   * @param frameId Sent as an unsigned 32-bit value. 0 means unset.
   * @param userData Free-form bytes, or null for none. The trailer isn't encrypted, even with
   *     end-to-end encryption. It's dropped if it doesn't fit, or if it would put an H.264 start
   *     code in the trailer.
   */
  @CalledByNative
  public FrameMetadata(long userTimestampUs, long frameId, @Nullable byte[] userData) {
    this.userTimestampUs = userTimestampUs;
    this.frameId = frameId;
    this.userData = userData;
  }

  @CalledByNative
  public long getUserTimestampUs() {
    return userTimestampUs;
  }

  @CalledByNative
  public long getFrameId() {
    return frameId;
  }

  @Nullable
  @CalledByNative
  public byte[] getUserData() {
    return userData;
  }
}

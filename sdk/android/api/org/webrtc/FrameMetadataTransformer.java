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
 * Java wrapper for a C++ FrameMetadataTransformer, which writes (send) or strips (receive) packet
 * trailers on video frames. Meant for video: LiveKit receivers only strip trailers on video tracks.
 *
 * <p>Creating a transformer doesn't install it. Install it with RtpSender.setFrameTransformer() or
 * RtpReceiver.setFrameTransformer(). A send transformer only writes the values on frames from a
 * VideoSource it was added to, and generates none itself.
 */
public class FrameMetadataTransformer implements FrameTransformer {
  public enum Direction {
    SEND(0),
    RECEIVE(1);

    private final int value;

    Direction(int value) {
      this.value = value;
    }

    int getValue() {
      return value;
    }
  }

  private long nativeFrameMetadataTransformer;
  private final Direction direction;

  FrameMetadataTransformer(long nativeFrameMetadataTransformer, Direction direction) {
    this.nativeFrameMetadataTransformer = nativeFrameMetadataTransformer;
    this.direction = direction;
  }

  public Direction getDirection() {
    return direction;
  }

  /**
   * When disabled, frames pass through untouched. A disabled receive transformer doesn't strip
   * trailers, so don't disable one while frames carrying trailers can still arrive.
   */
  public void setEnabled(boolean enabled) {
    checkExists();
    nativeSetEnabled(nativeFrameMetadataTransformer, enabled);
  }

  public boolean isEnabled() {
    checkExists();
    return nativeIsEnabled(nativeFrameMetadataTransformer);
  }

  /**
   * Receive side: the metadata for the frame with this RTP timestamp, or null if it isn't cached.
   * The entry stays cached, so any number of sinks can look up the same frame.
   */
  @Nullable
  public FrameMetadata lookup(long rtpTimestamp) {
    checkExists();
    return nativeLookup(nativeFrameMetadataTransformer, rtpTimestamp);
  }

  /**
   * Releases this object's reference. An installed transformer keeps running until the native
   * objects holding it are destroyed or it's replaced.
   */
  public void dispose() {
    checkExists();
    JniCommon.nativeReleaseRef(nativeFrameMetadataTransformer);
    nativeFrameMetadataTransformer = 0;
  }

  @Override
  public long getNativeFrameTransformer() {
    checkExists();
    return nativeGetFrameTransformer(nativeFrameMetadataTransformer);
  }

  /** Returns a pointer to webrtc::FrameMetadataTransformer. */
  long getNativeFrameMetadataTransformer() {
    checkExists();
    return nativeFrameMetadataTransformer;
  }

  private void checkExists() {
    if (nativeFrameMetadataTransformer == 0) {
      throw new IllegalStateException("FrameMetadataTransformer has been disposed.");
    }
  }

  private static native void nativeSetEnabled(long frameMetadataTransformer, boolean enabled);
  private static native boolean nativeIsEnabled(long frameMetadataTransformer);
  @Nullable
  private static native FrameMetadata nativeLookup(
      long frameMetadataTransformer, long rtpTimestamp);
  private static native long nativeGetFrameTransformer(long frameMetadataTransformer);
}

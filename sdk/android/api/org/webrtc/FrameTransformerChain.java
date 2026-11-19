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

/**
 * Runs every frame through two transformers in the order given. It doesn't know what its stages
 * are, so the caller sets the order. A chain is itself a FrameTransformer, so chains can nest.
 *
 * <p>Disposing a chain doesn't dispose its stages. Each Java object is disposed separately, and the
 * native references make the order safe.
 */
public class FrameTransformerChain implements FrameTransformer {
  private long nativeFrameTransformerChain;

  public FrameTransformerChain(FrameTransformer first, FrameTransformer second) {
    if (first == null || second == null) {
      throw new IllegalArgumentException("Chain stages must not be null.");
    }
    nativeFrameTransformerChain = nativeCreateFrameTransformerChain(
        first.getNativeFrameTransformer(), second.getNativeFrameTransformer());
  }

  @Override
  public long getNativeFrameTransformer() {
    checkExists();
    return nativeFrameTransformerChain;
  }

  /**
   * Releases this object's reference. An installed chain keeps running until the native objects
   * holding it are destroyed or it's replaced.
   */
  public void dispose() {
    checkExists();
    JniCommon.nativeReleaseRef(nativeFrameTransformerChain);
    nativeFrameTransformerChain = 0;
  }

  private void checkExists() {
    if (nativeFrameTransformerChain == 0) {
      throw new IllegalStateException("FrameTransformerChain has been disposed.");
    }
  }

  private static native long nativeCreateFrameTransformerChain(long first, long second);
}

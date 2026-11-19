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

public class FrameMetadataTransformerFactory {
  /** Creates a transformer without installing it. */
  public static FrameMetadataTransformer createFrameMetadataTransformer(
      FrameMetadataTransformer.Direction direction) {
    return new FrameMetadataTransformer(
        nativeCreateFrameMetadataTransformer(direction.getValue()), direction);
  }

  private static native long nativeCreateFrameMetadataTransformer(int direction);
}

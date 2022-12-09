/*
 * Copyright 2022 LiveKit
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

public class FrameCryptorFactory {
  public static FrameCryptor createFrameCryptorForRtpSender(
      RtpSender rtpSender, FrameCryptorAlgorithm algorithm, FrameCryptorKeyManager keyManager) {
    long nativeFrameCryptor = nativeCreateFrameCryptorForRtpSender(rtpSender.getNativeRtpSender(),
        algorithm.ordinal(), keyManager.getNativeKeyManager());
    return new FrameCryptor(nativeFrameCryptor);
  }

  public static FrameCryptor createFrameCryptorForRtpReceiver(
      RtpReceiver rtpReceiver, FrameCryptorAlgorithm algorithm, FrameCryptorKeyManager keyManager) {
    long nativeFrameCryptor =
        nativeCreateFrameCryptorForRtpReceiver(rtpReceiver.getNativeRtpReceiver(),
            algorithm.ordinal(), keyManager.getNativeKeyManager());
    return new FrameCryptor(nativeFrameCryptor);
  }

  private static native long nativeCreateFrameCryptorForRtpSender(
      long nativeRtpSender, int algorithm, long nativeFrameCryptorKeyManager);
  private static native long nativeCreateFrameCryptorForRtpReceiver(
      long nativeRtpReceiver, int algorithm, long nativeFrameCryptorKeyManager);
}

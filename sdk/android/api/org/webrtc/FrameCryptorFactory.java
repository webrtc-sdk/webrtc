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
  public static FrameCryptorKeyManager createFrameCryptorKeyManager() {
    return nativeCreateFrameCryptorKeyManager();
  }

  public static FrameCryptor createFrameCryptorForRtpSender(RtpSender rtpSender,
      String participantId, FrameCryptorAlgorithm algorithm, FrameCryptorKeyManager keyManager) {
    return nativeCreateFrameCryptorForRtpSender(rtpSender.getNativeRtpSender(), participantId,
        algorithm.ordinal(), keyManager.getNativeKeyManager());
  }

  public static FrameCryptor createFrameCryptorForRtpReceiver(RtpReceiver rtpReceiver,
      String participantId, FrameCryptorAlgorithm algorithm, FrameCryptorKeyManager keyManager) {
    return nativeCreateFrameCryptorForRtpReceiver(rtpReceiver.getNativeRtpReceiver(), participantId,
        algorithm.ordinal(), keyManager.getNativeKeyManager());
  }

  private static native FrameCryptor nativeCreateFrameCryptorForRtpSender(
      long rtpSender, String participantId, int algorithm, long nativeFrameCryptorKeyManager);
  private static native FrameCryptor nativeCreateFrameCryptorForRtpReceiver(
      long rtpReceiver, String participantId, int algorithm, long nativeFrameCryptorKeyManager);

  private static native FrameCryptorKeyManager nativeCreateFrameCryptorKeyManager();
}

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

import java.util.ArrayList;

public class FrameCryptorKeyManager {
  private long nativeKeyManager;

  @CalledByNative
  public FrameCryptorKeyManager(long nativeKeyManager) {
    this.nativeKeyManager = nativeKeyManager;
  }

  public long getNativeKeyManager() {
    return nativeKeyManager;
  }

  public boolean setKey(String participantId, int index, byte[] key) {
    checkKeyManagerExists();
    return nativeSetKey(nativeKeyManager, participantId, index, key);
  }

  public byte[] ratchetKey(String participantId, int index) {
    checkKeyManagerExists();
    return nativeRatchetKey(nativeKeyManager, participantId, index);
  }

  public byte[] exportKey(String participantId, int index) {
    checkKeyManagerExists();
    return nativeExportKey(nativeKeyManager, participantId, index);
  }

  public void dispose() {
    checkKeyManagerExists();
    JniCommon.nativeReleaseRef(nativeKeyManager);
    nativeKeyManager = 0;
  }

  private void checkKeyManagerExists() {
    if (nativeKeyManager == 0) {
      throw new IllegalStateException("FrameCryptorKeyManager has been disposed.");
    }
  }

  private static native boolean nativeSetKey(
      long keyManagerPointer, String participantId, int index, byte[] key);
  private static native byte[] nativeRatchetKey(
      long keyManagerPointer, String participantId, int index);
  private static native byte[] nativeExportKey(
      long keyManagerPointer, String participantId, int index);
}
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

  public FrameCryptorKeyManager() {
    this.nativeKeyManager = createNativeKeyManager();
  }

  public long getNativeKeyManager() {
    return nativeKeyManager;
  }

  public boolean setKey(int index, byte[] key){
    return nativeSetKey(index, key);
  }

  public boolean setKeys(ArrayList<byte[]> keys) {
    return nativeSetKeys(keys);
  }

  public ArrayList<byte[]> getKeys() {
    return nativeGetKeys();
  }

  public void dispose() {
    if (nativeKeyManager != 0) {
      nativeDispose(nativeKeyManager);
      nativeKeyManager = 0;
    }
  }

  private static native boolean nativeSetKey(int index, byte[] key);
  private static native boolean nativeSetKeys(ArrayList<byte[]> keys);
  private static native ArrayList<byte[]> nativeGetKeys();
  private static native long createNativeKeyManager();
  private static native void nativeDispose(long nativeKeyManager);
}
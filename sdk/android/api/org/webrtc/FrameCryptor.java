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

import androidx.annotation.Nullable;

public class FrameCryptor {
  private long nativeFrameCryptor;

  public long getNativeFrameCryptor() {
    return nativeFrameCryptor;
  }

  @CalledByNative
  public FrameCryptor(long nativeFrameCryptor) {
    this.nativeFrameCryptor = nativeFrameCryptor;
  }

  public void setEnabled(boolean enabled) {
    checkFrameCryptorExists();
    nativeSetEnabled(nativeFrameCryptor, enabled);
  }

  public boolean isEnabled() {
    checkFrameCryptorExists();
    return nativeIsEnabled(nativeFrameCryptor);
  }

  public int getKeyIndex() {
    checkFrameCryptorExists();
    return nativeGetKeyIndex(nativeFrameCryptor);
  }

  public void setKeyIndex(int index) {
    checkFrameCryptorExists();
    nativeSetKeyIndex(nativeFrameCryptor, index);
  }

  public void dispose() {
    checkFrameCryptorExists();
    JniCommon.nativeReleaseRef(nativeFrameCryptor);
    nativeFrameCryptor = 0;
  }

  private void checkFrameCryptorExists() {
    if (nativeFrameCryptor == 0) {
      throw new IllegalStateException("FrameCryptor has been disposed.");
    }
  }

  private static native void nativeSetEnabled(long frameCryptorPointer, boolean enabled);
  private static native boolean nativeIsEnabled(long frameCryptorPointer);
  private static native void nativeSetKeyIndex(long frameCryptorPointer, int index);
  private static native int nativeGetKeyIndex(long frameCryptorPointer);
}

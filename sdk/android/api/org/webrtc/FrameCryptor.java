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

public class FrameCryptor {
  private long nativeFrameCryptor;

  public long getNativeFrameCryptor() {
    return nativeFrameCryptor;
  }

  public FrameCryptor(long nativeFrameCryptor) {
    this.nativeFrameCryptor = nativeFrameCryptor;
  }

  public void setEnabled(boolean enabled) {
    nativeSetEnabled(enabled);
  }

  public boolean isEnabled() {
    return nativeIsEnabled();
  }

  public void setKeyIndex(int index) {
    nativeSetKeyIndex(index);
  }

  public int getKeyIndex() {
    return nativeGetKeyIndex();
  }

  public void dispose() {
    if (nativeFrameCryptor != 0) {
      nativeDispose(nativeFrameCryptor);
      nativeFrameCryptor = 0;
    }
  }

  private static native void nativeSetEnabled(boolean enabled);
  private static native boolean nativeIsEnabled();
  private static native void nativeSetKeyIndex(int index);
  private static native int nativeGetKeyIndex();
  private static native void nativeDispose(long nativeFrameCryptor);
}

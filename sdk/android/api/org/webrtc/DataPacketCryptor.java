/*
 * Copyright 2025 LiveKit
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

public class DataPacketCryptor {

    private long nativePtr;

    public long getNativeDataPacketCryptor() {
        return nativePtr;
    }

    @CalledByNative
    public DataPacketCryptor(long nativeFrameCryptor) {
        this.nativePtr = nativeFrameCryptor;
    }

    public EncryptedPacket encrypt(String participantId, int keyIndex, byte[] data) {
        checkDataPacketCryptorExists();
        return nativeEncrypt(nativePtr, participantId, keyIndex, data);
    }

    public byte[] decrypt(String participantId, EncryptedPacket packet) {
        checkDataPacketCryptorExists();
        return nativeDecrypt(nativePtr, participantId, packet.keyIndex, packet.payload, packet.iv);
    }

    public void dispose() {
        checkDataPacketCryptorExists();
        JniCommon.nativeReleaseRef(nativePtr);
        nativePtr = 0;
    }

    private void checkDataPacketCryptorExists() {
        if (nativePtr == 0) {
            throw new IllegalStateException("DataPacketCryptor has been disposed.");
        }
    }

    public static class EncryptedPacket {
        public final byte[] payload;
        public final byte[] iv;
        public final int keyIndex;

        @CalledByNative("EncryptedPacket")
        public EncryptedPacket(byte[] payload, byte[] iv, int keyIndex) {
            this.payload = payload;
            this.iv = iv;
            this.keyIndex = keyIndex;
        }
    }

    private static native EncryptedPacket nativeEncrypt(long dataCryptorPointer, String participantId, int keyIndex, byte[] data);

    private static native byte[] nativeDecrypt(long dataCryptorPointer, String participantId, int keyIndex, byte[] data, byte[] iv);
}

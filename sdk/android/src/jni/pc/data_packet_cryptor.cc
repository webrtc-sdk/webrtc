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
#include "sdk/android/src/jni/pc/frame_cryptor.h"

#include "api/rtp_receiver_interface.h"
#include "api/rtp_sender_interface.h"
#include "rtc_base/ref_counted_object.h"
#include "sdk/android/generated_peerconnection_jni/FrameCryptorFactory_jni.h"
#include "sdk/android/generated_peerconnection_jni/FrameCryptor_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/pc/frame_cryptor_key_provider.h"
#include "sdk/android/src/jni/pc/frame_cryptor.h"
#include "sdk/android/src/jni/pc/owned_factory_and_threads.h"

namespace webrtc {
namespace jni {

ScopedJavaLocalRef<jobject> NativeToJavaDataPacketCryptor(
    JNIEnv* env,
    rtc::scoped_refptr<DataPacketCryptor> cryptor) {
  if (!cryptor)
    return nullptr;
  // Cryptor is now owned by the Java object, and will be freed from
  // DataPacketCryptor.dispose().
  return Java_DataPacketCryptor_Constructor(env,
                                       jlongFromPointer(cryptor.release()));
}

static void JNI_DataPacketCryptor_Encrypt(
    JNIEnv* jni,
    jlong j_data_cryptor_pointer,
    jstring j_participant_id,
    int key_index,
    const jni_zero::JavaParamRef<jbyteArray>& j_data) {
  std::string participant_id =
      JavaToNativeString(jni, jni_zero::JavaParamRef<jstring>(jni, j_participant_id));
  std::vector<int8_t> data = JavaToNativeByteArray(jni, j_data);

  RTCErrorOr<scoped_refptr<EncryptedPacket>> result =
      reinterpret_cast<DataPacketCryptor*>(j_data_cryptor_pointer)
          ->Encrypt(participant_id, key_index, j_data);
  if (!result.ok()) {
    RTC_LOG(LS_ERROR) << "Failed to encrypt payload: " << result.error().message();
    return nullptr;
  } else {
    auto packet = result.value();
    std::vector<int8_t> int8tData =
        std::vector<int8_t>(packet.data.begin(), packet.data.end());
    std::vector<int8_t> int8tIv =
        std::vector<int8_t>(packet.iv.begin(), packet.iv.end());
    ScopedJavaLocalRef<jbyteArray> j_data = NativeToJavaByteArray(env, rtc::ArrayView<int8_t>(int8tData));
    ScopedJavaLocalRef<jbyteArray> j_iv = NativeToJavaByteArray(env, rtc::ArrayView<int8_t>(int8tIv));
    return Java_EncryptedPacket_Constructor(env, j_data, j_iv, packet.key_index);;
  }
}

static void JNI_DataPacketCryptor_Decrypt(
    JNIEnv* jni,
    jlong j_data_cryptor_pointer,
    jstring j_participant_id,
    int key_index,
    const jni_zero::JavaParamRef<jbyteArray>& j_data) {
  std::string participant_id =
      JavaToNativeString(jni, jni_zero::JavaParamRef<jstring>(jni, j_participant_id));
  std::vector<int8_t> data = JavaToNativeByteArray(jni, j_data);

  RTCErrorOr<scoped_refptr<EncryptedPacket>> result =
      reinterpret_cast<DataPacketCryptor*>(j_data_cryptor_pointer)
          ->Decrypt(participant_id, key_index, j_data);
  if (!result.ok()) {
    RTC_LOG(LS_ERROR) << "Failed to encrypt payload: " << result.error().message();
    return nullptr;
  } else {
    auto decryptedData = result.value();
    std::vector<int8_t> int8tDecryptedData =
        std::vector<int8_t>(decryptedData.begin(), decryptedData.end());
    return NativeToJavaByteArray(env, rtc::ArrayView<int8_t>(int8tDecryptedData));
  }
}

}  // namespace jni
}  // namespace webrtc

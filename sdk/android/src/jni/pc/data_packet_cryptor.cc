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
#include "sdk/android/src/jni/pc/data_packet_cryptor.h"

#include "rtc_base/ref_counted_object.h"
#include "sdk/android/generated_peerconnection_jni/DataPacketCryptorFactory_jni.h"
#include "sdk/android/generated_peerconnection_jni/DataPacketCryptor_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/pc/frame_cryptor.h"
#include "sdk/android/src/jni/pc/frame_cryptor_key_provider.h"

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

static jni_zero::ScopedJavaLocalRef<jobject> JNI_DataPacketCryptor_Encrypt(
    JNIEnv* env,
    jlong j_data_cryptor_pointer,
    const jni_zero::JavaParamRef<jstring>& j_participant_id,
    int key_index,
    const jni_zero::JavaParamRef<jbyteArray>& j_data) {
  auto participant_id =
      JavaToNativeString(env, jni_zero::JavaParamRef<jstring>(env, j_participant_id));
  auto data = JavaToNativeByteArray(env, j_data);

  RTCErrorOr<scoped_refptr<EncryptedPacket>> result =
      reinterpret_cast<DataPacketCryptor*>(j_data_cryptor_pointer)
          ->Encrypt(participant_id, key_index, std::vector<uint8_t>(data.begin(), data.end()));
  if (!result.ok()) {
    RTC_LOG(LS_ERROR) << "Failed to encrypt payload: " << result.error().message();
    return nullptr;
  } else {
    auto packet = result.value();
    auto int8tData =
        std::vector<int8_t>(packet->data.begin(), packet->data.end());
    auto int8tIv =
        std::vector<int8_t>(packet->iv.begin(), packet->iv.end());
    auto j_data = NativeToJavaByteArray(env, rtc::ArrayView<int8_t>(int8tData));
    auto j_iv = NativeToJavaByteArray(env, rtc::ArrayView<int8_t>(int8tIv));
    return Java_EncryptedPacket_Constructor(env, j_data, j_iv, packet->key_index);;
  }
}

static jni_zero::ScopedJavaLocalRef<jbyteArray> JNI_DataPacketCryptor_Decrypt(
    JNIEnv* env,
    jlong j_data_cryptor_pointer,
    const jni_zero::JavaParamRef<jstring>& j_participant_id,
    int key_index,
    const jni_zero::JavaParamRef<jbyteArray>& j_data,
    const jni_zero::JavaParamRef<jbyteArray>& j_iv) {
  auto participant_id =
      JavaToNativeString(env, jni_zero::JavaParamRef<jstring>(env, j_participant_id));
  auto data = JavaToNativeByteArray(env, j_data);
  auto iv = JavaToNativeByteArray(env, j_iv);

  auto encrypted_packet = webrtc::make_ref_counted<EncryptedPacket>(
      std::vector<uint8_t>(data.begin(), data.end()),
      std::vector<uint8_t>(iv.begin(), iv.end()),
      key_index);

  auto result =
      reinterpret_cast<DataPacketCryptor*>(j_data_cryptor_pointer)
          ->Decrypt(participant_id, encrypted_packet);
  if (!result.ok()) {
    RTC_LOG(LS_ERROR) << "Failed to decrypt payload: " << result.error().message();
    return nullptr;
  } else {
    auto decryptedData = result.value();
    std::vector<int8_t> int8tDecryptedData =
        std::vector<int8_t>(decryptedData.begin(), decryptedData.end());
    return NativeToJavaByteArray(env, rtc::ArrayView<int8_t>(int8tDecryptedData));
  }
}

static ScopedJavaLocalRef<jobject>
JNI_DataPacketCryptorFactory_CreateDataPacketCryptor(
    JNIEnv* env,
    jint j_algorithm_index,
    jlong j_key_provider) {
  auto keyProvider =
      reinterpret_cast<webrtc::DefaultKeyProviderImpl*>(j_key_provider);
  auto data_packet_cryptor = make_ref_counted<DataPacketCryptor>(
      AlgorithmFromIndex(j_algorithm_index),
      rtc::scoped_refptr<webrtc::KeyProvider>(keyProvider));

  return NativeToJavaDataPacketCryptor(env, data_packet_cryptor);
}

}  // namespace jni
}  // namespace webrtc

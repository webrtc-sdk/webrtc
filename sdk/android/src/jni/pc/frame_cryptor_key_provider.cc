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
#include "sdk/android/src/jni/pc/frame_cryptor_key_provider.h"

#include "sdk/android/generated_peerconnection_jni/FrameCryptorKeyProvider_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

ScopedJavaLocalRef<jobject> NativeToJavaFrameCryptorKeyProvider(
    JNIEnv* env,
    rtc::scoped_refptr<webrtc::DefaultKeyProviderImpl> key_provider) {
  if (!key_provider)
    return nullptr;
  // Sender is now owned by the Java object, and will be freed from
  // FrameCryptorKeyProvider.dispose().
  return Java_FrameCryptorKeyProvider_Constructor(
      env, jlongFromPointer(key_provider.release()));
}

static jboolean JNI_FrameCryptorKeyProvider_SetKey(
    JNIEnv* jni,
    jlong j_key_provider,
    const base::android::JavaParamRef<jstring>& participantId,
    jint j_index,
    const base::android::JavaParamRef<jbyteArray>& j_key) {
  auto key = JavaToNativeByteArray(jni, j_key);
  auto participant_id = JavaToStdString(jni, participantId);
  return reinterpret_cast<webrtc::DefaultKeyProviderImpl*>(j_key_provider)
      ->SetKey(participant_id, j_index,
               std::vector<uint8_t>(key.begin(), key.end()));
}

static base::android::ScopedJavaLocalRef<jbyteArray>
JNI_FrameCryptorKeyProvider_RatchetKey(
    JNIEnv* env,
    jlong keyProviderPointer,
    const base::android::JavaParamRef<jstring>& participantId,
    jint j_index) {
  auto participant_id = JavaToStdString(env, participantId);
  auto key_provider =
      reinterpret_cast<webrtc::DefaultKeyProviderImpl*>(keyProviderPointer);
  auto newKey = key_provider->RatchetKey(participant_id, j_index);
  std::vector<int8_t> int8tKey =
      std::vector<int8_t>(newKey.begin(), newKey.end());
  return NativeToJavaByteArray(env, rtc::ArrayView<int8_t>(int8tKey));
}

static base::android::ScopedJavaLocalRef<jbyteArray>
JNI_FrameCryptorKeyProvider_ExportKey(
    JNIEnv* env,
    jlong keyProviderPointer,
    const base::android::JavaParamRef<jstring>& participantId,
    jint j_index) {
  auto participant_id = JavaToStdString(env, participantId);
  auto key_provider =
      reinterpret_cast<webrtc::DefaultKeyProviderImpl*>(keyProviderPointer);
  auto key = key_provider->ExportKey(participant_id, j_index);
  std::vector<int8_t> int8tKey = std::vector<int8_t>(key.begin(), key.end());
  return NativeToJavaByteArray(env, rtc::ArrayView<int8_t>(int8tKey));
}

}  // namespace jni
}  // namespace webrtc

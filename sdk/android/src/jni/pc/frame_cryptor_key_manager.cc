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
#include "sdk/android/src/jni/pc/frame_cryptor_key_manager.h"

#include "sdk/android/generated_peerconnection_jni/FrameCryptorKeyManager_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

ScopedJavaLocalRef<jobject> NativeToJavaFrameCryptorKeyManager(
    JNIEnv* env,
    rtc::scoped_refptr<DefaultKeyManagerImpl> key_manager) {
  if (!key_manager)
    return nullptr;
  // Sender is now owned by the Java object, and will be freed from
  // FrameCryptorKeyManager.dispose().
  return Java_FrameCryptorKeyManager_Constructor(
      env, jlongFromPointer(key_manager.release()));
}

jboolean JNI_FrameCryptorKeyManager_SetKey(
    JNIEnv* jni,
    jlong j_key_manager,
    jint j_index,
    const base::android::JavaParamRef<jbyteArray>& j_key) {
  auto key = JavaToNativeByteArray(jni, j_key);
  return reinterpret_cast<DefaultKeyManagerImpl*>(j_key_manager)
      ->SetKey(j_index, key);
}

static jint JNI_FrameCryptorKeyManager_GetKeyCount(JNIEnv* jni,
                                                   jlong j_key_manager) {
  return reinterpret_cast<DefaultKeyManagerImpl*>(j_key_manager)->KeyCount();
}

static ScopedJavaLocalRef<jbyteArray> JNI_FrameCryptorKeyManager_GetKey(
    JNIEnv* jni,
    jlong j_key_manager,
    jint j_index) {
  auto uint8Key = reinterpret_cast<DefaultKeyManagerImpl*>(j_key_manager)->GetKey(j_index);
  std::vector<int8_t> int8tKey = std::vector<int8_t>(uint8Key.begin(), uint8Key.end());
  return NativeToJavaByteArray(jni, rtc::ArrayView<int8_t>(int8tKey));
}

}  // namespace jni
}  // namespace webrtc

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
    rtc::scoped_refptr<KeyManager> key_manager) {
  if (!key_manager)
    return nullptr;
  // Sender is now owned by the Java object, and will be freed from
  // FrameCryptorKeyManager.dispose().
  return Java_FrameCryptorKeyManager_Constructor(
      env, jlongFromPointer(key_manager.release()));
}

jboolean JNI_FrameCryptorKeyManager_SetKey(
    JNIEnv* jni,
    jlong keyManagerPointer,
    jint index,
    const base::android::JavaParamRef<jbyteArray>& j_key) {
  auto key = JavaToNativeByteArray(jni, j_key);
  return true;
}

static jint JNI_FrameCryptorKeyManager_GetKeyCount(JNIEnv* jni,
                                                   jlong j_key_manager) {
  return 0;
}

static ScopedJavaLocalRef<jbyteArray> JNI_FrameCryptorKeyManager_GetKey(
    JNIEnv* jni,
    jlong j_key_manager,
    jint j_index) {
  std::vector<int8_t> key = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                             0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
  rtc::ArrayView<int8_t> key_view(key);
  return NativeToJavaByteArray(jni, key_view);
}

}  // namespace jni
}  // namespace webrtc

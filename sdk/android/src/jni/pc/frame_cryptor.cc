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

#include "sdk/android/generated_peerconnection_jni/FrameCryptor_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"

namespace webrtc {
namespace jni {

ScopedJavaLocalRef<jobject> NativeToJavaFrameCryptor(
    JNIEnv* env,
    rtc::scoped_refptr<FrameCryptorTransformer> cryptor) {
  if (!cryptor)
    return nullptr;
  // Sender is now owned by the Java object, and will be freed from
  // FrameCryptor.dispose().
  return Java_FrameCryptor_Constructor(env,
                                       jlongFromPointer(cryptor.release()));
}

static void JNI_FrameCryptor_SetEnabled(JNIEnv* jni,
                                        jlong j_frame_cryptor_pointer,
                                        jboolean j_enabled) {
  reinterpret_cast<FrameCryptorTransformer*>(j_frame_cryptor_pointer)
      ->SetEnabled(j_enabled);
}

static jboolean JNI_FrameCryptor_IsEnabled(JNIEnv* jni,
                                           jlong j_frame_cryptor_pointer) {
  return reinterpret_cast<FrameCryptorTransformer*>(j_frame_cryptor_pointer)
      ->enabled();
}

static void JNI_FrameCryptor_SetKeyIndex(JNIEnv* jni,
                                        jlong j_frame_cryptor_pointer,
                                        jint j_index) {
  reinterpret_cast<FrameCryptorTransformer*>(j_frame_cryptor_pointer)
      ->SetKeyIndex(j_index);
}

static jint JNI_FrameCryptor_GetKeyIndex(JNIEnv* jni,
                                           jlong j_frame_cryptor_pointer) {
  return reinterpret_cast<FrameCryptorTransformer*>(j_frame_cryptor_pointer)
      ->key_index();
}

}  // namespace jni
}  // namespace webrtc

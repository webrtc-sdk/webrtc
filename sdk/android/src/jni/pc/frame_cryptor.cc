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
#include "sdk/android/src/jni/pc/frame_cryptor_key_manager.h"

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

webrtc::FrameCryptorTransformer::Algorithm AlgorithmFromIndex(int index) {
  switch (index) {
    case 0:
      return webrtc::FrameCryptorTransformer::Algorithm::kAesGcm;
    case 1:
      return webrtc::FrameCryptorTransformer::Algorithm::kAesCbc;
    default:
      return webrtc::FrameCryptorTransformer::Algorithm::kAesGcm;
  }
}

static base::android::ScopedJavaLocalRef<jobject>
JNI_FrameCryptorFactory_CreateFrameCryptorForRtpReceiver(
    JNIEnv* env,
    jlong j_rtp_receiver_pointer,
    jint j_algorithm_index,
    jlong j_key_manager) {
  auto keyManager = reinterpret_cast<DefaultKeyManagerImpl*>(j_key_manager);
  auto rtpReceiver =
      reinterpret_cast<RtpReceiverInterface*>(j_rtp_receiver_pointer);
  auto mediaType =
      rtpReceiver->track()->kind() == "audio"
          ? webrtc::FrameCryptorTransformer::MediaType::kAudioFrame
          : webrtc::FrameCryptorTransformer::MediaType::kVideoFrame;
  auto frame_crypto_transformer =
      rtc::scoped_refptr<webrtc::FrameCryptorTransformer>(
          new webrtc::FrameCryptorTransformer(
              mediaType, AlgorithmFromIndex(j_algorithm_index),
              rtc::scoped_refptr<webrtc::KeyManager>(keyManager)));

  rtpReceiver->SetDepacketizerToDecoderFrameTransformer(frame_crypto_transformer);
  frame_crypto_transformer->SetEnabled(false);

  return NativeToJavaFrameCryptor(env, frame_crypto_transformer);
}

static base::android::ScopedJavaLocalRef<jobject>
JNI_FrameCryptorFactory_CreateFrameCryptorForRtpSender(
    JNIEnv* env,
    jlong j_rtp_sender_pointer,
    jint j_algorithm_index,
    jlong j_key_manager) {
  auto keyManager = reinterpret_cast<DefaultKeyManagerImpl*>(j_key_manager);
  auto rtpSender = reinterpret_cast<RtpSenderInterface*>(j_rtp_sender_pointer);
  auto mediaType =
      rtpSender->track()->kind() == "audio"
          ? webrtc::FrameCryptorTransformer::MediaType::kAudioFrame
          : webrtc::FrameCryptorTransformer::MediaType::kVideoFrame;
  auto frame_crypto_transformer =
      rtc::scoped_refptr<webrtc::FrameCryptorTransformer>(
          new webrtc::FrameCryptorTransformer(
              mediaType, AlgorithmFromIndex(j_algorithm_index),
              rtc::scoped_refptr<webrtc::KeyManager>(keyManager)));

  rtpSender->SetEncoderToPacketizerFrameTransformer(frame_crypto_transformer);
  frame_crypto_transformer->SetEnabled(false);

  return NativeToJavaFrameCryptor(env, frame_crypto_transformer);
}

static base::android::ScopedJavaLocalRef<jobject>
JNI_FrameCryptorFactory_CreateFrameCryptorKeyManager(JNIEnv* env) {
  return NativeToJavaFrameCryptorKeyManager(
      env, rtc::make_ref_counted<DefaultKeyManagerImpl>());
}

}  // namespace jni
}  // namespace webrtc

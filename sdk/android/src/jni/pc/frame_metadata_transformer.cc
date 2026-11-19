/*
 * Copyright 2026 LiveKit, Inc.
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

#include "api/frame_metadata/frame_metadata_transformer.h"

#include <jni.h>

#include <cstdint>
#include <optional>

#include "api/frame_transformer_interface.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "sdk/android/generated_peerconnection_jni/FrameMetadataTransformerFactory_jni.h"
#include "sdk/android/generated_peerconnection_jni/FrameMetadataTransformer_jni.h"
#include "sdk/android/generated_peerconnection_jni/FrameTransformerChain_jni.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "sdk/android/src/jni/video_frame.h"

namespace webrtc {
namespace jni {

namespace {

FrameMetadataTransformer* ToTransformer(jlong j_pointer) {
  return reinterpret_cast<FrameMetadataTransformer*>(j_pointer);
}

// The values of org.webrtc.FrameMetadataTransformer.Direction.
constexpr jint kDirectionSend = 0;

}  // namespace

static jlong JNI_FrameMetadataTransformerFactory_CreateFrameMetadataTransformer(
    JNIEnv* jni,
    jint j_direction) {
  // The Java object owns this reference and releases it in dispose().
  return jlongFromPointer(
      make_ref_counted<FrameMetadataTransformer>(
          j_direction == kDirectionSend
              ? FrameMetadataTransformer::Direction::kSend
              : FrameMetadataTransformer::Direction::kReceive)
          .release());
}

static void JNI_FrameMetadataTransformer_SetEnabled(JNIEnv* jni,
                                                    jlong j_pointer,
                                                    jboolean j_enabled) {
  ToTransformer(j_pointer)->SetEnabled(j_enabled);
}

static jboolean JNI_FrameMetadataTransformer_IsEnabled(JNIEnv* jni,
                                                       jlong j_pointer) {
  return ToTransformer(j_pointer)->enabled();
}

static ScopedJavaLocalRef<jobject> JNI_FrameMetadataTransformer_Lookup(
    JNIEnv* jni,
    jlong j_pointer,
    jlong j_rtp_timestamp) {
  const std::optional<FrameMetadata> metadata =
      ToTransformer(j_pointer)->Lookup(static_cast<uint32_t>(j_rtp_timestamp));
  if (!metadata.has_value()) {
    return nullptr;
  }
  return NativeToJavaFrameMetadata(jni, *metadata);
}

static jlong JNI_FrameMetadataTransformer_GetFrameTransformer(JNIEnv* jni,
                                                              jlong j_pointer) {
  return jlongFromPointer(
      static_cast<FrameTransformerInterface*>(ToTransformer(j_pointer)));
}

static jlong JNI_FrameTransformerChain_CreateFrameTransformerChain(
    JNIEnv* jni,
    jlong j_first,
    jlong j_second) {
  scoped_refptr<FrameTransformerInterface> chain =
      make_ref_counted<FrameTransformerChain>(
          scoped_refptr<FrameTransformerInterface>(
              reinterpret_cast<FrameTransformerInterface*>(j_first)),
          scoped_refptr<FrameTransformerInterface>(
              reinterpret_cast<FrameTransformerInterface*>(j_second)));
  // The Java object owns this reference and releases it in dispose().
  return jlongFromPointer(chain.release());
}

}  // namespace jni
}  // namespace webrtc

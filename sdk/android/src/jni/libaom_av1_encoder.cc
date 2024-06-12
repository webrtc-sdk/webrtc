/*
 *  Copyright 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/codecs/av1/libaom_av1_encoder.h"

#include <jni.h>

#include "api/environment/environment.h"
#include "sdk/android/generated_libaom_av1_encoder_jni/LibaomAv1Encoder_jni.h"
#include "sdk/android/src/jni/jni_helpers.h"

#include<vector>
#include<string>

namespace webrtc {
namespace jni {

jlong JNI_LibaomAv1Encoder_Create(JNIEnv* jni, jlong j_webrtc_env_ref) {
  return NativeToJavaPointer(
      CreateLibaomAv1Encoder(
          *reinterpret_cast<const Environment*>(j_webrtc_env_ref))
          .release());
}

static  webrtc::ScopedJavaLocalRef<jobject> JNI_LibaomAv1Encoder_GetSupportedScalabilityModes(JNIEnv* jni) {
  std::vector<std::string> modes;
   for (const auto scalability_mode : webrtc::kAllScalabilityModes) {
      if (webrtc::ScalabilityStructureConfig(scalability_mode).has_value()) {
       modes.push_back(std::string(webrtc::ScalabilityModeToString(scalability_mode)));
      }
    }
  return NativeToJavaStringArray(jni, modes);
}
}  // namespace jni
}  // namespace webrtc

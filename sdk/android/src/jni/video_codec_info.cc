/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/src/jni/video_codec_info.h"

#include "sdk/android/generated_video_jni/VideoCodecInfo_jni.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/src/jni/jni_helpers.h"
#include "api/video_codecs/scalability_mode.h"
#include "modules/video_coding/svc/scalability_mode_util.h"

namespace webrtc {
namespace jni {

SdpVideoFormat VideoCodecInfoToSdpVideoFormat(JNIEnv* jni,
                                              const JavaRef<jobject>& j_info) {
  std::vector<std::string> params =
      JavaToStdVectorStrings(jni, Java_VideoCodecInfo_getScalabilityModes(jni, j_info));
  absl::InlinedVector<ScalabilityMode, kScalabilityModeCount>
    scalability_modes;
  for (auto mode : params) {
    auto scalability_mode = ScalabilityModeFromString(mode);
    if (scalability_mode != absl::nullopt) {
      scalability_modes.push_back(*scalability_mode);
    }
  }
  return SdpVideoFormat(
      JavaToNativeString(jni, Java_VideoCodecInfo_getName(jni, j_info)),
      JavaToNativeStringMap(jni, Java_VideoCodecInfo_getParams(jni, j_info)),
      scalability_modes);
}

ScopedJavaLocalRef<jobject> SdpVideoFormatToVideoCodecInfo(
    JNIEnv* jni,
    const SdpVideoFormat& format) {
  ScopedJavaLocalRef<jobject> j_params =
      NativeToJavaStringMap(jni, format.parameters);
  webrtc::ScopedJavaLocalRef<jobject> j_scalability_modes;
  if (!format.scalability_modes.empty()) {
    JavaListBuilder builder(jni);
    for (auto mode : format.scalability_modes) {
      std::string scalability_mode(ScalabilityModeToString(mode));
      builder.add(NativeToJavaString(jni, scalability_mode));
    }
    j_scalability_modes = builder.java_list();
  }
  return Java_VideoCodecInfo_Constructor(
      jni, NativeToJavaString(jni, format.name), j_params, j_scalability_modes);
}

}  // namespace jni
}  // namespace webrtc

/*
 * Copyright 2023 LiveKit
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

#ifndef SDK_ANDROID_SRC_JNI_PC_RTP_CAPABLILITES_H_
#define SDK_ANDROID_SRC_JNI_PC_RTP_CAPABLILITES_H_

#include <jni.h>

#include "api/rtp_parameters.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"

namespace webrtc {
namespace jni {

RtpCapabilities JavaToNativeRtpCapabilities(JNIEnv* jni,
                                        const JavaRef<jobject>& j_capabilities);

ScopedJavaLocalRef<jobject> NativeToJavaRtpCapabilities(
    JNIEnv* jni,
    const RtpCapabilities& capabilities);

RtpCodecCapability JavaToNativeRtpCodecCapability(JNIEnv* jni,
                               const JavaRef<jobject>& j_codec_capability);

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_RTP_CAPABLILITES_H_

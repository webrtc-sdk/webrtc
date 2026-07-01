/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio/create_audio_device_module.h"

#include "absl/base/nullability.h"
#include "api/audio/audio_device.h"
#if defined(WEBRTC_IOS) || defined(WEBRTC_MAC)
#include "api/audio/create_audio_engine_device_module.h"
#endif
#include "api/environment/environment.h"
#include "api/scoped_refptr.h"
#include "modules/audio_device/audio_device_impl.h"

namespace webrtc {

absl_nullable scoped_refptr<AudioDeviceModule> CreateAudioDeviceModule(
    const Environment& env,
    AudioDeviceModule::AudioLayer audio_layer,
    bool bypass_voice_processing) {
#if defined(WEBRTC_IOS) || defined(WEBRTC_MAC)
  if (audio_layer == AudioDeviceModule::kAppleAudioEngine) {
    return CreateAudioEngineDeviceModule(env, bypass_voice_processing);
  }
#endif

  return AudioDeviceModuleImpl::Create(env, audio_layer, bypass_voice_processing);
}

}  // namespace webrtc

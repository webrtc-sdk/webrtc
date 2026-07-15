/*
 * Copyright 2026 LiveKit
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

#include "api/audio/create_audio_engine_device_module.h"

#include "api/make_ref_counted.h"
#include "modules/audio_device/audio_engine_device.h"

namespace webrtc {

absl_nullable scoped_refptr<AudioDeviceModule> CreateAudioEngineDeviceModule(
    const Environment& env,
    bool bypass_voice_processing) {
  return make_ref_counted<AudioEngineDevice>(env, bypass_voice_processing);
}

}  // namespace webrtc

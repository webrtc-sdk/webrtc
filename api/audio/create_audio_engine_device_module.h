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

#ifndef API_AUDIO_CREATE_AUDIO_ENGINE_DEVICE_MODULE_H_
#define API_AUDIO_CREATE_AUDIO_ENGINE_DEVICE_MODULE_H_

#include "absl/base/nullability.h"
#include "api/audio/audio_device.h"
#include "api/environment/environment.h"
#include "api/scoped_refptr.h"

namespace webrtc {

#if defined(WEBRTC_IOS) || defined(WEBRTC_MAC)
// Creates an AVAudioEngine based AudioDeviceModule.
//
// The returned module binds to the thread it is created on and expects all
// control calls (Init, StartPlayout, RegisterAudioCallback, destruction, ...)
// on that same thread. Call this on the worker thread, as
// RTCPeerConnectionFactory does for its AudioEngine device type.
absl_nullable scoped_refptr<AudioDeviceModule> CreateAudioEngineDeviceModule(
    const Environment& env,
    bool bypass_voice_processing);
#endif

}  // namespace webrtc

#endif  // API_AUDIO_CREATE_AUDIO_ENGINE_DEVICE_MODULE_H_

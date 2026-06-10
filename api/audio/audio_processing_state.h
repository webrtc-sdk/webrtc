/*
 *  Copyright 2026 LiveKit
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef API_AUDIO_AUDIO_PROCESSING_STATE_H_
#define API_AUDIO_AUDIO_PROCESSING_STATE_H_

#include <optional>

#include "api/audio_options.h"

namespace webrtc {

// Keep numeric values in sync with the Java and ObjC API enums because
// diagnostics pass these values across language boundaries.
enum class AudioProcessingImplementation {
  kUnknown = 0,
  kDisabled = 1,
  kSoftware = 2,
  kPlatform = 3,
  kSoftwareAndPlatform = 4,
};

// Diagnostic state of one audio processing component (echo cancellation,
// noise suppression, auto gain control or high-pass filter), observed at
// three stages of one pipeline: requested (caller intent) -> resolved (the
// engine's per-path decision) -> active (live truth), with `effective` as the
// merged verdict. std::optional fields are tri-state; nullopt means unknown
// (e.g. nothing requested yet, the resolver has not run, or the state cannot
// be read back). Language bindings collapse unknown to false and expose the
// requested pair as one nullable component-options object.
struct AudioProcessingComponentState {
  // The caller's most recent request for this component, as passed to
  // AudioTrackInterface::SetAudioProcessingOptions. Distinguishes "nobody
  // asked" (nullopt) from "asked for disabled" (false).
  std::optional<bool> requested_enabled;
  std::optional<AudioProcessingMode> requested_mode;

  // Whether the resolver decided the WebRTC software (APM) implementation
  // should run, after weighing the requested mode against platform
  // availability, coupling, and policy. Automatic mode resolves to software
  // exactly when the platform path is unavailable or disallowed.
  std::optional<bool> software_resolved;

  // Whether APM's live configuration currently has this component enabled
  // (AudioProcessing::GetConfig()). Normally equals software_resolved once
  // options are applied; differs while an apply is in flight, if applying
  // failed, or if something else has since reconfigured the shared APM.
  std::optional<bool> software_active;

  // Whether this device/OS offers a built-in implementation of this component
  // at all. Capability only - says nothing about whether it is in use.
  bool platform_available = false;

  // Whether the resolver decided the platform implementation should run, as
  // submitted to the OS. Unlike the software path, the OS owns the outcome:
  // it can decline, defer, or couple this with another component.
  std::optional<bool> platform_resolved;

  // Whether the device reports the platform implementation actually running
  // right now. Lags platform_resolved during engine transitions; stays false
  // if the OS rejected the request or the input path is not configured.
  std::optional<bool> platform_active;

  // The verdict: which implementation is in effect right now. Derived from
  // the active states (active wins over resolved when they disagree).
  AudioProcessingImplementation effective = AudioProcessingImplementation::kUnknown;
};

// Diagnostic snapshot of the resolved audio processing state for the shared
// audio processing module. The module is owned by the peer connection factory
// and shared engine-wide, so this reflects factory-scoped state.
//
// Device-level (platform) processing detail - topology, raw per-effect
// availability, Apple Voice Processing I/O state - is intentionally not
// embedded here; read it from the audio device module's platform audio
// processing state instead.
struct AudioProcessingState {
  bool has_audio_processing_module = false;

  AudioProcessingComponentState echo_cancellation;
  AudioProcessingComponentState noise_suppression;
  AudioProcessingComponentState auto_gain_control;
  AudioProcessingComponentState high_pass_filter;
};

}  // namespace webrtc

#endif  // API_AUDIO_AUDIO_PROCESSING_STATE_H_

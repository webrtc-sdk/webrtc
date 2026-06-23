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

#ifndef API_AUDIO_AUDIO_PROCESSING_OPTIONS_RESOLVER_H_
#define API_AUDIO_AUDIO_PROCESSING_OPTIONS_RESOLVER_H_

#include <optional>

#include "api/audio/audio_device.h"
#include "api/audio/audio_processing_options_result.h"
#include "api/audio_options.h"
#include "api/function_view.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct RTC_EXPORT CoupledAudioProcessingPathResolution {
  // True when AEC or NS is explicitly present in AudioOptions. Callers update
  // the shared platform path (to should_use_echo_noise_platform_path) only when
  // this is true. An AGC-only update leaves the path untouched.
  bool has_echo_or_noise_option = false;
  // Requested AEC/NS platform path state after applying the options. If AEC and
  // NS are absent, this mirrors the current platform path argument.
  bool should_use_echo_noise_platform_path = false;
  // Whether the AGC option asks for platform AGC. The caller must still gate
  // this on the effective AEC/NS platform path state because Apple AGC only has
  // effect while that path is active.
  bool auto_gain_control_wants_platform = false;
};

struct RTC_EXPORT AudioProcessingOptionsValidationContext {
  AudioDeviceModule::PlatformAudioProcessingTopology topology =
      AudioDeviceModule::PlatformAudioProcessingTopology::kIndependent;

  bool is_echo_cancellation_platform_available = false;
  bool is_noise_suppression_platform_available = false;
  bool is_auto_gain_control_platform_available = false;
  bool is_highpass_filter_platform_available = false;

  bool is_echo_noise_platform_path_available = false;
  bool is_echo_noise_platform_path_active = false;
};

RTC_EXPORT AudioProcessingMode AudioProcessingModeOrAutomatic(std::optional<AudioProcessingMode> mode);

RTC_EXPORT bool AudioProcessingOptionWantsPlatform(std::optional<bool> enabled,
                                                   std::optional<AudioProcessingMode> mode);

RTC_EXPORT bool AudioProcessingOptionRequestsSoftware(std::optional<bool> enabled,
                                                      std::optional<AudioProcessingMode> mode);

RTC_EXPORT bool AudioProcessingOptionIsPlatformOnly(std::optional<bool> enabled,
                                                    std::optional<AudioProcessingMode> mode);

RTC_EXPORT std::optional<bool> ResolveAudioProcessingSoftwareFromPlatformState(std::optional<bool> enabled,
                                                                               std::optional<AudioProcessingMode> mode,
                                                                               bool platform_enabled);

RTC_EXPORT AudioProcessingOptionsResult
ValidateAudioProcessingOptions(const AudioOptions &options, const AudioProcessingOptionsValidationContext &context);

// Builds a validation context from the live ADM policy and capability state.
// ADM methods must be called on the ADM's expected thread.
RTC_EXPORT AudioProcessingOptionsValidationContext
AudioProcessingValidationContextForAudioDeviceModule(const AudioDeviceModule *adm);

// Resolves the shared platform path for ADMs where AEC and NS cannot be
// controlled independently. This only computes intent. ADM and APM side effects
// stay in the caller.
//
// Rules:
// - Any enabled AEC or NS software request keeps the shared platform path off.
// - Any enabled AEC or NS auto/platform request turns the shared path on unless
//   a software request or disabled sibling vetoes it.
// - Strict platform requests with a disabled sibling are rejected by
//   ValidateAudioProcessingOptions before apply.
// - AGC alone never turns the shared AEC/NS path on.
//
// `is_echo_noise_platform_path_active` is queried only when both AEC and NS are
// absent from `options` (the AGC-only case), so callers may supply an ADM probe
// without paying for it on every update.
RTC_EXPORT CoupledAudioProcessingPathResolution
ResolveCoupledAudioProcessingPath(const AudioOptions &options, FunctionView<bool()> is_echo_noise_platform_path_active);

}  // namespace webrtc

#endif  // API_AUDIO_AUDIO_PROCESSING_OPTIONS_RESOLVER_H_

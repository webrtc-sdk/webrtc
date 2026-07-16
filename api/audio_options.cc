/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_options.h"

#include <optional>
#include <string>

#include "rtc_base/strings/string_builder.h"

namespace webrtc {
namespace {

template <class T>
void ToStringIfSet(StringBuilder* result,
                   const char* key,
                   const std::optional<T>& val) {
  if (val) {
    (*result) << key << ": " << *val << ", ";
  }
}

const char *AudioProcessingModeToString(AudioProcessingMode mode) {
  switch (mode) {
    case AudioProcessingMode::kAutomatic:
      return "auto";
    case AudioProcessingMode::kPlatform:
      return "platform";
    case AudioProcessingMode::kSoftware:
      return "software";
  }
  return "auto";
}

void AudioProcessingModeToStringIfSet(StringBuilder *result, const char *key,
                                      const std::optional<AudioProcessingMode> &val) {
  if (val) {
    (*result) << key << ": " << AudioProcessingModeToString(*val) << ", ";
  }
}

template <typename T>
void SetFrom(std::optional<T>* s, const std::optional<T>& o) {
  if (o) {
    *s = o;
  }
}

}  // namespace

AudioOptions::AudioOptions() = default;
AudioOptions::~AudioOptions() = default;

void AudioOptions::SetAll(const AudioOptions& change) {
  SetFrom(&echo_cancellation, change.echo_cancellation);
  SetFrom(&echo_cancellation_mode, change.echo_cancellation_mode);
#if defined(WEBRTC_IOS)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  SetFrom(&ios_force_software_aec_HACK, change.ios_force_software_aec_HACK);
#pragma clang diagnostic pop
#endif
  SetFrom(&auto_gain_control, change.auto_gain_control);
  SetFrom(&auto_gain_control_mode, change.auto_gain_control_mode);
  SetFrom(&noise_suppression, change.noise_suppression);
  SetFrom(&noise_suppression_mode, change.noise_suppression_mode);
  SetFrom(&highpass_filter, change.highpass_filter);
  SetFrom(&highpass_filter_mode, change.highpass_filter_mode);
  SetFrom(&stereo_swapping, change.stereo_swapping);
  SetFrom(&audio_jitter_buffer_max_packets,
          change.audio_jitter_buffer_max_packets);
  SetFrom(&audio_jitter_buffer_fast_accelerate,
          change.audio_jitter_buffer_fast_accelerate);
  SetFrom(&audio_jitter_buffer_min_delay_ms,
          change.audio_jitter_buffer_min_delay_ms);
  SetFrom(&audio_network_adaptor, change.audio_network_adaptor);
  SetFrom(&audio_network_adaptor_config, change.audio_network_adaptor_config);
  SetFrom(&init_recording_on_send, change.init_recording_on_send);
}

bool AudioOptions::operator==(const AudioOptions& o) const {
  return echo_cancellation == o.echo_cancellation && echo_cancellation_mode == o.echo_cancellation_mode &&
#if defined(WEBRTC_IOS)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
         ios_force_software_aec_HACK == o.ios_force_software_aec_HACK &&
#pragma clang diagnostic pop
#endif
         auto_gain_control == o.auto_gain_control && auto_gain_control_mode == o.auto_gain_control_mode &&
         noise_suppression == o.noise_suppression && noise_suppression_mode == o.noise_suppression_mode &&
         highpass_filter == o.highpass_filter && highpass_filter_mode == o.highpass_filter_mode &&
         stereo_swapping == o.stereo_swapping && audio_jitter_buffer_max_packets == o.audio_jitter_buffer_max_packets &&
         audio_jitter_buffer_fast_accelerate == o.audio_jitter_buffer_fast_accelerate &&
         audio_jitter_buffer_min_delay_ms == o.audio_jitter_buffer_min_delay_ms &&
         audio_network_adaptor == o.audio_network_adaptor &&
         audio_network_adaptor_config == o.audio_network_adaptor_config &&
         init_recording_on_send == o.init_recording_on_send;
}

std::string AudioOptions::ToString() const {
  StringBuilder result;
  result << "AudioOptions {";
  ToStringIfSet(&result, "aec", echo_cancellation);
  AudioProcessingModeToStringIfSet(&result, "aec_mode", echo_cancellation_mode);
#if defined(WEBRTC_IOS)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  ToStringIfSet(&result, "ios_force_software_aec_HACK",
                ios_force_software_aec_HACK);
#pragma clang diagnostic pop
#endif
  ToStringIfSet(&result, "agc", auto_gain_control);
  AudioProcessingModeToStringIfSet(&result, "agc_mode", auto_gain_control_mode);
  ToStringIfSet(&result, "ns", noise_suppression);
  AudioProcessingModeToStringIfSet(&result, "ns_mode", noise_suppression_mode);
  ToStringIfSet(&result, "hf", highpass_filter);
  AudioProcessingModeToStringIfSet(&result, "hf_mode", highpass_filter_mode);
  ToStringIfSet(&result, "swap", stereo_swapping);
  ToStringIfSet(&result, "audio_jitter_buffer_max_packets",
                audio_jitter_buffer_max_packets);
  ToStringIfSet(&result, "audio_jitter_buffer_fast_accelerate",
                audio_jitter_buffer_fast_accelerate);
  ToStringIfSet(&result, "audio_jitter_buffer_min_delay_ms",
                audio_jitter_buffer_min_delay_ms);
  ToStringIfSet(&result, "audio_network_adaptor", audio_network_adaptor);
  ToStringIfSet(&result, "init_recording_on_send", init_recording_on_send);
  result << "}";
  return result.Release();
}

}  // namespace webrtc

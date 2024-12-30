/*
 * Copyright 2024 LiveKit
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

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include "audio_engine_device.h"

#include <mach/mach_time.h>
#include <cmath>

#include "api/array_view.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "modules/audio_device/fine_audio_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/time_utils.h"

#import "base/RTCLogging.h"

#if defined(WEBRTC_IOS)
#import "components/audio/RTCAudioSession+Private.h"
#import "components/audio/RTCAudioSession.h"
#import "components/audio/RTCAudioSessionConfiguration.h"
#import "components/audio/RTCNativeAudioSessionDelegateAdapter.h"
#endif

namespace webrtc {

#define LOGI() RTC_LOG(LS_INFO) << "AudioEngineDevice::"
#define LOGE() RTC_LOG(LS_ERROR) << "AudioEngineDevice::"
#define LOGW() RTC_LOG(LS_WARNING) << "AudioEngineDevice::"

const UInt16 kFixedPlayoutDelayEstimate = 30;
const UInt16 kFixedRecordDelayEstimate = 30;

const size_t kMaximumFramesPerBuffer = 3072;  // Maximum slice size for VoiceProcessingIO
const size_t kAudioSampleSize = 2;            // Signed 16-bit integer

AudioEngineDevice::AudioEngineDevice(bool bypass_voice_processing)
    : bypass_voice_processing_(bypass_voice_processing),
      task_queue_factory_(CreateDefaultTaskQueueFactory()),
      initialized_(false) {
  LOGI() << "bypass_voice_processing " << bypass_voice_processing_;

  thread_ = rtc::Thread::Current();
  audio_device_buffer_.reset(new webrtc::AudioDeviceBuffer(task_queue_factory_.get()));

#if defined(WEBRTC_IOS)
  audio_session_observer_ =
      [[RTC_OBJC_TYPE(RTCNativeAudioSessionDelegateAdapter) alloc] initWithObserver:this];
  // Subscribe to audio session events.
  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  [session pushDelegate:audio_session_observer_];
#endif

  // Add observer for configuration changes
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  configuration_observer_ = (__bridge_retained void*)[center
      addObserverForName:AVAudioEngineConfigurationChangeNotification
                  object:audio_engine_
                   queue:nil
              usingBlock:^(NSNotification* notification) {
                OnEngineConfigurationChange();
              }];

  mach_timebase_info_data_t tinfo;
  mach_timebase_info(&tinfo);
  machTickUnitsToNanoseconds_ = (double)tinfo.numer / tinfo.denom;

  // Manual rendering formats are fixed to 48k for now.
  manual_render_rtc_format_ = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16
                                                               sampleRate:48000
                                                                 channels:1
                                                              interleaved:YES];
}

AudioEngineDevice::~AudioEngineDevice() {
  RTC_DCHECK_RUN_ON(thread_);

  if (configuration_observer_) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:(__bridge_transfer id)configuration_observer_];
    configuration_observer_ = nil;
  }

  safety_->SetNotAlive();

  Terminate();
  audio_session_observer_ = nil;
}

// MARK: - Main life cycle

bool AudioEngineDevice::Initialized() const {
  LOGI() << "Initialized";
  RTC_DCHECK_RUN_ON(thread_);

  return initialized_;
}

int32_t AudioEngineDevice::Init() {
  LOGI() << "Init";
  RTC_DCHECK_RUN_ON(thread_);

  if (initialized_) {
    LOGW() << "Init: Already initialized";
    return 0;
  }

#if defined(WEBRTC_IOS)
  RTC_OBJC_TYPE(RTCAudioSessionConfiguration)* config =
      [RTC_OBJC_TYPE(RTCAudioSessionConfiguration) webRTCConfiguration];
  playout_parameters_.reset(config.sampleRate, config.outputNumberOfChannels);
  record_parameters_.reset(config.sampleRate, config.inputNumberOfChannels);
#endif

  initialized_ = true;
  return 0;
}

int32_t AudioEngineDevice::Terminate() {
  LOGI() << "Terminate";
  RTC_DCHECK_RUN_ON(thread_);
  if (!initialized_) {
    return 0;
  }

  StopPlayout();
  StopRecording();

  initialized_ = false;
  return 0;
}

// ----------------------------------------------------------------------------------------------------
// Playout

bool AudioEngineDevice::PlayoutIsInitialized() const {
  LOGI() << "PlayoutIsInitialized";
  RTC_DCHECK_RUN_ON(thread_);

  return engine_state_.output_enabled;
}

bool AudioEngineDevice::Playing() const {
  LOGI() << "Playing";
  RTC_DCHECK_RUN_ON(thread_);

  return engine_state_.output_running;
}

int32_t AudioEngineDevice::InitPlayout() {
  LOGI() << "InitPlayout";
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(initialized_);

  if (engine_state_.output_enabled) {
    LOGW() << "InitPlayout: Already initialized";
    return 0;
  }

  SetEngineState([](EngineState state) -> EngineState {
    state.output_enabled = true;
    return state;
  });

  return 0;
}

int32_t AudioEngineDevice::StartPlayout() {
  LOGI() << "StartPlayout";
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(engine_state_.output_enabled);

  if (!engine_state_.output_enabled) {
    LOGW() << "StartPlayout: Not initialized";
    return -1;
  }

  if (engine_state_.output_running) {
    LOGW() << "StartPlayout: Already playing";
    return 0;
  }

  SetEngineState([](EngineState state) -> EngineState {
    state.output_running = true;
    return state;
  });

  return 0;
}

int32_t AudioEngineDevice::StopPlayout() {
  LOGI() << "StopPlayout";
  RTC_DCHECK_RUN_ON(thread_);

  if (!engine_state_.output_enabled) {
    LOGW() << "StopPlayout: Not initialized";
    return -1;
  }

  if (!engine_state_.output_running) {
    LOGW() << "StopPlayout: Already stopped";
    return 0;
  }

  SetEngineState([](EngineState state) -> EngineState {
    state.output_enabled = false;
    state.output_running = false;
    return state;
  });

  audio_device_buffer_->StopPlayout();

  return 0;
}

// ----------------------------------------------------------------------------------------------------
// Recording

bool AudioEngineDevice::RecordingIsInitialized() const {
  LOGI() << "RecordingIsInitialized";
  RTC_DCHECK_RUN_ON(thread_);

  return engine_state_.input_enabled;
}

bool AudioEngineDevice::Recording() const {
  LOGI() << "Recording";
  RTC_DCHECK_RUN_ON(thread_);

  return engine_state_.input_running;
}

int32_t AudioEngineDevice::InitRecording() {
  LOGI() << "InitRecording";
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(initialized_);

  if (engine_state_.input_enabled) {
    LOGW() << "InitRecording: Already initialized";
    return 0;
  }

  SetEngineState([](EngineState state) -> EngineState {
    state.output_enabled = true;
    state.input_enabled = true;
    state.input_muted = true;  // Muted by default
    return state;
  });

  return 0;
}

int32_t AudioEngineDevice::StartRecording() {
  LOGI() << "StartRecording";
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(engine_state_.input_enabled);

  if (!engine_state_.input_enabled) {
    LOGW() << "StartRecording: Not initialized";
    return -1;
  }

  if (engine_state_.input_running) {
    LOGW() << "StartRecording: Already recording";
    return 0;
  }

  SetEngineState([](EngineState state) -> EngineState {
    state.input_running = true;
    state.input_muted = false;  // Always unmute
    return state;
  });

  return 0;
}

int32_t AudioEngineDevice::StopRecording() {
  LOGI() << "StopRecording";
  RTC_DCHECK_RUN_ON(thread_);

  if (!engine_state_.input_enabled) {
    LOGW() << "StopRecording: Not initialized";
    return -1;
  }

  if (!engine_state_.input_running) {
    LOGW() << "StopRecording: Already stopped";
    return 0;
  }

  SetEngineState([](EngineState state) -> EngineState {
    state.input_enabled = false;
    state.input_running = false;
    return state;
  });

  audio_device_buffer_->StopRecording();

  return 0;
}

// ----------------------------------------------------------------------------------------------------
// AudioSessionObserver

void AudioEngineDevice::OnInterruptionBegin() {
  LOGI() << "OnInterruptionBegin";

  RTC_DCHECK(thread_);
  thread_->PostTask(SafeTask(safety_, [this] {
    this->SetEngineState([](EngineState state) -> EngineState {
      state.is_interrupted = true;
      return state;
    });
  }));
}

void AudioEngineDevice::OnInterruptionEnd() {
  LOGI() << "OnInterruptionEnd";

  RTC_DCHECK(thread_);
  thread_->PostTask(SafeTask(safety_, [this] {
    this->SetEngineState([](EngineState state) -> EngineState {
      state.is_interrupted = false;
      return state;
    });
  }));
}

void AudioEngineDevice::OnValidRouteChange() {
  LOGI() << "OnValidRouteChange";
  RTC_DCHECK(thread_);
}

void AudioEngineDevice::OnCanPlayOrRecordChange(bool can_play_or_record) {
  LOGI() << "OnCanPlayOrRecordChange";
  RTC_DCHECK(thread_);
}

void AudioEngineDevice::OnChangedOutputVolume() {
  LOGI() << "OnChangedOutputVolume";
  RTC_DCHECK(thread_);
}

// ----------------------------------------------------------------------------------------------------
// Not Implemented

bool AudioEngineDevice::IsInterrupted() {
  LOGI() << "IsInterrupted";
  RTC_DCHECK_RUN_ON(thread_);

  return engine_state_.is_interrupted;
}

int32_t AudioEngineDevice::ActiveAudioLayer(AudioDeviceModule::AudioLayer* audioLayer) const {
  LOGI() << "ActiveAudioLayer";
  if (audioLayer == nullptr) {
    return -1;
  }

  *audioLayer = AudioDeviceModule::kPlatformDefaultAudio;

  return 0;
}

int32_t AudioEngineDevice::InitSpeaker() {
  LOGI() << "InitSpeaker";

  return 0;
}

bool AudioEngineDevice::SpeakerIsInitialized() const {
  LOGI() << "SpeakerIsInitialized";

  return true;
}

int32_t AudioEngineDevice::SpeakerVolumeIsAvailable(bool* available) {
  LOGI() << "SpeakerVolumeIsAvailable";
  if (available == nullptr) {
    return -1;
  }

  *available = false;

  return 0;
}

int32_t AudioEngineDevice::SetSpeakerVolume(uint32_t volume) {
  LOGW() << "SetSpeakerVolume: Not implemented, value: " << volume;

  return -1;
}

int32_t AudioEngineDevice::SpeakerVolume(uint32_t* volume) const {
  LOGW() << "SpeakerVolume: Not implemented";

  return -1;
}

int32_t AudioEngineDevice::MaxSpeakerVolume(uint32_t* maxVolume) const {
  LOGW() << "MaxSpeakerVolume: Not implemented";

  return -1;
}

int32_t AudioEngineDevice::MinSpeakerVolume(uint32_t* minVolume) const {
  LOGW() << "MinSpeakerVolume: Not implemented";

  return -1;
}

int32_t AudioEngineDevice::SpeakerMuteIsAvailable(bool* available) {
  LOGI() << "SpeakerMuteIsAvailable";
  if (available == nullptr) {
    return -1;
  }

  *available = false;

  return 0;
}

int32_t AudioEngineDevice::SetSpeakerMute(bool enable) {
  LOGI() << "SetSpeakerMute: " << enable;

  return -1;
}

int32_t AudioEngineDevice::SpeakerMute(bool* enabled) const {
  LOGW() << "SpeakerMute: Not implemented";

  return -1;
}

int32_t AudioEngineDevice::InitMicrophone() {
  LOGI() << "InitMicrophone";
  RTC_DCHECK_RUN_ON(thread_);

  return 0;
}

bool AudioEngineDevice::MicrophoneIsInitialized() const {
  LOGI() << "MicrophoneIsInitialized";
  RTC_DCHECK_RUN_ON(thread_);

  return true;
}

// ----------------------------------------------------------------------------------------------------
// Microphone Muting

int32_t AudioEngineDevice::MicrophoneMuteIsAvailable(bool* available) {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "MicrophoneMuteIsAvailable";
  if (available == nullptr) {
    return -1;
  }

  *available = true;

  return 0;
}

int32_t AudioEngineDevice::SetMicrophoneMute(bool enable) {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "SetMicrophoneMute: " << enable;

  SetEngineState([enable](EngineState state) -> EngineState {
    state.input_muted = enable;
    return state;
  });

  return 0;
}

int32_t AudioEngineDevice::MicrophoneMute(bool* enabled) const {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "MicrophoneMute";

  if (enabled == nullptr) {
    return -1;
  }

  *enabled = engine_state_.input_muted;

  return 0;
}

// ----------------------------------------------------------------------------------------------------
// Stereo Playout

int32_t AudioEngineDevice::StereoPlayoutIsAvailable(bool* available) const {
  LOGI() << "StereoPlayoutIsAvailable";
  if (available == nullptr) {
    return -1;
  }

  *available = false;

  return 0;
}

int32_t AudioEngineDevice::SetStereoPlayout(bool enable) {
  LOGW() << "SetStereoPlayout: Not implemented, value:" << enable;

  audio_device_buffer_->SetPlayoutChannels(1);

  return 0;
}

int32_t AudioEngineDevice::StereoPlayout(bool* enabled) const {
  LOGI() << "StereoPlayout";
  if (enabled == nullptr) {
    return -1;
  }

  *enabled = false;

  return 0;
}

// ----------------------------------------------------------------------------------------------------
// Stereo Recording

int32_t AudioEngineDevice::StereoRecordingIsAvailable(bool* available) const {
  LOGI() << "StereoRecordingIsAvailable";
  if (available == nullptr) {
    return -1;
  }

  *available = false;

  return 0;
}

int32_t AudioEngineDevice::SetStereoRecording(bool enable) {
  LOGW() << "SetStereoRecording: Not implemented, value: " << enable;

  audio_device_buffer_->SetRecordingChannels(1);

  return 0;
}

int32_t AudioEngineDevice::StereoRecording(bool* enabled) const {
  LOGI() << "StereoRecording";
  if (enabled == nullptr) {
    return -1;
  }

  *enabled = false;

  return 0;
}

// ----------------------------------------------------------------------------------------------------
// Microphone Volume

int32_t AudioEngineDevice::MicrophoneVolumeIsAvailable(bool* available) {
  LOGI() << "MicrophoneVolumeIsAvailable";
  if (available == nullptr) {
    return -1;
  }

  *available = false;

  return 0;
}

int32_t AudioEngineDevice::SetMicrophoneVolume(uint32_t volume) {
  LOGW() << "SetMicrophoneVolume: Not implemented, value: " << volume;

  return -1;
}

int32_t AudioEngineDevice::MicrophoneVolume(uint32_t* volume) const {
  LOGW() << "SetMicrophoneVolume: Not implemented";

  return -1;
}

int32_t AudioEngineDevice::MaxMicrophoneVolume(uint32_t* maxVolume) const {
  LOGW() << "SetMicrophoneVolume: Not implemented";

  return -1;
}

int32_t AudioEngineDevice::MinMicrophoneVolume(uint32_t* minVolume) const {
  LOGW() << "MinMicrophoneVolume: Not implemented";

  return -1;
}

// ----------------------------------------------------------------------------------------------------
// Playout Device

int32_t AudioEngineDevice::PlayoutIsAvailable(bool* available) {
  LOGI() << "PlayoutIsAvailable";
  if (available == nullptr) {
    return -1;
  }

  *available = true;

  return 0;
}

int32_t AudioEngineDevice::SetPlayoutDevice(uint16_t index) {
  LOGW() << "SetPlayoutDevice: Not implemented, value: " << index;

  return 0;
}

int32_t AudioEngineDevice::SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType deviceType) {
  LOGW() << "SetPlayoutDevice: Not implemented, value: " << deviceType;

  return -1;
}

int32_t AudioEngineDevice::PlayoutDeviceName(uint16_t index, char name[kAdmMaxDeviceNameSize],
                                             char guid[kAdmMaxGuidSize]) {
  // LOGW() << "PlayoutDeviceName: Not implemented";

  return -1;
}

int16_t AudioEngineDevice::PlayoutDevices() {
  // LOGI() << "PlayoutDevices";

  return (int16_t)1;
}

// ----------------------------------------------------------------------------------------------------
// Recording Device

int32_t AudioEngineDevice::RecordingDeviceName(uint16_t index, char name[kAdmMaxDeviceNameSize],
                                               char guid[kAdmMaxGuidSize]) {
  // LOGW() << "RecordingDeviceName";

  return -1;
}

int32_t AudioEngineDevice::SetRecordingDevice(uint16_t index) {
  LOGI() << "SetRecordingDevice, index: " << index;

  return 0;
}

int32_t AudioEngineDevice::SetRecordingDevice(AudioDeviceModule::WindowsDeviceType type) {
  LOGI() << "SetRecordingDevice, type: " << type;

  return -1;
}

int32_t AudioEngineDevice::RecordingIsAvailable(bool* available) {
  LOGI() << "RecordingIsAvailable";
  if (available == nullptr) {
    return -1;
  }

  *available = true;

  return 0;
}

int16_t AudioEngineDevice::RecordingDevices() {
  // LOGI() << "RecordingDevices";

  return (int16_t)1;
}

//

int32_t AudioEngineDevice::RegisterAudioCallback(AudioTransport* audioCallback) {
  LOGI() << "RegisterAudioCallback";
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(audio_device_buffer_ != nullptr);
  RTC_DCHECK(audioCallback != nullptr);

  return audio_device_buffer_->RegisterAudioCallback(audioCallback);
}

// ----------------------------------------------------------------------------------------------------
// Misc

bool AudioEngineDevice::BuiltInAECIsAvailable() const { return false; }

bool AudioEngineDevice::BuiltInAGCIsAvailable() const { return false; }

bool AudioEngineDevice::BuiltInNSIsAvailable() const { return false; }

int32_t AudioEngineDevice::EnableBuiltInAEC(bool enable) { return -1; }

int32_t AudioEngineDevice::EnableBuiltInAGC(bool enable) { return -1; }

int32_t AudioEngineDevice::EnableBuiltInNS(bool enable) { return -1; }

// ----------------------------------------------------------------------------------------------------
// Misc

#if defined(WEBRTC_IOS)
int AudioEngineDevice::GetPlayoutAudioParameters(AudioParameters* params) const { return -1; }
int AudioEngineDevice::GetRecordAudioParameters(AudioParameters* params) const { return -1; }
#endif

int32_t AudioEngineDevice::PlayoutDelay(uint16_t* delayMS) const {
  // LOGI() << "PlayoutDelay";
  if (delayMS == nullptr) {
    return -1;
  }

  *delayMS = kFixedPlayoutDelayEstimate;

  return 0;
}

int32_t AudioEngineDevice::SetObserver(AudioDeviceObserver* observer) {
  LOGI() << "SetObserver";
  RTC_DCHECK_RUN_ON(thread_);

  observer_ = observer;

  return 0;
}

// ----------------------------------------------------------------------------------------------------
// Unique methods to AudioEngineDevice

int32_t AudioEngineDevice::ManualRenderingMode(bool* enabled) {
  LOGI() << "ManualRenderingMode";
  RTC_DCHECK_RUN_ON(thread_);

  if (enabled == nullptr) {
    return -1;
  }

  *enabled = engine_state_.is_manual_mode;

  return 0;
}

int32_t AudioEngineDevice::SetManualRenderingMode(bool enable) {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "SetManualRenderingMode: " << enable;

  SetEngineState([enable](EngineState state) -> EngineState {
    state.is_manual_mode = enable;
    return state;
  });

  return 0;
}

int32_t AudioEngineDevice::InitAndStartRecording() {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "InitAndStartRecording";

  if (engine_state_.input_running) {
    LOGW() << "InitAndStartRecording: Already recording";
    return 0;
  }

  audio_device_buffer_->StartRecording();

  if (fine_audio_buffer_) {
    fine_audio_buffer_->ResetRecord();
  }

  SetEngineState([](EngineState state) -> EngineState {
    state.output_enabled = true;
    state.output_running = true;
    state.input_enabled = true;
    state.input_running = true;
    state.input_muted = false;  // Always unmute
    return state;
  });

  return 0;
}

int32_t AudioEngineDevice::SetAdvancedDucking(bool enable) {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "SetAdvancedDucking: " << enable;

  SetEngineState([enable](EngineState state) -> EngineState {
    state.advanced_ducking = enable;
    return state;
  });

  return 0;
}

int32_t AudioEngineDevice::AdvancedDucking(bool* enabled) {
  RTC_DCHECK_RUN_ON(thread_);

  if (enabled == nullptr) {
    return -1;
  }

  *enabled = engine_state_.advanced_ducking;
  LOGI() << "AdvancedDucking value: " << *enabled;

  return 0;
}

int32_t AudioEngineDevice::SetDuckingLevel(long level) {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "SetDuckingLevel: " << level;

  SetEngineState([level](EngineState state) -> EngineState {
    state.ducking_level = level;
    return state;
  });

  return 0;
}

int32_t AudioEngineDevice::DuckingLevel(long* level) {
  LOGI() << "DuckingLevel";
  RTC_DCHECK_RUN_ON(thread_);

  if (level == nullptr) {
    return -1;
  }

  *level = engine_state_.ducking_level;
  LOGI() << "DuckingLevel value: " << *level;

  return 0;
}

// ----------------------------------------------------------------------------------------------------
// Private - Engine Related

void AudioEngineDevice::OnEngineConfigurationChange() {
  LOGI() << "OnEngineConfigurationChange";

  // thread_->PostTask(SafeTask(safety_, [this] {
  //   RTC_DCHECK_RUN_ON(thread_);

  //   EngineState previous_state = this->engine_state_;

  //   this->SetEngineState([](EngineState state) -> EngineState {
  //     return EngineState();  // Return default state to shutdown
  //   });

  //   this->SetEngineState([previous_state](EngineState state) -> EngineState {
  //     return previous_state;  // Recover engine state
  //   });
  // }));
}

bool AudioEngineDevice::IsMicrophonePermissionGranted() {
  AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
  return status == AVAuthorizationStatusAuthorized;
}

void AudioEngineDevice::SetEngineState(std::function<EngineState(EngineState)> state_transform) {
  RTC_DCHECK_RUN_ON(thread_);

  EngineState old_state = engine_state_;
  EngineState new_state = state_transform(old_state);

  if (old_state == new_state) {
    LOGI() << "SetEngineState: Nothing to update";
    return;
  }

  // Checks
  if (new_state.input_running) {
    RTC_DCHECK(new_state.input_enabled);
  }

  if (new_state.output_running) {
    RTC_DCHECK(new_state.output_enabled);
  }

  engine_state_ = new_state;
  UpdateEngineState(old_state, new_state);
}

void AudioEngineDevice::UpdateEngineState(EngineState old_state, EngineState new_state) {
  RTC_DCHECK_RUN_ON(thread_);

  // Playout or Recording enabled, create an engine instance.
  bool is_new_engine = !old_state.IsAnyEnabled() && new_state.IsAnyEnabled();
  // Playout or Recording not enabled, destroy engine instance.
  bool is_release_engine = old_state.IsAnyEnabled() && !new_state.IsAnyEnabled();

  bool is_restart_required = (old_state.input_enabled != new_state.input_enabled) ||
                             (old_state.output_enabled != new_state.output_enabled);

  if (is_new_engine) {
    LOGI() << "Creating AVAudioEngine...";
    audio_engine_ = [[AVAudioEngine alloc] init];
  }

  if (old_state.IsAnyRunning()) {
    if (!new_state.IsAnyRunning() || is_restart_required) {
      LOGI() << "Stopping AVAudioEngine...";
      [audio_engine_ stop];
    } else if (!old_state.is_interrupted && new_state.is_interrupted) {
      LOGI() << "Pausing AVAudioEngine...";
      [audio_engine_ pause];
    }

    if (!new_state.IsAnyRunning() || is_restart_required ||
        (!old_state.is_interrupted && new_state.is_interrupted)) {
      if (old_state.output_running && !new_state.output_running) {
        LOGI() << "Stopping Playout buffer...";
        audio_device_buffer_->StopPlayout();
      }
      if (old_state.input_running && !new_state.input_running) {
        LOGI() << "Stopping Record buffer...";
        audio_device_buffer_->StopRecording();
      }
    }
  }

  if ((!old_state.output_enabled && new_state.output_enabled) ||
      (!old_state.input_enabled && new_state.input_enabled)) {
    if (observer_ != nullptr) {
      // Invoke here before configuring nodes. In iOS, session configuration is required before
      // enabling AGC, muted talker etc.
      observer_->OnEngineWillStart(audio_engine_, new_state.output_enabled,
                                   new_state.input_enabled);
    }
  }

  if (!old_state.output_enabled && new_state.output_enabled) {
    LOGI() << "Enabling output for AVAudioEngine...";
    RTC_DCHECK(!audio_engine_.running);

    // Turning voice processing on outputNode, will turn on for inputNode also and mic indicator
    // goes on. if (!audio_engine_.outputNode.voiceProcessingEnabled) {
    //   NSError* error = nil;
    //   BOOL set_vp_result = [audio_engine_.outputNode setVoiceProcessingEnabled:YES error:&error];
    //   if (!set_vp_result) {
    //     NSLog(@"setVoiceProcessingEnabled error: %@", error.localizedDescription);
    //     RTC_DCHECK(set_vp_result);
    //   }
    //   LOGI() << "setVoiceProcessingEnabled (output) result: " << set_vp_result ? "YES" : "NO";
    // }
    AVAudioFormat* output_node_format = [this->OutputNode() outputFormatForBus:0];

    LOGI() << "Output format sampleRate: " << output_node_format.sampleRate
           << " channels: " << output_node_format.channelCount;

    AVAudioFormat* engine_output_format = [[AVAudioFormat alloc]
        initWithCommonFormat:output_node_format.commonFormat  // Usually float32
                  sampleRate:output_node_format.sampleRate
                    channels:1
                 interleaved:output_node_format.interleaved];

    audio_device_buffer_->SetPlayoutSampleRate(engine_output_format.sampleRate);
    audio_device_buffer_->SetPlayoutChannels(engine_output_format.channelCount);
    RTC_DCHECK(audio_device_buffer_ != nullptr);
    fine_audio_buffer_.reset(new FineAudioBuffer(audio_device_buffer_.get()));

    AVAudioFormat* rtc_output_format =
        [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16
                                         sampleRate:engine_output_format.sampleRate
                                           channels:1
                                        interleaved:YES];

    AVAudioSourceNodeRenderBlock source_block =
        ^OSStatus(BOOL* isSilence, const AudioTimeStamp* timestamp, AVAudioFrameCount frameCount,
                  AudioBufferList* outputData) {
          RTC_DCHECK(outputData->mNumberBuffers == 1);

          int16_t* dest_buffer = (int16_t*)outputData->mBuffers[0].mData;

          fine_audio_buffer_->GetPlayoutData(
              rtc::ArrayView<int16_t>(static_cast<int16_t*>(dest_buffer), frameCount),
              kFixedPlayoutDelayEstimate);

          return noErr;
        };

    source_node_ = [[AVAudioSourceNode alloc] initWithFormat:rtc_output_format
                                                 renderBlock:source_block];
    [audio_engine_ attachNode:source_node_];

    if (!(this->observer_ != nullptr &&
          this->observer_->OnEngineWillConnectOutput(
              audio_engine_, source_node_, audio_engine_.mainMixerNode, engine_output_format))) {
      // Default implementation.
      [audio_engine_ connect:source_node_
                          to:audio_engine_.mainMixerNode
                      format:engine_output_format];
    }

    [audio_engine_ connect:audio_engine_.mainMixerNode
                        to:this->OutputNode()
                    format:engine_output_format];

  } else if (old_state.output_enabled && !new_state.output_enabled) {
    LOGI() << "Disabling output for AVAudioEngine...";
    RTC_DCHECK(!audio_engine_.running);

    // Disconnect
    if (source_node_ != nil) {
      [audio_engine_ disconnectNodeInput:source_node_];
      [audio_engine_ disconnectNodeOutput:source_node_];
      [audio_engine_ detachNode:source_node_];
      source_node_ = nil;
    }
  }

  if (!old_state.input_enabled && new_state.input_enabled) {
    LOGI() << "Enabling input for AVAudioEngine...";
    RTC_DCHECK(!audio_engine_.running);

    if (!this->InputNode().voiceProcessingEnabled) {
      NSError* error = nil;
      BOOL set_vp_result = [this->InputNode() setVoiceProcessingEnabled:YES error:&error];
      if (!set_vp_result) {
        NSLog(@"setVoiceProcessingEnabled error: %@", error.localizedDescription);
        RTC_DCHECK(set_vp_result);
      }
      LOGI() << "setVoiceProcessingEnabled (input) result: " << set_vp_result ? "YES" : "NO";
    }

    if (!this->InputNode().isVoiceProcessingAGCEnabled) {
      LOGW() << "voiceProcessingAGCEnabled (input) is false, ensure AVAudioSession.Mode is "
                "videoChat or voiceChat.";
    }

    if (this->InputNode().voiceProcessingEnabled) {
      // Muted talker detection.
      if (@available(iOS 17.0, macCatalyst 17.0, macOS 14.0, tvOS 17.0, visionOS 1.0, *)) {
        auto listener_block = ^(AVAudioVoiceProcessingSpeechActivityEvent event) {
          LOGI() << "AVAudioVoiceProcessingSpeechActivityEvent: " << event;
          RTC_DCHECK(event == AVAudioVoiceProcessingSpeechActivityStarted ||
                     event == AVAudioVoiceProcessingSpeechActivityEnded);
          AudioDeviceModule::SpeechActivityEvent rtc_event =
              (event == AVAudioVoiceProcessingSpeechActivityStarted
                   ? AudioDeviceModule::SpeechActivityEvent::kStarted
                   : AudioDeviceModule::SpeechActivityEvent::kEnded);

          thread_->PostTask(SafeTask(safety_, [this, rtc_event] {
            RTC_DCHECK_RUN_ON(thread_);  // Silence warning.
            if (this->observer_ != nullptr) {
              this->observer_->OnSpeechActivityEvent(rtc_event);
            }
          }));
        };

        BOOL set_listener_result =
            [this->InputNode() setMutedSpeechActivityEventListener:listener_block];
        if (set_listener_result) {
          LOGI() << "setMutedSpeechActivityEventListener success";
        } else {
          LOGW() << "setMutedSpeechActivityEventListener failed, ensure AVAudioSession.Mode is "
                    "videoChat or voiceChat.";
        }
      }
    }

    input_eq_node_ = [[AVAudioUnitEQ alloc] initWithNumberOfBands:2];
    [audio_engine_ attachNode:input_eq_node_];

    input_mixer_node_ = [[AVAudioMixerNode alloc] init];
    [audio_engine_ attachNode:input_mixer_node_];

    AVAudioFormat* input_node_format = [this->InputNode() outputFormatForBus:0];
    // Example formats:
    // Airpods: 1 ch,  24000 Hz, Float32
    // Mac: 9 ch,  48000 Hz, Float32
    LOGI() << "Input format, sampleRate: " << input_node_format.sampleRate
           << " channels: " << input_node_format.channelCount;

    // When VoiceProcessingIO is enabled, channels must be reduced from Mac's default 9 channels
    // to 2 or lower.
    AVAudioFormat* engine_input_format = [[AVAudioFormat alloc]
        initWithCommonFormat:input_node_format.commonFormat  // Usually float32
                  sampleRate:input_node_format.sampleRate
                    channels:1
                 interleaved:input_node_format.interleaved];

    audio_device_buffer_->SetRecordingSampleRate(engine_input_format.sampleRate);
    audio_device_buffer_->SetRecordingChannels(engine_input_format.channelCount);
    RTC_DCHECK(audio_device_buffer_ != nullptr);
    fine_audio_buffer_.reset(new FineAudioBuffer(audio_device_buffer_.get()));

    AVAudioFormat* rtc_input_format =
        [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16
                                         sampleRate:engine_input_format.sampleRate
                                           channels:1
                                        interleaved:YES];

    AVAudioSinkNodeReceiverBlock sink_block = ^OSStatus(const AudioTimeStamp* timestamp,
                                                        AVAudioFrameCount frameCount,
                                                        const AudioBufferList* inputData) {
      RTC_DCHECK(inputData->mNumberBuffers == 1);

      const int64_t capture_time_ns = timestamp->mHostTime * machTickUnitsToNanoseconds_;
      const int16_t* rtc_buffer = (int16_t*)inputData->mBuffers[0].mData;

      fine_audio_buffer_->DeliverRecordedData(rtc::ArrayView<const int16_t>(rtc_buffer, frameCount),
                                              kFixedRecordDelayEstimate, capture_time_ns);

      return noErr;
    };

    if (!(observer_ != nullptr &&
          observer_->OnEngineWillConnectInput(audio_engine_, this->InputNode(), input_mixer_node_,
                                              engine_input_format))) {
      // Default implementation.
      [audio_engine_ connect:this->InputNode() to:input_mixer_node_ format:engine_input_format];
    }

    sink_node_ = [[AVAudioSinkNode alloc] initWithReceiverBlock:sink_block];
    [audio_engine_ attachNode:sink_node_];

    // Convert to RTC's internal format before passing buffers to SinkNode.
    [audio_engine_ connect:input_mixer_node_ to:sink_node_ format:rtc_input_format];

  } else if (old_state.input_enabled && !new_state.input_enabled) {
    LOGI() << "Disabling input for AVAudioEngine...";
    RTC_DCHECK(!audio_engine_.running);

    // Disconnect input eq
    if (input_eq_node_ != nil) {
      [audio_engine_ disconnectNodeInput:input_eq_node_];
      [audio_engine_ disconnectNodeOutput:input_eq_node_];
      [audio_engine_ detachNode:input_eq_node_];
      input_eq_node_ = nil;
    }

    // InputMixerNode
    if (input_mixer_node_ != nil) {
      [audio_engine_ disconnectNodeInput:input_mixer_node_];
      [audio_engine_ disconnectNodeOutput:input_mixer_node_];
      [audio_engine_ detachNode:input_mixer_node_];
      input_mixer_node_ = nil;
    }

    // SinkNode
    if (sink_node_ != nil) {
      [audio_engine_ disconnectNodeInput:sink_node_];
      [audio_engine_ disconnectNodeOutput:sink_node_];
      [audio_engine_ detachNode:sink_node_];
      sink_node_ = nil;
    }
  }

  if (new_state.input_enabled) {
    if (this->InputNode().voiceProcessingEnabled) {
      // Re-apply muted state.
      this->InputNode().voiceProcessingInputMuted = new_state.input_muted;
    }
  }

#if !TARGET_OS_TV
  if (new_state.input_enabled && this->InputNode().voiceProcessingEnabled &&
      (!old_state.input_enabled || (old_state.advanced_ducking != new_state.advanced_ducking ||
                                    old_state.ducking_level != new_state.ducking_level))) {
    // Other audio ducking.
    // iOS 17.0+, iPadOS 17.0+, Mac Catalyst 17.0+, macOS 14.0+, visionOS 1.0+
    if (@available(iOS 17.0, macCatalyst 17.0, macOS 14.0, visionOS 1.0, *)) {
      AVAudioVoiceProcessingOtherAudioDuckingConfiguration ducking_config;
      ducking_config.enableAdvancedDucking = new_state.advanced_ducking;
      ducking_config.duckingLevel =
          (AVAudioVoiceProcessingOtherAudioDuckingLevel)new_state.ducking_level;

      LOGI() << "setVoiceProcessingOtherAudioDuckingConfiguration";
      this->InputNode().voiceProcessingOtherAudioDuckingConfiguration = ducking_config;
    }
  }
#endif

  if ((!old_state.output_running && new_state.output_running && !new_state.input_running) ||
      (!old_state.output_enabled && new_state.output_enabled && new_state.input_running)) {
    LOGI() << "Starting Playout buffer...";
    audio_device_buffer_->StartPlayout();
    fine_audio_buffer_->ResetPlayout();
  }

  if ((!old_state.input_running && new_state.input_running && !new_state.output_running) ||
      (!old_state.input_enabled && new_state.input_enabled && new_state.output_running)) {
    LOGI() << "Starting Record buffer...";
    audio_device_buffer_->StartRecording();
    fine_audio_buffer_->ResetRecord();
  }

  if (new_state.IsAnyRunning()) {
    if (!old_state.IsAnyRunning() || (old_state.is_interrupted && !new_state.is_interrupted) ||
        is_restart_required) {
      LOGI() << "Starting AVAudioEngine...";
      NSError* error = nil;
      BOOL start_result = [audio_engine_ startAndReturnError:&error];
      if (!start_result) {
        LOGE() << "Failed to start engine: " << error.localizedDescription.UTF8String;
        DebugAudioEngine();
      }
    }
  }

  if (is_release_engine) {
    LOGI() << "Releasing AVAudioEngine...";
    audio_engine_ = nil;
  }
}

// ----------------------------------------------------------------------------------------------------
// Private - EngineState

void AudioEngineDevice::StartRenderLoop() {
  RTC_DCHECK_RUN_ON(render_thread_.get());

  // Constants for timing and frame management
  const double sample_rate = manual_render_rtc_format_.sampleRate;
  // Fixed number of frames to render per cycle
  const double target_frame_count = sample_rate / 100;
  const double nanoseconds_per_frame = 1e9 / sample_rate;
  const double target_cycle_time_ns = target_frame_count * nanoseconds_per_frame;

  // Variables for timing management
  uint64_t last_cycle_time = mach_absolute_time();
  double sleep_time_ms = 5.0;  // Initial sleep time
  const double min_sleep_time_ms = 1.0;
  const double max_sleep_time_ms = 20.0;

  // Simple moving average for sleep time adjustment
  constexpr size_t avg_window_size = 5;
  std::array<double, avg_window_size> cycle_times{};
  size_t cycle_index = 0;

  while (!render_thread_->IsQuitting()) {
    RTC_DCHECK(render_buffer_ != nullptr);
    AudioBufferList* abl = const_cast<AudioBufferList*>(render_buffer_.audioBufferList);

    // Calculate timing
    uint64_t current_time = mach_absolute_time();
    double elapsed_time_ns = (current_time - last_cycle_time) * machTickUnitsToNanoseconds_;

    // Update moving average of cycle times
    cycle_times[cycle_index] = elapsed_time_ns;
    cycle_index = (cycle_index + 1) % avg_window_size;

    // Calculate average cycle time
    double avg_cycle_time = 0;
    for (double time : cycle_times) {
      avg_cycle_time += time;
    }
    avg_cycle_time /= avg_window_size;

    // Adjust sleep time based on average cycle time
    if (avg_cycle_time > 0) {  // Only adjust if we have valid timing data
      double time_diff = target_cycle_time_ns - avg_cycle_time;
      double adjustment =
          (time_diff / target_cycle_time_ns) * sleep_time_ms * 0.1;  // Gradual adjustment
      sleep_time_ms = std::clamp(sleep_time_ms + adjustment, min_sleep_time_ms, max_sleep_time_ms);
    }

    // Set fixed frame count
    unsigned int frames_to_render = target_frame_count;

    // Adjust buffer size for the fixed frame count
    abl->mBuffers[0].mDataByteSize = frames_to_render * kAudioSampleSize;

    // Render audio
    OSStatus err = noErr;
    AVAudioEngineManualRenderingStatus result = render_block_(frames_to_render, abl, &err);

    if (result == AVAudioEngineManualRenderingStatusSuccess) {
      LOGI() << "Render success, frames: " << frames_to_render
             << " frameLength: " << render_buffer_.frameLength
             << " sleep_time_ms: " << sleep_time_ms;
    } else {
      LOGI() << "Render error: " << err << " frames: " << frames_to_render;
      // On error, reset sleep time to default
      sleep_time_ms = 5.0;
    }

    RTC_DCHECK(abl->mNumberBuffers == 1);
    const int16_t* rtc_buffer =
        static_cast<const int16_t*>(static_cast<const void*>(abl->mBuffers[0].mData));

    last_cycle_time = mach_absolute_time();

    fine_audio_buffer_->DeliverRecordedData(
        rtc::ArrayView<const int16_t>(rtc_buffer, frames_to_render), kFixedRecordDelayEstimate,
        absl::nullopt);

    if (!render_thread_->IsQuitting()) {
      render_thread_->SleepMs(static_cast<int>(sleep_time_ms));
    }
  }
}

bool AudioEngineDevice::EngineState::operator==(const EngineState& rhs) const {
  return input_enabled == rhs.input_enabled && output_enabled == rhs.output_enabled &&
         input_running == rhs.input_running && output_running == rhs.output_running &&
         input_muted == rhs.input_muted && is_interrupted == rhs.is_interrupted &&
         is_manual_mode == rhs.is_manual_mode && voice_processing == rhs.voice_processing &&
         advanced_ducking == rhs.advanced_ducking && ducking_level == rhs.ducking_level;
}

bool AudioEngineDevice::EngineState::operator!=(const EngineState& rhs) const {
  return !(*this == rhs);
}

// ----------------------------------------------------------------------------------------------------
// Private - Misc

AVAudioInputNode* AudioEngineDevice::InputNode() {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(audio_engine_ != nil);
  RTC_DCHECK(engine_state_.input_enabled);
  RTC_DCHECK(!engine_state_.is_manual_mode);

  return audio_engine_.inputNode;
}

AVAudioOutputNode* AudioEngineDevice::OutputNode() {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(audio_engine_ != nil);
  RTC_DCHECK(engine_state_.output_enabled || engine_state_.is_manual_mode);

  return audio_engine_.outputNode;
}

// ----------------------------------------------------------------------------------------------------
// Private - Debug

void AudioEngineDevice::DebugAudioEngine() {
  RTC_DCHECK_RUN_ON(thread_);

  auto padded_string = [](int pad) { return std::string(pad * 2, ' '); };

  auto audio_format = [](AVAudioFormat* format) {
    std::ostringstream result;

    // Get the underlying AudioStreamBasicDescription
    const AudioStreamBasicDescription& asbd = *format.streamDescription;

    result << "(";
    // Basic properties
    result << "sampleRate: " << format.sampleRate;
    result << ", channels: " << format.channelCount;
    result << ", bitsPerChannel: " << asbd.mBitsPerChannel;

    // Format ID (should be LinearPCM)
    result << ", formatID: ";
    char formatID[5] = {0};
    *(UInt32*)formatID = CFSwapInt32HostToBig(asbd.mFormatID);
    result << formatID;
    result << (asbd.mFormatID == kAudioFormatLinearPCM ? " (LinearPCM)" : " (Not LinearPCM)");

    // Format Flags
    result << std::hex << std::showbase;
    result << ", formatFlags: " << asbd.mFormatFlags;

    // Check specific flags
    bool isFloat = (asbd.mFormatFlags & kAudioFormatFlagIsFloat);
    bool isPacked = (asbd.mFormatFlags & kAudioFormatFlagIsPacked);
    bool isNonInterleaved = (asbd.mFormatFlags & kAudioFormatFlagIsNonInterleaved);
    bool isNativeEndian = (asbd.mFormatFlags & kAudioFormatFlagsNativeEndian);

    bool isAudioUnitCanonical = isNativeEndian && isFloat && isPacked && isNonInterleaved;

    result << std::dec;  // Switch back to decimal
    result << " [";
    result << "float:" << (isFloat ? "true" : "false") << ", ";
    result << "packed:" << (isPacked ? "true" : "false") << ", ";
    result << "non-interleaved:" << (isNonInterleaved ? "true" : "false") << ", ";
    result << "native-endian:" << (isNativeEndian ? "true" : "false") << ", ";
    result << "audio-unit-canonical:" << (isAudioUnitCanonical ? "true" : "false");
    result << "]";

    result << ")";
    return result.str();
  };

  std::function<void(AVAudioNode*, int)> print_node;
  print_node = [this, &padded_string, &audio_format](AVAudioNode* node, int base_depth = 0) {
    LOGI() << padded_string(base_depth) << NSStringFromClass([node class]).UTF8String << "."
           << node.hash;

    // Inputs
    for (NSUInteger i = 0; i < node.numberOfInputs; i++) {
      AVAudioFormat* format = [node inputFormatForBus:i];
      LOGI() << padded_string(base_depth) << " <- #" << i << audio_format(format);

      AVAudioConnectionPoint* connection = [this->audio_engine_ inputConnectionPointForNode:node
                                                                                   inputBus:i];
      if (connection != nil) {
        LOGI() << padded_string(base_depth + 1) << " <-> "
               << NSStringFromClass([connection.node class]).UTF8String << "."
               << connection.node.hash << " #" << connection.bus;
      }
    }

    // Outputs
    for (NSUInteger i = 0; i < node.numberOfOutputs; i++) {
      AVAudioFormat* format = [node outputFormatForBus:i];
      LOGI() << padded_string(base_depth) << " -> #" << i << audio_format(format);

      for (NSUInteger o = 0; o < node.numberOfOutputs; o++) {
        NSArray* points = [this->audio_engine_ outputConnectionPointsForNode:node outputBus:o];
        for (AVAudioConnectionPoint* connection in points) {
          LOGI() << padded_string(base_depth + 1) << " <-> "
                 << NSStringFromClass([connection.node class]).UTF8String << "."
                 << connection.node.hash << " #" << connection.bus;
        }
      }
    }
  };

  NSArray<AVAudioNode*>* attachedNodes = [audio_engine_.attachedNodes allObjects];
  LOGI() << "==================================================";
  LOGI() << "DebugAudioEngine attached nodes: " << attachedNodes.count;

  for (NSUInteger i = 0; i < attachedNodes.count; i++) {
    AVAudioNode* node = attachedNodes[i];
    print_node(node, 0);
  }

  LOGI() << "==================================================";
}

}  // namespace webrtc

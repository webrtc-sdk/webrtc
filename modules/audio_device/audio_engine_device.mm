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

const UInt16 kFixedPlayoutDelayEstimate = 0;
const UInt16 kFixedRecordDelayEstimate = 0;
const UInt16 kStartEngineMaxRetries = 10;  // Maximum blocking 1sec.
const useconds_t kStartEngineRetryDelayMs = 100;

const size_t kMaximumFramesPerBuffer = 3072;
const size_t kAudioSampleSize = 2;  // Signed 16-bit integer

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
  [session addDelegate:audio_session_observer_];
#endif

  // Add observer for configuration changes
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  configuration_observer_ = (__bridge_retained void*)[center
      addObserverForName:AVAudioEngineConfigurationChangeNotification
                  object:engine_device_
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

#if defined(WEBRTC_IOS)
  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  [session removeDelegate:audio_session_observer_];
  audio_session_observer_ = nil;
#endif
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

  return engine_state_.IsOutputEnabled();
}

bool AudioEngineDevice::Playing() const {
  LOGI() << "Playing";
  RTC_DCHECK_RUN_ON(thread_);

  return engine_state_.IsOutputRunning();
}

int32_t AudioEngineDevice::InitPlayout() {
  LOGI() << "InitPlayout";
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(initialized_);

  SetEngineState([](EngineState state) -> EngineState {
    state.output_enabled = true;
    return state;
  });

  return 0;
}

int32_t AudioEngineDevice::StartPlayout() {
  LOGI() << "StartPlayout";
  RTC_DCHECK_RUN_ON(thread_);

  SetEngineState([](EngineState state) -> EngineState {
    state.output_running = true;
    return state;
  });

  return 0;
}

int32_t AudioEngineDevice::StopPlayout() {
  LOGI() << "StopPlayout";
  RTC_DCHECK_RUN_ON(thread_);

  SetEngineState([](EngineState state) -> EngineState {
    state.output_enabled = false;
    state.output_running = false;
    return state;
  });

  return 0;
}

// ----------------------------------------------------------------------------------------------------
// Recording

bool AudioEngineDevice::RecordingIsInitialized() const {
  LOGI() << "RecordingIsInitialized";
  RTC_DCHECK_RUN_ON(thread_);

  return engine_state_.IsInputEnabled();
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

  SetEngineState([](EngineState state) -> EngineState {
    state.input_enabled = true;
    return state;
  });

  return 0;
}

int32_t AudioEngineDevice::StartRecording() {
  LOGI() << "StartRecording";
  RTC_DCHECK_RUN_ON(thread_);

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

  SetEngineState([](EngineState state) -> EngineState {
    state.input_enabled = false;
    state.input_running = false;
    return state;
  });

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

void AudioEngineDevice::OnInterruptionEnd(bool should_resume) {
  LOGI() << "OnInterruptionEnd should_resume: " << should_resume;

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

  *enabled = engine_state_.render_mode == RenderMode::Manual;

  return 0;
}

int32_t AudioEngineDevice::SetManualRenderingMode(bool enable) {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "SetManualRenderingMode: " << enable;

  SetEngineState([enable](EngineState state) -> EngineState {
    state.render_mode = enable ? RenderMode::Manual : RenderMode::Device;
    return state;
  });

  return 0;
}

int32_t AudioEngineDevice::InitAndStartRecording() {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "InitAndStartRecording";

  SetEngineState([](EngineState state) -> EngineState {
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

int32_t AudioEngineDevice::SetInitRecordingPersistentMode(bool enable) {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "SetInitRecordingPersistentMode: " << enable;

  SetEngineState([enable](EngineState state) -> EngineState {
    state.input_enabled_persistent_mode = enable;
    return state;
  });

  return 0;
}

int32_t AudioEngineDevice::InitRecordingPersistentMode(bool* enabled) {
  LOGI() << "InitRecordingPersistentMode";
  RTC_DCHECK_RUN_ON(thread_);

  if (enabled == nullptr) {
    return -1;
  }

  *enabled = engine_state_.input_enabled_persistent_mode;
  LOGI() << "InitRecordingPersistentMode value: " << *enabled;

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
  EngineStateUpdate state = {old_state, new_state};

  if (state.HasNoChanges()) {
    LOGI() << "SetEngineState: Nothing to state";
    return;
  }

  // Check input should be enabled if running.
  if (new_state.IsInputRunning()) {
    RTC_DCHECK(new_state.IsInputEnabled());
  }

  // Check output should be enabled if running.
  if (new_state.IsOutputRunning()) {
    RTC_DCHECK(new_state.IsOutputEnabled());
  }

  // Save new state
  engine_state_ = new_state;

  // Did switch Device -> Manual rendering
  if (state.DidEnableManualRenderingMode()) {
    EngineStateUpdate shutdown_state = state;  // Copy current state
    shutdown_state.next = {};                  // Reset next state to default
    UpdateDeviceEngineState(shutdown_state);   // Shutdown device rendering
    EngineStateUpdate startup_state = state;   // Copy current state
    shutdown_state.prev = {};                  //
    UpdateManualEngineState(startup_state);    // Start manual mode
  } else if (state.DidEnableDeviceRenderingMode()) {
    EngineStateUpdate shutdown_state = state;
    shutdown_state.next = {};                 // Reset next state to default
    UpdateManualEngineState(shutdown_state);  // Shutdown manual rendering
    EngineStateUpdate startup_state = state;  // Copy current state
    shutdown_state.prev = {};                 //
    UpdateDeviceEngineState(startup_state);   // Start device mode
  } else if (new_state.render_mode == RenderMode::Device) {
    UpdateDeviceEngineState(state);
  } else if (new_state.render_mode == RenderMode::Manual) {
    UpdateManualEngineState(state);
  }

  // Buffer should be playing if output is running.
  if (new_state.IsOutputEnabled()) {
    RTC_DCHECK(audio_device_buffer_->IsPlaying());
  } else {
    RTC_DCHECK(!audio_device_buffer_->IsPlaying());
  }

  // Buffer should be recording if input is running.
  if (new_state.IsInputEnabled()) {
    RTC_DCHECK(audio_device_buffer_->IsRecording());
  } else {
    RTC_DCHECK(!audio_device_buffer_->IsRecording());
  }
}

void AudioEngineDevice::UpdateManualEngineState(EngineStateUpdate state) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(engine_device_ == nullptr);

  if (state.prev.IsAnyRunning() && !state.next.IsAnyRunning()) {
    LOGI() << "Stopping AVAudioEngine...";
    RTC_DCHECK(engine_manual_input_ != nil);
    [engine_manual_input_ stop];

    LOGI() << "Stopping render thread...";
    RTC_DCHECK(render_thread_ != nullptr);
    render_thread_->Stop();
    render_thread_ = nullptr;

    LOGI() << "Releasing render buffer...";
    RTC_DCHECK(render_buffer_ != nullptr);
    render_buffer_ = nullptr;

    if (observer_ != nullptr) {
      observer_->OnEngineDidStop(engine_manual_input_, state.next.IsOutputEnabled(),
                                 state.next.IsInputEnabled());
    }
  }

  if (state.next.IsAnyEnabled() && !state.prev.IsAnyEnabled()) {
    LOGI() << "Creating AVAudioEngine (manual)...";
    RTC_DCHECK(engine_manual_input_ == nullptr);
    engine_manual_input_ = [[AVAudioEngine alloc] init];

    NSError* error = nil;
    BOOL result =
        [engine_manual_input_ enableManualRenderingMode:AVAudioEngineManualRenderingModeRealtime
                                                 format:manual_render_rtc_format_
                                      maximumFrameCount:kMaximumFramesPerBuffer
                                                  error:&error];
    if (!result) {
      LOGE() << "Failed to set manual rendering mode: " << error.localizedDescription.UTF8String;
    }

    if (observer_ != nullptr) {
      observer_->OnEngineDidCreate(engine_manual_input_);
    }
  }

  if (!state.next.IsOutputEnabled() && audio_device_buffer_->IsPlaying()) {
    LOGI() << "Stopping Playout buffer...";
    if (engine_device_ != nullptr) {
      // Rendering must be stopped first.
      RTC_DCHECK(!engine_device_.running);
    }
    audio_device_buffer_->StopPlayout();
  }

  if (!state.next.IsInputEnabled() && audio_device_buffer_->IsRecording()) {
    LOGI() << "Stopping Record buffer...";
    if (engine_device_ != nullptr) {
      // Rendering must be stopped first.
      RTC_DCHECK(!engine_device_.running);
    }
    audio_device_buffer_->StopRecording();
  }

  if (state.DidAnyEnable() && observer_ != nullptr) {
    // Invoke here before configuring nodes. In iOS, session configuration is required before
    // enabling AGC, muted talker etc.
    observer_->OnEngineWillEnable(engine_manual_input_, state.next.IsOutputEnabled(),
                                  state.next.IsInputEnabled());
  }

  if (state.next.IsOutputEnabled() && !state.prev.IsOutputEnabled()) {
    LOGI() << "Enabling output for AVAudioEngine...";
    RTC_DCHECK(!engine_manual_input_.running);

    audio_device_buffer_->SetPlayoutSampleRate(manual_render_rtc_format_.sampleRate);
    audio_device_buffer_->SetPlayoutChannels(manual_render_rtc_format_.channelCount);
    RTC_DCHECK(audio_device_buffer_ != nullptr);
    fine_audio_buffer_.reset(new FineAudioBuffer(audio_device_buffer_.get()));

  } else if (state.prev.IsOutputEnabled() && !state.next.IsOutputEnabled()) {
    LOGI() << "Disabling output for AVAudioEngine...";
    RTC_DCHECK(!engine_manual_input_.running);
  }

  if (state.next.IsInputEnabled() && !state.prev.IsInputEnabled()) {
    LOGI() << "Enabling input for AVAudioEngine...";
    RTC_DCHECK(!engine_manual_input_.running);

    audio_device_buffer_->SetRecordingSampleRate(manual_render_rtc_format_.sampleRate);
    audio_device_buffer_->SetRecordingChannels(manual_render_rtc_format_.channelCount);
    RTC_DCHECK(audio_device_buffer_ != nullptr);
    fine_audio_buffer_.reset(new FineAudioBuffer(audio_device_buffer_.get()));

    if (!(this->observer_ != nullptr &&
          this->observer_->OnEngineWillConnectOutput(
              engine_manual_input_, engine_manual_input_.mainMixerNode, this->OutputNode(),
              manual_render_rtc_format_))) {
      // Default implementation.
      [engine_manual_input_ connect:engine_manual_input_.mainMixerNode
                                 to:this->OutputNode()
                             format:manual_render_rtc_format_];
    }

  } else if (state.prev.IsInputEnabled() && !state.next.IsInputEnabled()) {
    LOGI() << "Disabling input for AVAudioEngine...";
    RTC_DCHECK(!engine_manual_input_.running);
  }

  if (state.DidAnyDisable() && observer_ != nullptr) {
    observer_->OnEngineDidDisable(engine_manual_input_, state.next.IsOutputEnabled(),
                                  state.next.IsInputEnabled());
  }

  // Start playout buffer if output is running
  if (state.next.IsOutputEnabled() && !audio_device_buffer_->IsPlaying()) {
    if (engine_device_ != nullptr) {
      // Rendering must be stopped first.
      RTC_DCHECK(!engine_device_.running);
    }
    LOGI() << "Starting Playout buffer...";
    audio_device_buffer_->StartPlayout();
    fine_audio_buffer_->ResetPlayout();
  }

  // Start recording buffer if input is running
  if (state.next.IsInputEnabled() && !audio_device_buffer_->IsRecording()) {
    if (engine_device_ != nullptr) {
      // Rendering must be stopped first.
      RTC_DCHECK(!engine_device_.running);
    }
    LOGI() << "Starting Record buffer...";
    audio_device_buffer_->StartRecording();
    fine_audio_buffer_->ResetRecord();
  }

  if (state.next.IsAnyRunning() && !state.prev.IsAnyRunning()) {
    if (observer_ != nullptr) {
      observer_->OnEngineWillStart(engine_manual_input_, state.next.IsOutputEnabled(),
                                   state.next.IsInputEnabled());
    }

    LOGI() << "Allocating render buffer...";
    RTC_DCHECK(render_buffer_ == nullptr);
    render_buffer_ = [[AVAudioPCMBuffer alloc] initWithPCMFormat:manual_render_rtc_format_
                                                   frameCapacity:kMaximumFramesPerBuffer];

    LOGI() << "Starting AVAudioEngine...";
    NSError* error = nil;

    BOOL start_result = [engine_manual_input_ startAndReturnError:&error];
    if (!start_result) {
      LOGE() << "Failed to start engine after " << kStartEngineMaxRetries << " attempts";
      DebugAudioEngine();
    }

    // Assign manual rendering block
    render_block_ = engine_manual_input_.manualRenderingBlock;
    RTC_DCHECK(render_block_ != nullptr);

    // Create render thread
    LOGI() << "Starting render thread...";
    RTC_DCHECK(render_thread_ == nullptr);
    render_thread_ = rtc::Thread::Create();
    render_thread_->SetName("render_thread", nullptr);
    render_thread_->Start();
    render_thread_->PostTask([this] {
      // RTC_DCHECK_RUN_ON(thread_);
      this->StartRenderLoop();
    });
  }

  if (state.prev.IsAnyEnabled() && !state.next.IsAnyEnabled()) {
    if (observer_ != nullptr) {
      observer_->OnEngineWillRelease(engine_manual_input_);
    }
    LOGI() << "Releasing AVAudioEngine...";
    engine_manual_input_ = nil;
  }
}

void AudioEngineDevice::UpdateDeviceEngineState(EngineStateUpdate state) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(engine_manual_input_ == nullptr);

  if (state.prev.IsAnyRunning() &&
      (!state.next.IsAnyRunning() || state.DidUpdateAudioGraph() || state.DidBeginInterruption() ||
       state.IsEngineRecreateRequired())) {
    LOGI() << "Stopping AVAudioEngine...";
    RTC_DCHECK(engine_device_ != nil);
    [engine_device_ stop];

    if (observer_ != nullptr) {
      observer_->OnEngineDidStop(engine_device_, state.next.IsOutputEnabled(),
                                 state.next.IsInputEnabled());
    }
  }

  if (state.IsEngineRecreateRequired()) {
    LOGI() << "Recreate required, releasing AVAudioEngine...";
    if (observer_ != nullptr) {
      observer_->OnEngineWillRelease(engine_device_);
    }
    engine_device_ = nil;
  }

  if (state.next.IsAnyEnabled() &&
      (!state.prev.IsAnyEnabled() || state.IsEngineRecreateRequired())) {
    LOGI() << "Creating AVAudioEngine (device)...";
    RTC_DCHECK(engine_device_ == nullptr);
    engine_device_ = [[AVAudioEngine alloc] init];

    if (observer_ != nullptr) {
      observer_->OnEngineDidCreate(engine_device_);
    }
  }

  if (!state.next.IsOutputEnabled() && audio_device_buffer_->IsPlaying()) {
    LOGI() << "Stopping Playout buffer...";
    if (engine_device_ != nullptr) {
      // Rendering must be stopped first.
      RTC_DCHECK(!engine_device_.running);
    }
    audio_device_buffer_->StopPlayout();
  }

  if (!state.next.IsInputEnabled() && audio_device_buffer_->IsRecording()) {
    LOGI() << "Stopping Record buffer...";
    if (engine_device_ != nullptr) {
      // Rendering must be stopped first.
      RTC_DCHECK(!engine_device_.running);
    }
    audio_device_buffer_->StopRecording();
  }

  if (state.DidAnyEnable() && observer_ != nullptr) {
    // Invoke here before configuring nodes. In iOS, session configuration is required before
    // enabling AGC, muted talker etc.
    observer_->OnEngineWillEnable(engine_device_, state.next.IsOutputEnabled(),
                                  state.next.IsInputEnabled());
  }

  if (state.next.IsOutputEnabled() &&
      (!state.prev.IsOutputEnabled() || state.IsEngineRecreateRequired())) {
    LOGI() << "Enabling output for AVAudioEngine...";
    RTC_DCHECK(!engine_device_.running);

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
    [engine_device_ attachNode:source_node_];

    [engine_device_ connect:source_node_
                         to:engine_device_.mainMixerNode
                     format:engine_output_format];

    if (!(this->observer_ != nullptr &&
          this->observer_->OnEngineWillConnectOutput(engine_device_, engine_device_.mainMixerNode,
                                                     this->OutputNode(), engine_output_format))) {
      // Default implementation.
      [engine_device_ connect:engine_device_.mainMixerNode
                           to:this->OutputNode()
                       format:engine_output_format];
    }

  } else if ((state.prev.IsOutputEnabled() && !state.next.IsOutputEnabled()) &&
             !state.IsEngineRecreateRequired()) {
    LOGI() << "Disabling output for AVAudioEngine...";
    RTC_DCHECK(!engine_device_.running);

    // Disconnect
    if (source_node_ != nil) {
      [engine_device_ disconnectNodeInput:source_node_];
      [engine_device_ disconnectNodeOutput:source_node_];
      [engine_device_ detachNode:source_node_];
      source_node_ = nil;
    }
  }

  if (state.next.IsInputEnabled() &&
      (!state.prev.IsInputEnabled() || state.IsEngineRecreateRequired())) {
    LOGI() << "Enabling input for AVAudioEngine...";
    RTC_DCHECK(!engine_device_.running);

    if (!this->InputNode().voiceProcessingEnabled) {
      NSError* error = nil;
      BOOL set_vp_result = [this->InputNode() setVoiceProcessingEnabled:YES error:&error];
      if (!set_vp_result) {
        NSLog(@"AudioEngineDevice setVoiceProcessingEnabled error: %@", error.localizedDescription);
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

    input_mixer_node_ = [[AVAudioMixerNode alloc] init];
    [engine_device_ attachNode:input_mixer_node_];

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
          observer_->OnEngineWillConnectInput(engine_device_, this->InputNode(), input_mixer_node_,
                                              engine_input_format))) {
      // Default implementation.
      [engine_device_ connect:this->InputNode() to:input_mixer_node_ format:engine_input_format];
    }

    sink_node_ = [[AVAudioSinkNode alloc] initWithReceiverBlock:sink_block];
    [engine_device_ attachNode:sink_node_];

    // Convert to RTC's internal format before passing buffers to SinkNode.
    [engine_device_ connect:input_mixer_node_ to:sink_node_ format:rtc_input_format];

  } else if ((state.prev.IsInputEnabled() && !state.next.IsInputEnabled()) &&
             !state.IsEngineRecreateRequired()) {
    LOGI() << "Disabling input for AVAudioEngine...";
    RTC_DCHECK(!engine_device_.running);

    // InputMixerNode
    if (input_mixer_node_ != nil) {
      [engine_device_ disconnectNodeInput:input_mixer_node_];
      [engine_device_ disconnectNodeOutput:input_mixer_node_];
      [engine_device_ detachNode:input_mixer_node_];
      input_mixer_node_ = nil;
    }

    // SinkNode
    if (sink_node_ != nil) {
      [engine_device_ disconnectNodeInput:sink_node_];
      [engine_device_ disconnectNodeOutput:sink_node_];
      [engine_device_ detachNode:sink_node_];
      sink_node_ = nil;
    }
  }

  if (state.DidAnyDisable() && observer_ != nullptr) {
    observer_->OnEngineDidDisable(engine_device_, state.next.IsOutputEnabled(),
                                  state.next.IsInputEnabled());
  }

  if (state.next.IsInputEnabled() && this->InputNode().voiceProcessingEnabled &&
      this->InputNode().voiceProcessingInputMuted != state.next.input_muted) {
    LOGI() << "setVoiceProcessingInputMuted: " << state.next.input_muted;
    this->InputNode().voiceProcessingInputMuted = state.next.input_muted;
  }

#if !TARGET_OS_TV
  if (state.next.IsInputEnabled() && this->InputNode().voiceProcessingEnabled &&
      (!state.prev.IsInputEnabled() ||
       (state.prev.advanced_ducking != state.next.advanced_ducking ||
        state.prev.ducking_level != state.next.ducking_level))) {
    // Other audio ducking.
    // iOS 17.0+, iPadOS 17.0+, Mac Catalyst 17.0+, macOS 14.0+, visionOS 1.0+
    if (@available(iOS 17.0, macCatalyst 17.0, macOS 14.0, visionOS 1.0, *)) {
      AVAudioVoiceProcessingOtherAudioDuckingConfiguration ducking_config;
      ducking_config.enableAdvancedDucking = state.next.advanced_ducking;
      ducking_config.duckingLevel =
          (AVAudioVoiceProcessingOtherAudioDuckingLevel)state.next.ducking_level;

      LOGI() << "setVoiceProcessingOtherAudioDuckingConfiguration";
      this->InputNode().voiceProcessingOtherAudioDuckingConfiguration = ducking_config;
    }
  }
#endif

  // Start playout buffer if output is running
  if (state.next.IsOutputEnabled() && !audio_device_buffer_->IsPlaying()) {
    if (engine_device_ != nullptr) {
      // Rendering must be stopped first.
      RTC_DCHECK(!engine_device_.running);
    }
    LOGI() << "Starting Playout buffer...";
    audio_device_buffer_->StartPlayout();
    fine_audio_buffer_->ResetPlayout();
  }

  // Start recording buffer if input is running
  if (state.next.IsInputEnabled() && !audio_device_buffer_->IsRecording()) {
    if (engine_device_ != nullptr) {
      // Rendering must be stopped first.
      RTC_DCHECK(!engine_device_.running);
    }
    LOGI() << "Starting Record buffer...";
    audio_device_buffer_->StartRecording();
    fine_audio_buffer_->ResetRecord();
  }

  if (state.next.IsAnyRunning()) {
    if (!state.prev.IsAnyRunning() || state.DidEndInterruption() || state.DidUpdateAudioGraph() ||
        state.IsEngineRecreateRequired()) {
      if (observer_ != nullptr) {
        observer_->OnEngineWillStart(engine_device_, state.next.IsOutputEnabled(),
                                     state.next.IsInputEnabled());
      }

      LOGI() << "Starting AVAudioEngine...";
      NSError* error = nil;
      BOOL start_result = false;
      int start_retry_count = 0;

      // Workaround for error -66637, when recovering from interruptions with categoryMode:
      // .mixWithOthers.
      while (!start_result && start_retry_count < kStartEngineMaxRetries) {
        if (start_retry_count > 0) {
          LOGW() << "Retrying engine start (attempt " << start_retry_count + 1 << "/"
                 << kStartEngineMaxRetries << ")";
          usleep(kStartEngineRetryDelayMs * 1000);
        }

        start_result = [engine_device_ startAndReturnError:&error];
        if (!start_result) {
          LOGE() << "Failed to start engine: " << error.localizedDescription.UTF8String;
          start_retry_count++;
        }
      }

      if (!start_result) {
        LOGE() << "Failed to start engine after " << kStartEngineMaxRetries << " attempts";
        DebugAudioEngine();
      }
    }
  }

  if (state.prev.IsAnyEnabled() && !state.next.IsAnyEnabled()) {
    if (observer_ != nullptr) {
      observer_->OnEngineWillRelease(engine_device_);
    }
    LOGI() << "Releasing AVAudioEngine...";
    engine_device_ = nil;
  }
}

// ----------------------------------------------------------------------------------------------------
// Private - EngineState

void AudioEngineDevice::StartRenderLoop() {
  RTC_DCHECK_RUN_ON(render_thread_.get());

  const double sample_rate = manual_render_rtc_format_.sampleRate;
  const size_t frames_per_buffer = static_cast<size_t>(sample_rate / 100);  // 10ms chunks
  const size_t buffer_size = frames_per_buffer * kAudioSampleSize;
  const int sleep_ms = 5;  // Fixed sleep time

  while (!render_thread_->IsQuitting()) {
    RTC_DCHECK(render_buffer_ != nullptr);
    AudioBufferList* abl = const_cast<AudioBufferList*>(render_buffer_.audioBufferList);
    abl->mBuffers[0].mDataByteSize = buffer_size;

    OSStatus err = noErr;
    AVAudioEngineManualRenderingStatus result = render_block_(frames_per_buffer, abl, &err);

    if (result == AVAudioEngineManualRenderingStatusSuccess) {
      RTC_DCHECK(abl->mNumberBuffers == 1);
      const int16_t* rtc_buffer =
          static_cast<const int16_t*>(static_cast<const void*>(abl->mBuffers[0].mData));

      const uint64_t capture_time = mach_absolute_time();
      const int64_t capture_time_ns = capture_time * machTickUnitsToNanoseconds_;

      fine_audio_buffer_->DeliverRecordedData(
          rtc::ArrayView<const int16_t>(rtc_buffer, frames_per_buffer), kFixedRecordDelayEstimate,
          capture_time_ns);
    } else {
      LOGW() << "Render error: " << err << " frames: " << frames_per_buffer;
    }

    if (!render_thread_->IsQuitting()) {
      render_thread_->SleepMs(sleep_ms);
    }
  }
}

// ----------------------------------------------------------------------------------------------------
// Private - Misc

AVAudioInputNode* AudioEngineDevice::InputNode() {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(engine_device_ != nil);
  RTC_DCHECK(engine_state_.IsInputEnabled());
  RTC_DCHECK(engine_state_.render_mode == RenderMode::Device);

  return engine_device_.inputNode;
}

AVAudioOutputNode* AudioEngineDevice::OutputNode() {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(engine_state_.IsOutputEnabled());

  if (engine_state_.render_mode == RenderMode::Manual) {
    RTC_DCHECK(engine_manual_input_ != nil);
    return engine_manual_input_.outputNode;
  } else {
    RTC_DCHECK(engine_device_ != nil);
    return engine_device_.outputNode;
  }
}

// ----------------------------------------------------------------------------------------------------
// Private - Debug

void AudioEngineDevice::DebugAudioEngine() {
  RTC_DCHECK_RUN_ON(thread_);

#if TARGET_OS_IOS
  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  RTCLog(@"RTCAudioSession %@", session);
#endif

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

      AVAudioConnectionPoint* connection = [this->engine_device_ inputConnectionPointForNode:node
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
        NSArray* points = [this->engine_device_ outputConnectionPointsForNode:node outputBus:o];
        for (AVAudioConnectionPoint* connection in points) {
          LOGI() << padded_string(base_depth + 1) << " <-> "
                 << NSStringFromClass([connection.node class]).UTF8String << "."
                 << connection.node.hash << " #" << connection.bus;
        }
      }
    }
  };

  NSArray<AVAudioNode*>* attachedNodes = [engine_device_.attachedNodes allObjects];
  LOGI() << "==================================================";
  LOGI() << "DebugAudioEngine attached nodes: " << attachedNodes.count;

  for (NSUInteger i = 0; i < attachedNodes.count; i++) {
    AVAudioNode* node = attachedNodes[i];
    print_node(node, 0);
  }

  LOGI() << "==================================================";
}

}  // namespace webrtc

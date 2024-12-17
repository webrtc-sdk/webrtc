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

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#include "audio_engine_device.h"

#include <mach/mach_time.h>
#include <cmath>

#include "api/array_view.h"
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

const uint32_t N_REC_SAMPLES_PER_SEC = 48000;
const uint32_t N_PLAY_SAMPLES_PER_SEC = 48000;
const uint32_t N_REC_CHANNELS = 1;   // default is mono recording
const uint32_t N_PLAY_CHANNELS = 1;  // default is stereo playout

AudioEngineDevice::AudioEngineDevice(bool bypass_voice_processing)
    : bypass_voice_processing_(bypass_voice_processing),
      audio_device_buffer_(nullptr),
      initialized_(false),
      has_configured_session_(false) {
  LOGI() << "bypass_voice_processing " << bypass_voice_processing_;

  io_thread_checker_.Detach();
  thread_ = rtc::Thread::Current();

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

void AudioEngineDevice::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  LOGI() << "AttachAudioBuffer";
  RTC_DCHECK(audioBuffer);
  RTC_DCHECK_RUN_ON(thread_);
  audio_device_buffer_ = audioBuffer;

  // Fixes values for mac.
  audio_device_buffer_->SetRecordingSampleRate(N_REC_SAMPLES_PER_SEC);
  audio_device_buffer_->SetPlayoutSampleRate(N_PLAY_SAMPLES_PER_SEC);
  audio_device_buffer_->SetRecordingChannels(N_REC_CHANNELS);
  audio_device_buffer_->SetPlayoutChannels(N_PLAY_CHANNELS);

  fine_audio_buffer_.reset(new FineAudioBuffer(audio_device_buffer_));
}

// MARK: - Main life cycle

bool AudioEngineDevice::Initialized() const {
  LOGI() << "Initialized";
  RTC_DCHECK_RUN_ON(thread_);

  return initialized_;
}

AudioDeviceGeneric::InitStatus AudioEngineDevice::Init() {
  LOGI() << "Init";
  io_thread_checker_.Detach();

  RTC_DCHECK_RUN_ON(thread_);
  if (initialized_) {
    return InitStatus::OK;
  }

#if defined(WEBRTC_IOS)
  RTC_OBJC_TYPE(RTCAudioSessionConfiguration)* config =
      [RTC_OBJC_TYPE(RTCAudioSessionConfiguration) webRTCConfiguration];
  playout_parameters_.reset(config.sampleRate, config.outputNumberOfChannels);
  record_parameters_.reset(config.sampleRate, config.inputNumberOfChannels);
#endif

  rtc_internal_format_ = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16
                                                          sampleRate:48000.0
                                                            channels:1
                                                         interleaved:YES];

  initialized_ = true;

  return InitStatus::OK;
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
  RTC_DCHECK(!engine_state_.output_enabled);
  RTC_DCHECK(!engine_state_.output_running);

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
  RTC_DCHECK(!engine_state_.output_running);

  if (!engine_state_.output_enabled) {
    LOGW() << "StartPlayout: Not initialized";
    return -1;
  }

  if (engine_state_.output_running) {
    LOGW() << "StartPlayout: Already playing";
    return 0;
  }

  if (fine_audio_buffer_) {
    fine_audio_buffer_->ResetPlayout();
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
  RTC_DCHECK(!engine_state_.input_enabled);
  RTC_DCHECK(!engine_state_.input_running);

  if (engine_state_.input_enabled) {
    LOGW() << "InitRecording: Already initialized";
    return 0;
  }

  SetEngineState([](EngineState state) -> EngineState {
    state.input_enabled = true;
    return state;
  });

  return 0;
}

int32_t AudioEngineDevice::StartRecording() {
  LOGI() << "StartRecording";
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(engine_state_.input_enabled);
  RTC_DCHECK(!engine_state_.input_running);

  if (!engine_state_.input_enabled) {
    LOGW() << "StartRecording: Not initialized";
    return -1;
  }

  if (engine_state_.input_running) {
    LOGW() << "StartRecording: Already recording";
    return 0;
  }

  if (fine_audio_buffer_) {
    fine_audio_buffer_->ResetRecord();
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

int32_t AudioEngineDevice::ActiveAudioLayer(AudioDeviceModule::AudioLayer& audioLayer) const {
  LOGI() << "ActiveAudioLayer";
  audioLayer = AudioDeviceModule::kPlatformDefaultAudio;

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

int32_t AudioEngineDevice::SpeakerVolumeIsAvailable(bool& available) {
  LOGI() << "SpeakerVolumeIsAvailable";
  available = false;

  return 0;
}

int32_t AudioEngineDevice::SetSpeakerVolume(uint32_t volume) {
  LOGW() << "SetSpeakerVolume: Not implemented, value: " << volume;

  return -1;
}

int32_t AudioEngineDevice::SpeakerVolume(uint32_t& volume) const {
  LOGW() << "SpeakerVolume: Not implemented";

  return -1;
}

int32_t AudioEngineDevice::MaxSpeakerVolume(uint32_t& maxVolume) const {
  LOGW() << "MaxSpeakerVolume: Not implemented";

  return -1;
}

int32_t AudioEngineDevice::MinSpeakerVolume(uint32_t& minVolume) const {
  LOGW() << "MinSpeakerVolume: Not implemented";

  return -1;
}

int32_t AudioEngineDevice::SpeakerMuteIsAvailable(bool& available) {
  LOGI() << "SpeakerMuteIsAvailable";
  available = false;

  return 0;
}

int32_t AudioEngineDevice::SetSpeakerMute(bool enable) {
  LOGI() << "SetSpeakerMute: " << enable;

  return -1;
}

int32_t AudioEngineDevice::SpeakerMute(bool& enabled) const {
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

int32_t AudioEngineDevice::MicrophoneMuteIsAvailable(bool& available) {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "MicrophoneMuteIsAvailable";
  available = true;
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

int32_t AudioEngineDevice::MicrophoneMute(bool& enabled) const {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "MicrophoneMute";

  enabled = engine_state_.input_muted;

  return 0;
}

// ----------------------------------------------------------------------------------------------------
// Stereo Playout

int32_t AudioEngineDevice::StereoPlayoutIsAvailable(bool& available) {
  LOGI() << "StereoPlayoutIsAvailable";
  available = false;

  return 0;
}

int32_t AudioEngineDevice::SetStereoPlayout(bool enable) {
  LOGW() << "SetStereoPlayout: Not implemented, value:" << enable;

  return -1;
}

int32_t AudioEngineDevice::StereoPlayout(bool& enabled) const {
  LOGI() << "StereoPlayout";
  enabled = false;

  return 0;
}

// ----------------------------------------------------------------------------------------------------
// Stereo Recording

int32_t AudioEngineDevice::StereoRecordingIsAvailable(bool& available) {
  LOGI() << "StereoPlayoutIsAvailable";
  available = false;

  return 0;
}

int32_t AudioEngineDevice::SetStereoRecording(bool enable) {
  LOGW() << "SetStereoRecording: Not implemented, value: " << enable;

  return -1;
}

int32_t AudioEngineDevice::StereoRecording(bool& enabled) const {
  LOGI() << "StereoRecording";
  enabled = false;

  return 0;
}

// ----------------------------------------------------------------------------------------------------
// Microphone Volume

int32_t AudioEngineDevice::MicrophoneVolumeIsAvailable(bool& available) {
  LOGI() << "MicrophoneVolumeIsAvailable";
  available = false;

  return 0;
}

int32_t AudioEngineDevice::SetMicrophoneVolume(uint32_t volume) {
  LOGW() << "SetMicrophoneVolume: Not implemented, value: " << volume;

  return -1;
}

int32_t AudioEngineDevice::MicrophoneVolume(uint32_t& volume) const {
  LOGW() << "SetMicrophoneVolume: Not implemented";

  return -1;
}

int32_t AudioEngineDevice::MaxMicrophoneVolume(uint32_t& maxVolume) const {
  LOGW() << "SetMicrophoneVolume: Not implemented";

  return -1;
}

int32_t AudioEngineDevice::MinMicrophoneVolume(uint32_t& minVolume) const {
  LOGW() << "MinMicrophoneVolume: Not implemented";

  return -1;
}

// ----------------------------------------------------------------------------------------------------
// Playout Device

int32_t AudioEngineDevice::PlayoutIsAvailable(bool& available) {
  LOGI() << "PlayoutIsAvailable";
  available = true;

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
  LOGW() << "PlayoutDeviceName: Not implemented";

  return -1;
}

int16_t AudioEngineDevice::PlayoutDevices() {
  LOGI() << "PlayoutDevices";

  return (int16_t)1;
}

// ----------------------------------------------------------------------------------------------------
// Recording Device

int32_t AudioEngineDevice::RecordingDeviceName(uint16_t index, char name[kAdmMaxDeviceNameSize],
                                               char guid[kAdmMaxGuidSize]) {
  LOGW() << "RecordingDeviceName";

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

int32_t AudioEngineDevice::RecordingIsAvailable(bool& available) {
  LOGI() << "RecordingIsAvailable";

  available = true;
  return 0;
}

int16_t AudioEngineDevice::RecordingDevices() {
  LOGI() << "RecordingDevices";

  return (int16_t)1;
}

// ----------------------------------------------------------------------------------------------------
// Misc

int32_t AudioEngineDevice::PlayoutDelay(uint16_t& delayMS) const {
  delayMS = kFixedPlayoutDelayEstimate;
  return 0;
}

int32_t AudioEngineDevice::SetObserver(AudioDeviceObserver* observer) {
  LOGI() << "SetObserver";
  RTC_DCHECK_RUN_ON(thread_);

  observer_ = observer;
  return 0;
}

// ----------------------------------------------------------------------------------------------------
// Private - Engine Related

void AudioEngineDevice::OnEngineConfigurationChange() {
  LOGI() << "OnEngineConfigurationChange";

  thread_->PostTask(SafeTask(safety_, [this] {
    RTC_DCHECK_RUN_ON(thread_);

    EngineState previous_state = this->engine_state_;

    this->SetEngineState([](EngineState state) -> EngineState {
      return EngineState();  // Return default state to shutdown
    });

    this->SetEngineState([previous_state](EngineState state) -> EngineState {
      return previous_state;  // Recover engine state
    });
  }));
}

bool AudioEngineDevice::IsMicrophonePermissionGranted() {
  AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
  return status == AVAuthorizationStatusAuthorized;
}

void AudioEngineDevice::SetEngineState(std::function<EngineState(EngineState)> state_transform) {
  RTC_DCHECK_RUN_ON(thread_);

  EngineState old_state = engine_state_;
  EngineState new_state = state_transform(old_state);

#if defined(WEBRTC_IOS)
  if ((!old_state.output_enabled && new_state.output_enabled) ||
      (!old_state.input_enabled && new_state.input_enabled)) {
    RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
    [session lockForConfiguration];
    [session notifyAudioEngineWillUpdateStateWithOutputEnabled:new_state.output_enabled
                                                  inputEnabled:new_state.input_enabled];
    ConfigureAudioSessionLocked();
    [session unlockForConfiguration];
  }

  if (!old_state.output_enabled && new_state.output_enabled) {
    RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
    bool is_category_record = session.category == AVAudioSessionCategoryPlayAndRecord ||
                              session.category == AVAudioSessionCategoryRecord;

    // Already enable input if mic perms are already granted.
    if (!new_state.input_enabled && is_category_record) {
      EngineState update_state = new_state;
      update_state.input_enabled = true;
      update_state.input_muted = true;
      new_state = update_state;
    }
  }
#endif

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

  UpdateEngineState(old_state, new_state);
  engine_state_ = new_state;
}

void AudioEngineDevice::UpdateEngineState(EngineState old_state, EngineState new_state) {
  RTC_DCHECK_RUN_ON(thread_);

  if (!old_state.IsAnyEnabled() && new_state.IsAnyEnabled()) {
    LOGI() << "Creating AVAudioEngine...";
    audio_engine_ = [[AVAudioEngine alloc] init];
  }

  bool did_change_audio_graph = (old_state.input_enabled != new_state.input_enabled) ||
                                (old_state.output_enabled != new_state.output_enabled);

  if (old_state.IsAnyRunning()) {
    if (!new_state.IsAnyRunning() || did_change_audio_graph) {
      LOGI() << "Stopping AVAudioEngine...";
      [audio_engine_ stop];
    } else if (!old_state.is_interrupted && new_state.is_interrupted) {
      LOGI() << "Pausing AVAudioEngine...";
      [audio_engine_ pause];
    }
  }

  if (!old_state.output_enabled && new_state.output_enabled) {
    LOGI() << "Enabling output for AVAudioEngine...";
    RTC_DCHECK(!audio_engine_.running);

    AVAudioFormat* output_format = [audio_engine_.outputNode outputFormatForBus:0];

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

    source_node_ = [[AVAudioSourceNode alloc] initWithFormat:rtc_internal_format_
                                                 renderBlock:source_block];

    [audio_engine_ attachNode:source_node_];

    [audio_engine_ connect:source_node_ to:audio_engine_.mainMixerNode format:output_format];

    [audio_engine_ connect:audio_engine_.mainMixerNode
                        to:audio_engine_.outputNode
                    format:output_format];

  } else if (old_state.output_enabled && !new_state.output_enabled) {
    LOGI() << "Disabling output for AVAudioEngine...";
    RTC_DCHECK(!audio_engine_.running);

    // Disconnect
    [audio_engine_ disconnectNodeInput:source_node_];
    [audio_engine_ disconnectNodeOutput:source_node_];
    // Detach
    [audio_engine_ detachNode:source_node_];
    // Release
    source_node_ = nil;
  }

  if (!old_state.input_enabled && new_state.input_enabled) {
    LOGI() << "Enabling input for AVAudioEngine...";
    RTC_DCHECK(!audio_engine_.running);

    AVAudioFormat* input_format = [audio_engine_.inputNode outputFormatForBus:0];

    input_eq_node_ = [[AVAudioUnitEQ alloc] initWithNumberOfBands:2];
    [audio_engine_ attachNode:input_eq_node_];

    input_mixer_node_ = [[AVAudioMixerNode alloc] init];
    [audio_engine_ attachNode:input_mixer_node_];

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

    sink_node_ = [[AVAudioSinkNode alloc] initWithReceiverBlock:sink_block];
    [audio_engine_ attachNode:sink_node_];

    // InputNode -> InputEQNode -> InputMixerNode -> SinkNode -> RTC
    [audio_engine_ connect:audio_engine_.inputNode to:input_eq_node_ format:input_format];

    [audio_engine_ connect:input_eq_node_ to:input_mixer_node_ format:input_format];
    // Convert to RTC's internal format before passing buffers to SinkNode.
    [audio_engine_ connect:input_mixer_node_ to:sink_node_ format:rtc_internal_format_];

#if defined(WEBRTC_IOS)
    if (!audio_engine_.inputNode.voiceProcessingEnabled) {
      // Voice processing.
      NSError* error = nil;
      BOOL set_input_vp_result = [audio_engine_.inputNode setVoiceProcessingEnabled:YES
                                                                              error:&error];
      if (!set_input_vp_result) {
        NSLog(@"setVoiceProcessingEnabled error: %@", error.localizedDescription);
        RTC_DCHECK(set_input_vp_result);
      }
      LOGI() << "setVoiceProcessingEnabled (input) result: " << set_input_vp_result ? "YES" : "NO";

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
            [audio_engine_.inputNode setMutedSpeechActivityEventListener:listener_block];
        LOGI() << "setMutedSpeechActivityEventListener result: " << set_listener_result ? "YES"
                                                                                        : "NO";
        RTC_DCHECK(set_listener_result);
      }

      // Other audio ducking.
      // iOS 17.0+, iPadOS 17.0+, Mac Catalyst 17.0+, macOS 14.0+, visionOS 1.0+
      if (@available(iOS 17.0, macCatalyst 17.0, macOS 14.0, visionOS 1.0, *)) {
        AVAudioVoiceProcessingOtherAudioDuckingConfiguration ducking_config;
        ducking_config.enableAdvancedDucking = YES;
        ducking_config.duckingLevel = AVAudioVoiceProcessingOtherAudioDuckingLevelMax;

        LOGI() << "setVoiceProcessingOtherAudioDuckingConfiguration";
        [audio_engine_.inputNode setVoiceProcessingOtherAudioDuckingConfiguration:ducking_config];
      }
    }
#endif
  } else if (old_state.input_enabled && !new_state.input_enabled) {
    LOGI() << "Disabling input for AVAudioEngine...";
    RTC_DCHECK(!audio_engine_.running);

    // Disconnect input eq
    [audio_engine_ disconnectNodeInput:input_eq_node_];
    [audio_engine_ disconnectNodeOutput:input_eq_node_];
    [audio_engine_ detachNode:input_eq_node_];
    input_eq_node_ = nil;

    // InputMixerNode
    [audio_engine_ disconnectNodeInput:input_mixer_node_];
    [audio_engine_ disconnectNodeOutput:input_mixer_node_];
    [audio_engine_ detachNode:input_mixer_node_];
    input_mixer_node_ = nil;

    // SinkNode
    [audio_engine_ disconnectNodeInput:sink_node_];
    [audio_engine_ disconnectNodeOutput:sink_node_];
    [audio_engine_ detachNode:sink_node_];
    sink_node_ = nil;
  }

  if (new_state.input_enabled) {
    if (audio_engine_.inputNode.voiceProcessingEnabled) {
      // Re-apply muted state.
      audio_engine_.inputNode.voiceProcessingInputMuted = new_state.input_muted;
    }
  }

  if (new_state.IsAnyRunning()) {
    if (!old_state.IsAnyRunning() || (old_state.is_interrupted && !new_state.is_interrupted) ||
        did_change_audio_graph) {
      LOGI() << "Starting AVAudioEngine...";
      NSError* error = nil;
      BOOL start_result = [audio_engine_ startAndReturnError:&error];
      if (!start_result) {
        LOGE() << "Failed to start engine: " << error.localizedDescription.UTF8String;
      }
    }
  }

  if (old_state.IsAnyEnabled() && !new_state.IsAnyEnabled()) {
    LOGI() << "Releasing AVAudioEngine...";
    audio_engine_ = nil;
  }
}

// ----------------------------------------------------------------------------------------------------
// Private - EngineState

bool AudioEngineDevice::EngineState::operator==(const EngineState& rhs) const {
  return input_enabled == rhs.input_enabled && output_enabled == rhs.output_enabled &&
         input_running == rhs.input_running && output_running == rhs.output_running &&
         input_muted == rhs.input_muted && is_interrupted == rhs.is_interrupted;
}

bool AudioEngineDevice::EngineState::operator!=(const EngineState& rhs) const {
  return !(*this == rhs);
}

// ----------------------------------------------------------------------------------------------------
// Private - Audio session
#if defined(WEBRTC_IOS)
bool AudioEngineDevice::ConfigureAudioSession() {
  RTC_DCHECK_RUN_ON(thread_);
  RTCLog(@"Configuring audio session.");
  if (has_configured_session_) {
    RTCLogWarning(@"Audio session already configured.");
    return false;
  }
  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  [session lockForConfiguration];
  bool success = [session configureWebRTCSession:nil];
  [session unlockForConfiguration];
  if (success) {
    has_configured_session_ = true;
    RTCLog(@"Configured audio session.");
  } else {
    RTCLog(@"Failed to configure audio session.");
  }
  return success;
}

bool AudioEngineDevice::ConfigureAudioSessionLocked() {
  RTC_DCHECK_RUN_ON(thread_);
  RTCLog(@"Configuring audio session.");
  if (has_configured_session_) {
    RTCLogWarning(@"Audio session already configured.");
    return false;
  }
  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  bool success = [session configureWebRTCSession:nil];
  if (success) {
    has_configured_session_ = true;
    RTCLog(@"Configured audio session.");
  } else {
    RTCLog(@"Failed to configure audio session.");
  }
  return success;
}

void AudioEngineDevice::UnconfigureAudioSession() {
  RTC_DCHECK_RUN_ON(thread_);
  RTCLog(@"Unconfiguring audio session.");
  if (!has_configured_session_) {
    RTCLogWarning(@"Audio session already unconfigured.");
    return;
  }
  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  [session lockForConfiguration];
  [session unconfigureWebRTCSession:nil];
  [session endWebRTCSession:nil];
  [session unlockForConfiguration];
  has_configured_session_ = false;
  RTCLog(@"Unconfigured audio session.");
}
#endif

}  // namespace webrtc

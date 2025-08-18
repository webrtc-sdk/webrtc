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

#if defined(WEBRTC_IOS)
#import "components/audio/RTCAudioSession+Private.h"
#import "components/audio/RTCAudioSession.h"
#import "components/audio/RTCAudioSessionConfiguration.h"
#import "components/audio/RTCNativeAudioSessionDelegateAdapter.h"
#endif

#if TARGET_OS_OSX
#import "./mac/audio_device_utils_mac.h"
#endif

namespace webrtc {

NSString* const kAudioEngineInputMixerNodeKey = @"_audio_engine_input_mixer_node_key";

#define LOGI() RTC_LOG(LS_INFO) << "AudioEngineDevice::"
#define LOGE() RTC_LOG(LS_ERROR) << "AudioEngineDevice::"
#define LOGW() RTC_LOG(LS_WARNING) << "AudioEngineDevice::"

const UInt16 kFixedPlayoutDelayEstimate = 0;
const UInt16 kFixedRecordDelayEstimate = 0;
const UInt16 kStartEngineMaxRetries = 10;  // Maximum blocking 1sec.
const useconds_t kStartEngineRetryDelayMs = 100;

const size_t kMaximumFramesPerBuffer = 3072;
const size_t kAudioSampleSize = 2;  // Signed 16-bit integer

AudioEngineDevice::AudioEngineDevice(bool voice_processing_bypassed)
    : task_queue_factory_(CreateDefaultTaskQueueFactory()), initialized_(false) {
  LOGI() << "voice_processing_bypassed " << voice_processing_bypassed;

  thread_ = webrtc::Thread::Current();
  audio_device_buffer_.reset(new webrtc::AudioDeviceBuffer(task_queue_factory_.get()));

#if defined(WEBRTC_IOS)
  audio_session_observer_ =
      [[RTC_OBJC_TYPE(RTCNativeAudioSessionDelegateAdapter) alloc] initWithObserver:this];
  // Subscribe to audio session events.
  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  [session addDelegate:audio_session_observer_];
#endif

  mach_timebase_info_data_t tinfo;
  mach_timebase_info(&tinfo);
  machTickUnitsToNanoseconds_ = (double)tinfo.numer / tinfo.denom;

  // Manual rendering formats are fixed to 48k for now.
  manual_render_rtc_format_ = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16
                                                               sampleRate:48000
                                                                 channels:1
                                                              interleaved:YES];

  // Initial engine state
  engine_state_.voice_processing_bypassed = voice_processing_bypassed;
}

AudioEngineDevice::~AudioEngineDevice() {
  RTC_DCHECK_RUN_ON(thread_);

  safety_->SetNotAlive();
#if TARGET_OS_OSX
  default_device_update_safety_->SetNotAlive();
#endif

  Terminate();

#if defined(WEBRTC_IOS)
  RTC_OBJC_TYPE(RTCAudioSession)* session = [RTC_OBJC_TYPE(RTCAudioSession) sharedInstance];
  [session removeDelegate:audio_session_observer_];
  audio_session_observer_ = nil;
#endif
}

#if TARGET_OS_OSX
OSStatus AudioEngineDevice::objectListenerProc(AudioObjectID objectId, UInt32 numberAddresses,
                                               const AudioObjectPropertyAddress addresses[],
                                               void* clientData) {
  AudioEngineDevice* ptrThis = (AudioEngineDevice*)clientData;
  RTC_DCHECK(ptrThis != NULL);

  // ptrThis->implObjectListenerProc(objectId, numberAddresses, addresses);

  for (UInt32 i = 0; i < numberAddresses; i++) {
    ptrThis->HandleDeviceListenerEvent(addresses[i].mSelector);
  }

  return 0;
}

void AudioEngineDevice::HandleDeviceListenerEvent(AudioObjectPropertySelector selector) {
  thread_->PostTask(SafeTask(safety_, [this, selector] {
    RTC_DCHECK_RUN_ON(thread_);

    if (selector == kAudioHardwarePropertyDevices) {
      auto old_input_device_ids = input_device_ids_;
      auto old_output_device_ids = output_device_ids_;
      UpdateAllDeviceIDs();
      // Check if device ids updated
      if (old_output_device_ids != output_device_ids_ ||
          old_input_device_ids != input_device_ids_) {
        LOGI() << "Did update devices";

        // Current device
        if (engine_state_.output_device_id != kAudioObjectUnknown) {
          bool contains = std::binary_search(output_device_ids_.begin(), output_device_ids_.end(),
                                             engine_state_.output_device_id);
          if (!contains) {
            int32_t result = ModifyEngineState([](EngineState state) -> EngineState {
              state.output_device_id = kAudioObjectUnknown;
              return state;
            });
            if (result != 0) {
              LOGE() << "Failed to reset output device ID, error: " << result;
            }
          }
        }

        if (engine_state_.input_device_id != kAudioObjectUnknown) {
          bool contains = std::binary_search(input_device_ids_.begin(), input_device_ids_.end(),
                                             engine_state_.input_device_id);
          if (!contains) {
            int32_t result = ModifyEngineState([](EngineState state) -> EngineState {
              state.input_device_id = kAudioObjectUnknown;
              return state;
            });
            if (result != 0) {
              LOGE() << "Failed to reset input device ID, error: " << result;
            }
          }
        }

        if (observer_) {
          observer_->OnDevicesUpdated();
        }
      }
    } else if (selector == kAudioHardwarePropertyDefaultOutputDevice ||
               selector == kAudioHardwarePropertyDefaultInputDevice) {
      // Cancel any pending updates
      default_device_update_safety_->SetNotAlive();
      default_device_update_safety_ = PendingTaskSafetyFlag::Create();

      // Schedule a new debounced update
      thread_->PostDelayedTask(
          SafeTask(default_device_update_safety_,
                   [this, selector] {
                     RTC_DCHECK_RUN_ON(thread_);
                     LOGI() << "Processing debounced default device update for selector: "
                            << selector;

                     if (selector == kAudioHardwarePropertyDefaultOutputDevice) {
                       LOGI() << "Did update default output device";
                       int32_t result = ModifyEngineState([](EngineState state) -> EngineState {
                         state.default_output_device_update_count++;
                         return state;
                       });
                       if (result != 0) {
                         LOGE() << "Failed to update default output device update count, error: "
                                << result;
                       }
                     } else if (selector == kAudioHardwarePropertyDefaultInputDevice) {
                       LOGI() << "Did update default input device";
                       int32_t result = ModifyEngineState([](EngineState state) -> EngineState {
                         state.default_input_device_update_count++;
                         return state;
                       });
                       if (result != 0) {
                         LOGE() << "Failed to update default input device update count, error: "
                                << result;
                       }
                     }
                   }),
          TimeDelta::Millis(kDefaultDeviceUpdateDebounceMs));
    }
  }));
}

#endif

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

#if TARGET_OS_OSX
  // Setting RunLoop to NULL here instructs HAL to manage its own thread for
  // notifications. This was the default behaviour on OS X 10.5 and earlier,
  // but now must be explicitly specified. HAL would otherwise try to use the
  // main thread to issue notifications.
  AudioObjectPropertyAddress propertyAddress = {kAudioHardwarePropertyRunLoop,
                                                kAudioObjectPropertyScopeGlobal,
                                                kAudioObjectPropertyElementMaster};

  CFRunLoopRef runLoop = NULL;
  UInt32 size = sizeof(CFRunLoopRef);
  OSStatus err = noErr;

  err = AudioObjectSetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, size,
                                   &runLoop);
  if (err != noErr) {
    LOGE() << "AudioObjectSetPropertyData failed with error: " << err;
    return -1;
  }

  // Listen for any device changes.
  propertyAddress.mSelector = kAudioHardwarePropertyDevices;
  err = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &propertyAddress,
                                       &objectListenerProc, this);
  if (err != noErr) {
    LOGE() << "AudioObjectAddPropertyListener failed with error: " << err;
    return -1;
  }

  // Listen for default output device change.
  propertyAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
  err = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &propertyAddress,
                                       &objectListenerProc, this);
  if (err != noErr) {
    LOGE() << "AudioObjectAddPropertyListener failed with error: " << err;
    return -1;
  }

  // Listen for default input device change.
  propertyAddress.mSelector = kAudioHardwarePropertyDefaultInputDevice;
  err = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &propertyAddress,
                                       &objectListenerProc, this);
  if (err != noErr) {
    LOGE() << "AudioObjectAddPropertyListener failed with error: " << err;
    return -1;
  }

  UpdateAllDeviceIDs();
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

#if TARGET_OS_OSX
  // Remove listeners for global scope.
  AudioObjectPropertyAddress propertyAddress = {
      kAudioHardwarePropertyDevices,     // selector
      kAudioObjectPropertyScopeGlobal,   // scope
      kAudioObjectPropertyElementMaster  // element
  };

  OSStatus err = noErr;
  err = AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &propertyAddress,
                                          &objectListenerProc, this);
  if (err != noErr) {
    LOGE() << "AudioObjectRemovePropertyListener failed with error: " << err;
    return -1;
  }

  propertyAddress.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
  err = AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &propertyAddress,
                                          &objectListenerProc, this);
  if (err != noErr) {
    LOGE() << "AudioObjectRemovePropertyListener failed with error: " << err;
    return -1;
  }

  propertyAddress.mSelector = kAudioHardwarePropertyDefaultInputDevice;
  err = AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &propertyAddress,
                                          &objectListenerProc, this);
  if (err != noErr) {
    LOGE() << "AudioObjectRemovePropertyListener failed with error: " << err;
    return -1;
  }
#endif

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

  int32_t result = ModifyEngineState([](EngineState state) -> EngineState {
    state.output_enabled = true;
    return state;
  });

  return result;
}

int32_t AudioEngineDevice::StartPlayout() {
  LOGI() << "StartPlayout";
  RTC_DCHECK_RUN_ON(thread_);

  int32_t result = ModifyEngineState([](EngineState state) -> EngineState {
    state.output_running = true;
    return state;
  });

  return result;
}

int32_t AudioEngineDevice::StopPlayout() {
  LOGI() << "StopPlayout";
  RTC_DCHECK_RUN_ON(thread_);

  int32_t result = ModifyEngineState([](EngineState state) -> EngineState {
    state.output_enabled = false;
    state.output_running = false;
    return state;
  });

  return result;
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

  int32_t result = ModifyEngineState([](EngineState state) -> EngineState {
    state.input_enabled = true;
    return state;
  });

  return result;
}

int32_t AudioEngineDevice::StartRecording() {
  LOGI() << "StartRecording";
  RTC_DCHECK_RUN_ON(thread_);

  int32_t result = ModifyEngineState([](EngineState state) -> EngineState {
    state.input_running = true;
    state.input_muted = false;  // Always unmute
    return state;
  });

  return result;
}

int32_t AudioEngineDevice::StopRecording() {
  LOGI() << "StopRecording";
  RTC_DCHECK_RUN_ON(thread_);

  int32_t result = ModifyEngineState([](EngineState state) -> EngineState {
    state.input_enabled = false;
    state.input_running = false;
    return state;
  });

  return result;
}

// ----------------------------------------------------------------------------------------------------
// AudioSessionObserver

void AudioEngineDevice::OnInterruptionBegin() {
  LOGI() << "OnInterruptionBegin";

  RTC_DCHECK(thread_);
  thread_->PostTask(SafeTask(safety_, [this] {
    int32_t result = this->ModifyEngineState([](EngineState state) -> EngineState {
      state.is_interrupted = true;
      return state;
    });
    if (result != 0) {
      LOGE() << "Failed to update engine state for interruption begin, error: " << result;
    }
  }));
}

void AudioEngineDevice::OnInterruptionEnd(bool should_resume) {
  LOGI() << "OnInterruptionEnd should_resume: " << should_resume;

  RTC_DCHECK(thread_);
  thread_->PostTask(SafeTask(safety_, [this] {
    int32_t result = this->ModifyEngineState([](EngineState state) -> EngineState {
      state.is_interrupted = false;
      return state;
    });
    if (result != 0) {
      LOGE() << "Failed to update engine state for interruption end, error: " << result;
    }
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

  int32_t result = ModifyEngineState([enable](EngineState state) -> EngineState {
    state.input_muted = enable;
    return state;
  });

  return result;
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
  LOGI() << "SetPlayoutDevice value: " << index;
  RTC_DCHECK_RUN_ON(thread_);

#if TARGET_OS_OSX
  if (index > (output_device_ids_.size())) {
    LOGE() << "Device index is out of range: " << index;
    return -1;
  }

  // Set as default device if index == 0
  AudioDeviceID output_device_id = index == 0 ? kAudioObjectUnknown : output_device_ids_[index - 1];

  int32_t result = ModifyEngineState([output_device_id](EngineState state) -> EngineState {
    state.output_device_id = output_device_id;
    return state;
  });
  return result;
#else
  return 0;
#endif
}

int32_t AudioEngineDevice::SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType deviceType) {
  LOGW() << "SetPlayoutDevice: Not implemented, value: " << deviceType;

  return -1;
}
int32_t AudioEngineDevice::PlayoutDeviceName(uint16_t index, char name[kAdmMaxDeviceNameSize],
                                             char guid[kAdmMaxGuidSize]) {
  RTC_DCHECK_RUN_ON(thread_);

#if TARGET_OS_OSX
  RTC_DCHECK(output_device_ids_.size() == output_device_labels_.size());

  if ((index > (output_device_ids_.size())) || (name == NULL)) {
    LOGE() << "Device index is out of range: " << index;
    return -1;
  }

  memset(name, 0, kAdmMaxDeviceNameSize);
  memset(guid, 0, kAdmMaxGuidSize);

  // Default device
  if (index == 0) {
    std::optional<AudioDeviceID> default_device_id = mac_audio_utils::GetDefaultOutputDeviceID();
    if (!default_device_id) {
      return -1;
    }

    std::optional<std::string> label = mac_audio_utils::GetDeviceLabel(*default_device_id, false);
    std::optional<std::string> device_guid =
        std::string("default");  // mac_audio_utils::GetDeviceUniqueID(*default_device_id);
    if (!label || !device_guid) {
      return -1;
    }

    strncpy(name, (*label).c_str(), kAdmMaxDeviceNameSize - 1);
    strncpy(guid, (*device_guid).c_str(), kAdmMaxGuidSize - 1);

    return 0;
  }

  // Get device name
  strncpy(name, output_device_labels_[index - 1].c_str(), kAdmMaxDeviceNameSize - 1);

  std::optional<std::string> device_guid =
      mac_audio_utils::GetDeviceUniqueID(output_device_ids_[index - 1]);
  if (device_guid) {
    strncpy(guid, device_guid->c_str(), kAdmMaxGuidSize - 1);
  } else {
    LOGE() << "Failed to get device unique ID for device: " << output_device_ids_[index - 1];
    return -1;
  }

  return 0;
#else
  return -1;
#endif
}

int16_t AudioEngineDevice::PlayoutDevices() {
  RTC_DCHECK_RUN_ON(thread_);

#if TARGET_OS_OSX
  return output_device_ids_.size() + 1;
#else
  return (int16_t)1;
#endif
}

// ----------------------------------------------------------------------------------------------------
// Recording Device

int32_t AudioEngineDevice::RecordingDeviceName(uint16_t index, char name[kAdmMaxDeviceNameSize],
                                               char guid[kAdmMaxGuidSize]) {
#if TARGET_OS_OSX
  RTC_DCHECK(input_device_ids_.size() == input_device_labels_.size());

  if ((index > (input_device_ids_.size())) || (name == NULL)) {
    LOGE() << "Device index is out of range: " << index;
    return -1;
  }

  memset(name, 0, kAdmMaxDeviceNameSize);
  memset(guid, 0, kAdmMaxGuidSize);

  // Default device
  if (index == 0) {
    std::optional<AudioDeviceID> default_device_id = mac_audio_utils::GetDefaultInputDeviceID();
    if (!default_device_id) {
      return -1;
    }

    std::optional<std::string> label = mac_audio_utils::GetDeviceLabel(*default_device_id, true);
    std::optional<std::string> device_guid =
        std::string("default");  // mac_audio_utils::GetDeviceUniqueID(*default_device_id);
    if (!label || !device_guid) {
      return -1;
    }

    strncpy(name, (*label).c_str(), kAdmMaxDeviceNameSize - 1);
    strncpy(guid, (*device_guid).c_str(), kAdmMaxGuidSize - 1);

    return 0;
  }

  // Get device name
  strncpy(name, input_device_labels_[index - 1].c_str(), kAdmMaxDeviceNameSize - 1);

  std::optional<std::string> device_guid =
      mac_audio_utils::GetDeviceUniqueID(input_device_ids_[index - 1]);
  if (device_guid) {
    strncpy(guid, device_guid->c_str(), kAdmMaxGuidSize - 1);
  } else {
    LOGE() << "Failed to get device unique ID for device: " << input_device_ids_[index - 1];
    return -1;
  }

  return 0;
#else
  return -1;
#endif
}

int32_t AudioEngineDevice::SetRecordingDevice(uint16_t index) {
  LOGI() << "SetRecordingDevice, index: " << index;
  RTC_DCHECK_RUN_ON(thread_);

#if TARGET_OS_OSX
  if (index > (input_device_ids_.size())) {
    RTC_LOG(LS_ERROR) << "Device index is out of range";
    return -1;
  }

  // Set as default device if index == 0
  AudioDeviceID input_device_id = index == 0 ? kAudioObjectUnknown : input_device_ids_[index - 1];

  int32_t result = ModifyEngineState([input_device_id](EngineState state) -> EngineState {
    state.input_device_id = input_device_id;
    return state;
  });
  return result;
#else
  return 0;
#endif
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
  RTC_DCHECK_RUN_ON(thread_);

#if TARGET_OS_OSX
  return input_device_ids_.size() + 1;
#else
  return (int16_t)1;
#endif
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

bool AudioEngineDevice::BuiltInAECIsAvailable() const { return true; }

bool AudioEngineDevice::BuiltInAGCIsAvailable() const { return true; }

bool AudioEngineDevice::BuiltInNSIsAvailable() const { return false; }

int32_t AudioEngineDevice::EnableBuiltInAEC(bool enable) { return 0; }

int32_t AudioEngineDevice::EnableBuiltInAGC(bool enable) { return 0; }

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

bool AudioEngineDevice::IsEngineRunning() {
  LOGI() << "IsEngineRunning";
  RTC_DCHECK_RUN_ON(thread_);

  if (engine_device_ == nil) return false;
  return engine_device_.running;
}

int32_t AudioEngineDevice::SetEngineState(EngineState new_state) {
  LOGI() << "SetEngineState";
  RTC_DCHECK_RUN_ON(thread_);

  int32_t result =
      ModifyEngineState([new_state](EngineState state) -> EngineState { return new_state; });

  return result;
}

int32_t AudioEngineDevice::GetEngineState(EngineState* state) {
  RTC_DCHECK_RUN_ON(thread_);

  *state = engine_state_;

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

int32_t AudioEngineDevice::VoiceProcessingBypassed(bool* enabled) {
  LOGI() << "VoiceProcessingBypassed";
  RTC_DCHECK_RUN_ON(thread_);

  if (enabled == nullptr) {
    return -1;
  }

  *enabled = engine_state_.voice_processing_bypassed;

  return 0;
}

int32_t AudioEngineDevice::SetVoiceProcessingEnabled(bool enable) {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "SetVoiceProcessingEnabled: " << enable;

  int32_t result = ModifyEngineState([enable](EngineState state) -> EngineState {
    state.voice_processing_enabled = enable;
    return state;
  });

  return result;
}

int32_t AudioEngineDevice::VoiceProcessingEnabled(bool* enabled) {
  LOGI() << "VoiceProcessingEnabled";
  RTC_DCHECK_RUN_ON(thread_);

  if (enabled == nullptr) {
    return -1;
  }

  *enabled = engine_state_.voice_processing_enabled;

  return 0;
}

int32_t AudioEngineDevice::SetVoiceProcessingBypassed(bool enable) {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "SetVoiceProcessingBypassed: " << enable;

  int32_t result = ModifyEngineState([enable](EngineState state) -> EngineState {
    state.voice_processing_bypassed = enable;
    return state;
  });

  return result;
}

int32_t AudioEngineDevice::VoiceProcessingAGCEnabled(bool* enabled) {
  LOGI() << "VoiceProcessingAGCEnabled";
  RTC_DCHECK_RUN_ON(thread_);

  if (enabled == nullptr) {
    return -1;
  }

  *enabled = engine_state_.voice_processing_agc_enabled;

  return 0;
}

int32_t AudioEngineDevice::SetVoiceProcessingAGCEnabled(bool enable) {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "SetVoiceProcessingAGCEnabled: " << enable;

  int32_t result = ModifyEngineState([enable](EngineState state) -> EngineState {
    state.voice_processing_agc_enabled = enable;
    return state;
  });

  return result;
}

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

  int32_t result = ModifyEngineState([enable](EngineState state) -> EngineState {
    state.render_mode = enable ? RenderMode::Manual : RenderMode::Device;
    return state;
  });

  return result;
}

int32_t AudioEngineDevice::GetMuteMode(MuteMode* mode) {
  LOGI() << "GetMuteMode";
  RTC_DCHECK_RUN_ON(thread_);

  if (mode == nullptr) {
    return -1;
  }

  *mode = engine_state_.mute_mode;

  return 0;
}

int32_t AudioEngineDevice::SetMuteMode(MuteMode mode) {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "SetMuteMode: " << mode;

  int32_t result = ModifyEngineState([mode](EngineState state) -> EngineState {
    state.mute_mode = mode;
    return state;
  });

  return result;
}

int32_t AudioEngineDevice::InitAndStartRecording() {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "InitAndStartRecording";

  int32_t result = ModifyEngineState([](EngineState state) -> EngineState {
    state.input_enabled = true;
    state.input_running = true;
    state.input_muted = false;  // Always unmute
    return state;
  });

  return result;
}

int32_t AudioEngineDevice::SetAdvancedDucking(bool enable) {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "SetAdvancedDucking: " << enable;

  int32_t result = ModifyEngineState([enable](EngineState state) -> EngineState {
    state.advanced_ducking = enable;
    return state;
  });

  return result;
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

  int32_t result = ModifyEngineState([level](EngineState state) -> EngineState {
    state.ducking_level = level;
    return state;
  });

  return result;
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

  int32_t result = ModifyEngineState([enable](EngineState state) -> EngineState {
    state.input_enabled_persistent_mode = enable;
    return state;
  });

  return result;
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

void AudioEngineDevice::ReconfigureEngine() {
  LOGI() << "ReconfigureEngine";

  // TODO: More optimizations
  // We only need to re-attach the input / output nodes with updated sample rate etc.

  thread_->PostTask(SafeTask(safety_, [this] {
    RTC_DCHECK_RUN_ON(thread_);

    EngineState current_state = this->engine_state_;

    // Re-configure is only for device mode
    if (current_state.render_mode != RenderMode::Device) return;

    EngineState shutdown_state = this->engine_state_;
    shutdown_state.input_enabled = false;
    shutdown_state.input_running = false;
    shutdown_state.output_enabled = false;
    shutdown_state.output_running = false;

    int32_t shutdown_result =
        this->ModifyEngineState([shutdown_state](EngineState state) -> EngineState {
          return shutdown_state;  // Shutdown engine
        });

    if (shutdown_result != 0) {
      LOGE() << "ReconfigureEngine: Failed to shutdown engine, error: " << shutdown_result;
      return;
    }

    int32_t recover_result =
        this->ModifyEngineState([current_state](EngineState state) -> EngineState {
          return current_state;  // Recover engine state
        });

    if (recover_result != 0) {
      LOGE() << "ReconfigureEngine: Failed to recover engine state, error: " << recover_result;
      // We're in a bad state now, could consider more recovery options here
    }
  }));
}

bool AudioEngineDevice::IsMicrophonePermissionGranted() {
  AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
  return status == AVAuthorizationStatusAuthorized;
}

int32_t AudioEngineDevice::ModifyEngineState(
    std::function<EngineState(EngineState)> state_transform) {
  RTC_DCHECK_RUN_ON(thread_);

  EngineState old_state = engine_state_;
  EngineState new_state = state_transform(old_state);
  EngineStateUpdate state = {old_state, new_state};

  // No changes, return immediately.
  if (state.HasNoChanges()) {
    return 0;
  }

  // Check input should be enabled if running.
  if (new_state.input_running && !new_state.input_enabled) {
    LOGE() << "ModifyEngineState: Input must be enabled if running";
    return -1;
  }

  // Check output should be enabled if running.
  if (new_state.output_running && !new_state.output_enabled) {
    LOGE() << "ModifyEngineState: Output must be enabled if running";
    return -1;
  }

  int32_t shutdown_result = 0;
  int32_t startup_result = 0;

  // Did switch Device -> Manual rendering
  if (state.DidEnableManualRenderingMode()) {
    EngineStateUpdate shutdown_state = state;                  // Copy current state
    shutdown_state.next = {};                                  // Reset next state to default
    shutdown_result = ApplyDeviceEngineState(shutdown_state);  // Shutdown device rendering
    if (shutdown_result != 0) {
      LOGE() << "ModifyEngineState: Failed to shutdown device rendering, error: "
             << shutdown_result;
    }
    EngineStateUpdate startup_state = state;                 // Copy current state
    shutdown_state.prev = {};                                //
    startup_result = ApplyManualEngineState(startup_state);  // Start manual mode
    if (startup_result != 0) {
      LOGE() << "ModifyEngineState: Failed to start manual mode, error: " << startup_result;
    }
  } else if (state.DidEnableDeviceRenderingMode()) {
    EngineStateUpdate shutdown_state = state;
    shutdown_state.next = {};                                  // Reset next state to default
    shutdown_result = ApplyManualEngineState(shutdown_state);  // Shutdown manual rendering
    if (shutdown_result != 0) {
      LOGE() << "ModifyEngineState: Failed to shutdown manual rendering, error: "
             << shutdown_result;
    }
    EngineStateUpdate startup_state = state;                 // Copy current state
    shutdown_state.prev = {};                                //
    startup_result = ApplyDeviceEngineState(startup_state);  // Start device mode
    if (startup_result != 0) {
      LOGE() << "ModifyEngineState: Failed to start device mode, error: " << startup_result;
    }
  } else if (new_state.render_mode == RenderMode::Device) {
    shutdown_result = ApplyDeviceEngineState(state);
    if (shutdown_result != 0) {
      LOGE() << "ModifyEngineState: Failed to update state in device mode, error: "
             << shutdown_result;
    }
  } else if (new_state.render_mode == RenderMode::Manual) {
    startup_result = ApplyManualEngineState(state);
    if (startup_result != 0) {
      LOGE() << "ModifyEngineState: Failed to update state in manual mode, error: "
             << startup_result;
    }
  }

  int32_t return_result = shutdown_result != 0 ? shutdown_result : startup_result;

  // Additional checks for buffer state.
  if (return_result == 0) {
    // Buffer should be playing if output is running.
    if (new_state.IsOutputEnabled()) {
      RTC_DCHECK(audio_device_buffer_->IsPlaying());
      if (!audio_device_buffer_->IsPlaying()) {
        LOGE() << "ModifyEngineState: Buffer should be playing when output is enabled";
      }
    } else {
      RTC_DCHECK(!audio_device_buffer_->IsPlaying());
      if (audio_device_buffer_->IsPlaying()) {
        LOGE() << "ModifyEngineState: Buffer should not be playing when output is disabled";
      }
    }

    // Buffer should be recording if input is running.
    if (new_state.IsInputEnabled()) {
      RTC_DCHECK(audio_device_buffer_->IsRecording());
      if (!audio_device_buffer_->IsRecording()) {
        LOGE() << "ModifyEngineState: Buffer should be recording when input is enabled";
      }
    } else {
      RTC_DCHECK(!audio_device_buffer_->IsRecording());
      if (audio_device_buffer_->IsRecording()) {
        LOGE() << "ModifyEngineState: Buffer should not be recording when input is disabled";
      }
    }

    // Update engine state if no error
    engine_state_ = new_state;
  }

  return return_result;
}

int32_t AudioEngineDevice::ApplyManualEngineState(EngineStateUpdate state) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(engine_device_ == nullptr);

  auto outputNode = [this, state]() {
    RTC_DCHECK_RUN_ON(thread_);
    RTC_DCHECK(engine_manual_input_ != nil);
    RTC_DCHECK(state.prev.IsOutputEnabled() || state.next.IsOutputEnabled());
    return engine_manual_input_.outputNode;
  };

  if (state.prev.IsAnyRunning() && !state.next.IsAnyRunning()) {
    LOGI() << "Stopping AVAudioEngine...";
    RTC_DCHECK(engine_manual_input_ != nil);
    [engine_manual_input_ stop];

    LOGI() << "Stopping render thread...";
    RTC_DCHECK(render_thread_ != nullptr);
    render_thread_->Stop();
    render_thread_ = nullptr;

    LOGI() << "Releasing manual render buffer...";
    RTC_DCHECK(render_buffer_ != nullptr);
    render_buffer_ = nullptr;

    LOGI() << "Releasing manual read buffer...";
    RTC_DCHECK(read_buffer_ != nullptr);
    read_buffer_ = nullptr;

    if (observer_ != nullptr) {
      int32_t result = observer_->OnEngineDidStop(
          engine_manual_input_, state.next.IsOutputEnabled(), state.next.IsInputEnabled());
      if (result != 0) {
        LOGE() << "Call to OnEngineDidStop returned error: " << result;
        return result;
      }
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
      int32_t result = observer_->OnEngineDidCreate(engine_manual_input_);
      if (result != 0) {
        LOGE() << "Call to OnEngineDidCreate returned error: " << result;
        return result;
      }
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
    int32_t result = observer_->OnEngineWillEnable(
        engine_manual_input_, state.next.IsOutputEnabled(), state.next.IsInputEnabled());
    if (result != 0) {
      LOGE() << "Call to OnEngineWillEnable returned error: " << result;
      return result;
    }
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

    if (this->observer_ != nullptr) {
      NSDictionary* context = @{};
      int32_t result = this->observer_->OnEngineWillConnectInput(
          engine_manual_input_, nil, engine_manual_input_.mainMixerNode, manual_render_rtc_format_,
          context);
      if (result != 0) {
        LOGE() << "Call to OnEngineWillConnectInput returned error: " << result;
        return result;
      }
    }

    [engine_manual_input_ connect:engine_manual_input_.mainMixerNode
                               to:outputNode()
                           format:manual_render_rtc_format_];

  } else if (state.prev.IsInputEnabled() && !state.next.IsInputEnabled()) {
    LOGI() << "Disabling input for AVAudioEngine...";
    RTC_DCHECK(!engine_manual_input_.running);
  }

  if (state.DidAnyDisable() && observer_ != nullptr) {
    int32_t result = observer_->OnEngineDidDisable(
        engine_manual_input_, state.next.IsOutputEnabled(), state.next.IsInputEnabled());
    if (result != 0) {
      LOGE() << "Call to OnEngineDidDisable returned error: " << result;
      return result;
    }
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
      int32_t result = observer_->OnEngineWillStart(
          engine_manual_input_, state.next.IsOutputEnabled(), state.next.IsInputEnabled());
      if (result != 0) {
        LOGE() << "Call to OnEngineWillStart returned error: " << result;
        return result;
      }
    }

    LOGI() << "Allocating manual render buffer...";
    RTC_DCHECK(render_buffer_ == nullptr);
    render_buffer_ = [[AVAudioPCMBuffer alloc] initWithPCMFormat:manual_render_rtc_format_
                                                   frameCapacity:kMaximumFramesPerBuffer];

    LOGI() << "Allocating manual read buffer...";
    RTC_DCHECK(read_buffer_ == nullptr);
    read_buffer_ = [[AVAudioPCMBuffer alloc] initWithPCMFormat:manual_render_rtc_format_
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
    render_thread_ = webrtc::Thread::Create();
    render_thread_->SetName("render_thread", nullptr);
    render_thread_->Start();
    render_thread_->PostTask([this] { this->StartRenderLoop(); });
  }

  if (state.prev.IsAnyEnabled() && !state.next.IsAnyEnabled()) {
    if (observer_ != nullptr) {
      int32_t result = observer_->OnEngineWillRelease(engine_manual_input_);
      if (result != 0) {
        LOGE() << "Call to OnEngineWillRelease returned error: " << result;
        return result;
      }
    }
    LOGI() << "Releasing AVAudioEngine...";
    engine_manual_input_ = nil;
  }

  return 0;
}

int32_t AudioEngineDevice::ApplyDeviceEngineState(EngineStateUpdate state) {
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(engine_manual_input_ == nullptr);

  std::vector<std::function<void()>> rollback_actions;

  auto rollback = [&](int32_t result) {
    for (auto& action : rollback_actions) {
      action();
    }

    return result;
  };

  auto inputNode = [this, state]() {
    RTC_DCHECK_RUN_ON(thread_);
    RTC_DCHECK(engine_device_ != nil);
    RTC_DCHECK(state.prev.IsInputEnabled() || state.next.IsInputEnabled());
    return engine_device_.inputNode;
  };

  auto outputNode = [this, state]() {
    RTC_DCHECK_RUN_ON(thread_);
    RTC_DCHECK(engine_device_ != nil);
    RTC_DCHECK(state.prev.IsOutputEnabled() || state.next.IsOutputEnabled());
    return engine_device_.outputNode;
  };

  // --------------------------------------------------------------------------------------------
  // Step: Stop AVAudioEngine
  //
  if (state.prev.IsAnyRunning() &&
      (!state.next.IsAnyRunning() || state.IsEngineRestartRequired() ||
       state.DidBeginInterruption() || state.IsEngineRecreateRequired())) {
    LOGI() << "Stopping AVAudioEngine...";
    RTC_DCHECK(engine_device_ != nil);

    if (configuration_observer_ != nullptr) {
      NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
      [center removeObserver:(__bridge_transfer id)configuration_observer_
                        name:AVAudioEngineConfigurationChangeNotification
                      object:engine_device_];
      configuration_observer_ = nil;
    }

    [engine_device_ stop];

    if (observer_ != nullptr) {
      int32_t result = observer_->OnEngineDidStop(engine_device_, state.next.IsOutputEnabled(),
                                                  state.next.IsInputEnabled());
      if (result != 0) {
        LOGE() << "Call to OnEngineDidStop returned error: " << result;
        return rollback(result);
      }
    }
  }

  // --------------------------------------------------------------------------------------------
  // Step: Recreate AVAudioEngine
  //
  if (state.IsEngineRecreateRequired()) {
    LOGI() << "Recreate required, releasing AVAudioEngine...";
    if (observer_ != nullptr) {
      int32_t result = observer_->OnEngineWillRelease(engine_device_);
      if (result != 0) {
        LOGE() << "Call to OnEngineWillRelease returned error: " << result;
        return rollback(result);
      }
    }
    engine_device_ = nil;
  }

  // --------------------------------------------------------------------------------------------
  // Step: Create AVAudioEngine
  //
  if (state.next.IsAnyEnabled() &&
      (!state.prev.IsAnyEnabled() || state.IsEngineRecreateRequired())) {
    LOGI() << "Creating AVAudioEngine (device)...";
    RTC_DCHECK(engine_device_ == nil);

    engine_device_ = [[AVAudioEngine alloc] init];

    rollback_actions.push_back([=, this]() {
      RTC_DCHECK_RUN_ON(thread_);
      LOGI() << "Rolling back create AVAudioEngine (device)...";
      engine_device_ = nil;
    });

    if (observer_ != nullptr) {
      int32_t result = observer_->OnEngineDidCreate(engine_device_);
      if (result != 0) {
        LOGE() << "Call to OnEngineDidCreate returned error: " << result;
        return rollback(result);
      }
    }
  }

  // --------------------------------------------------------------------------------------------
  // Step: Stop playout buffer
  //
  if (!state.next.IsOutputEnabled() && audio_device_buffer_->IsPlaying()) {
    LOGI() << "Stopping Playout buffer...";
    if (engine_device_ != nullptr) {
      // Rendering must be stopped first.
      RTC_DCHECK(!engine_device_.running);
    }
    audio_device_buffer_->StopPlayout();
  }

  // --------------------------------------------------------------------------------------------
  // Step: Stop recording buffer
  //
  if (!state.next.IsInputEnabled() && audio_device_buffer_->IsRecording()) {
    LOGI() << "Stopping Record buffer...";
    if (engine_device_ != nullptr) {
      // Rendering must be stopped first.
      RTC_DCHECK(!engine_device_.running);
    }
    audio_device_buffer_->StopRecording();
  }

  // --------------------------------------------------------------------------------------------
  // Step: Trigger "engine will enable" event
  //
  if (state.DidAnyEnable() && observer_ != nullptr) {
    // Invoke here before configuring nodes. In iOS, session configuration is required before
    // enabling AGC, muted talker etc.
    int32_t result = observer_->OnEngineWillEnable(engine_device_, state.next.IsOutputEnabled(),
                                                   state.next.IsInputEnabled());
    if (result != 0) {
      LOGE() << "Call to OnEngineWillEnable returned error: " << result;
      return rollback(result);
    }
  }

  // --------------------------------------------------------------------------------------------
  // Step: Configure Voice-Processing I/O
  //
  if (state.next.IsInputEnabled() &&
      inputNode().voiceProcessingEnabled != state.next.voice_processing_enabled) {
#if TARGET_OS_SIMULATOR
    LOGI() << "setVoiceProcessingEnabled (input): "
           << (state.next.voice_processing_enabled ? "YES" : "NO") << " (Ignored on Simulator)";
#else
    LOGI() << "setVoiceProcessingEnabled (input): " << state.next.voice_processing_enabled ? "YES"
                                                                                           : "NO";
    NSError* error = nil;
    BOOL set_vp_result = [inputNode() setVoiceProcessingEnabled:state.next.voice_processing_enabled
                                                          error:&error];
    if (!set_vp_result) {
      NSLog(@"AudioEngineDevice setVoiceProcessingEnabled error: %@", error.localizedDescription);
      RTC_DCHECK(set_vp_result);
    }
    LOGI() << "setVoiceProcessingEnabled (input) result: " << set_vp_result ? "YES" : "NO";
#endif

    if (inputNode().voiceProcessingEnabled) {
      // Always unmute vp if restart mute mode.
      if (state.next.mute_mode == MuteMode::RestartEngine &&
          inputNode().voiceProcessingInputMuted) {
        LOGI() << "Update mute (voice processing) unmuting vp for restart engine mode";
        inputNode().voiceProcessingInputMuted = false;
      }

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

        BOOL set_listener_result = [inputNode() setMutedSpeechActivityEventListener:listener_block];
        if (set_listener_result) {
          LOGI() << "setMutedSpeechActivityEventListener success";
        } else {
          LOGW() << "setMutedSpeechActivityEventListener failed, ensure AVAudioSession.Mode is "
                    "videoChat or voiceChat.";
        }
      }
    }
  }

  // --------------------------------------------------------------------------------------------
  // Step: Enable output
  //
  if (state.next.IsOutputEnabled() &&
      (!state.prev.IsOutputEnabled() || state.IsEngineRecreateRequired())) {
    LOGI() << "Enabling output for AVAudioEngine...";
    RTC_DCHECK(!engine_device_.running);

    AVAudioFormat* output_node_format = [outputNode() outputFormatForBus:0];

    LOGI() << "Output format sampleRate: " << output_node_format.sampleRate
           << " channels: " << output_node_format.channelCount
           << " formatID: " << output_node_format.streamDescription->mFormatID
           << " formatFlags: " << output_node_format.streamDescription->mFormatFlags
           << " bytesPerPacket: " << output_node_format.streamDescription->mBytesPerPacket
           << " framesPerPacket: " << output_node_format.streamDescription->mFramesPerPacket
           << " bytesPerFrame: " << output_node_format.streamDescription->mBytesPerFrame
           << " channelsPerFrame: " << output_node_format.streamDescription->mChannelsPerFrame
           << " bitsPerChannel: " << output_node_format.streamDescription->mBitsPerChannel;

    if (output_node_format.sampleRate == 0 || output_node_format.channelCount == 0) {
      LOGE() << "Output device not available, sampleRate=" << output_node_format.sampleRate
             << ", channelCount=" << output_node_format.channelCount;
      return rollback(kAudioEnginePlayoutDeviceNotAvailableError);
    }

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
              webrtc::ArrayView<int16_t>(static_cast<int16_t*>(dest_buffer), frameCount),
              kFixedPlayoutDelayEstimate);

          return noErr;
        };

    source_node_ = [[AVAudioSourceNode alloc] initWithFormat:rtc_output_format
                                                 renderBlock:source_block];
    [engine_device_ attachNode:source_node_];

    [engine_device_ connect:source_node_
                         to:engine_device_.mainMixerNode
                     format:engine_output_format];

    // mainMixerNode -> outputNode is connected by default by AVAudioEngine, but we connect anyways
    // with format.
    [engine_device_ connect:engine_device_.mainMixerNode
                         to:outputNode()
                     format:engine_output_format];

    if (this->observer_ != nullptr) {
      NSDictionary* context = @{};
      int32_t result =
          this->observer_->OnEngineWillConnectOutput(engine_device_, engine_device_.mainMixerNode,
                                                     outputNode(), engine_output_format, context);
      if (result != 0) {
        LOGE() << "Call to OnEngineWillConnectOutput returned error: " << result;
        return rollback(result);
      }
    }

  } else if ((state.prev.IsOutputEnabled() && !state.next.IsOutputEnabled()) &&
             !state.IsEngineRecreateRequired()) {
    LOGI() << "Disabling output for AVAudioEngine...";
    RTC_DCHECK(!engine_device_.running);

    // Detach source node
    if (source_node_ != nil) {
      if (![engine_device_.attachedNodes containsObject:source_node_]) {
        LOGW() << "Attempted to detach a node that wasn't attached to the engine";
      } else {
        @try {
          [engine_device_ detachNode:source_node_];
        } @catch (NSException* exception) {
          LOGW() << "Failed to detach node: " << exception.reason.UTF8String;
        }
      }
      source_node_ = nil;
    }
  }

  // --------------------------------------------------------------------------------------------
  // Step: Enable input
  //
  if (state.next.IsInputEnabled() &&
      (!state.prev.IsInputEnabled() || state.IsEngineRecreateRequired())) {
    LOGI() << "Enabling input for AVAudioEngine...";
    RTC_DCHECK(!engine_device_.running);

    // Apple: When the engine renders to and from an audio device, the AVAudioSession category and
    // the availability of hardware determines whether an app performs input (for example, input
    // hardware isn’t available in tvOS). Check the input node’s input format (specifically, the
    // hardware format) for a nonzero sample rate and channel count to see if input is in an enabled
    // state. Trying to perform input through the input node when it isn’t available or in an
    // enabled state causes the engine to throw an error (when possible) or an exception.
    AVAudioFormat* input_node_format = [inputNode() outputFormatForBus:0];
    // Example formats:
    // Airpods: 1 ch,  24000 Hz, Float32
    // Mac: 9 ch,  48000 Hz, Float32
    LOGI() << "Input format sampleRate: " << input_node_format.sampleRate
           << " channels: " << input_node_format.channelCount
           << " formatID: " << input_node_format.streamDescription->mFormatID
           << " formatFlags: " << input_node_format.streamDescription->mFormatFlags
           << " bytesPerPacket: " << input_node_format.streamDescription->mBytesPerPacket
           << " framesPerPacket: " << input_node_format.streamDescription->mFramesPerPacket
           << " bytesPerFrame: " << input_node_format.streamDescription->mBytesPerFrame
           << " channelsPerFrame: " << input_node_format.streamDescription->mChannelsPerFrame
           << " bitsPerChannel: " << input_node_format.streamDescription->mBitsPerChannel;

    // Check if the input node format is valid (has non-zero sample rate and channel count)
    if (input_node_format.sampleRate == 0 || input_node_format.channelCount == 0) {
      LOGE() << "Input device not available, sampleRate=" << input_node_format.sampleRate
             << ", channelCount=" << input_node_format.channelCount;
      return rollback(kAudioEngineRecordingDeviceNotAvailableError);
    }

    input_mixer_node_ = [[AVAudioMixerNode alloc] init];
    [engine_device_ attachNode:input_mixer_node_];

    // When VoiceProcessingIO is enabled, channels must be reduced from Mac's default 9 channels
    // to 2 or lower.
    AVAudioFormat* engine_input_format = [[AVAudioFormat alloc]
        initWithCommonFormat:input_node_format.commonFormat  // Usually float32
                  sampleRate:input_node_format.sampleRate
                    channels:1
                 interleaved:input_node_format.interleaved];

    AVAudioFormat* rtc_input_format =
        [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16
                                         sampleRate:engine_input_format.sampleRate
                                           channels:1
                                        interleaved:YES];

    audio_device_buffer_->SetRecordingSampleRate(rtc_input_format.sampleRate);
    audio_device_buffer_->SetRecordingChannels(rtc_input_format.channelCount);
    RTC_DCHECK(audio_device_buffer_ != nullptr);
    fine_audio_buffer_.reset(new FineAudioBuffer(audio_device_buffer_.get()));

    // Prepare Float32 -> Int16 converter.
    if (converter_ref_ == nullptr) {
      OSStatus err = AudioConverterNew(engine_input_format.streamDescription,
                                       rtc_input_format.streamDescription, &converter_ref_);
      RTC_DCHECK(err == noErr);
    }

    // Prepare buffer for Int16 converter.
    if (converter_buffer_ == nil) {
      converter_buffer_ = [[AVAudioPCMBuffer alloc] initWithPCMFormat:rtc_input_format
                                                        frameCapacity:kMaximumFramesPerBuffer];
    }

    // Convert to Int16 buffers within the sink block.
    AVAudioSinkNodeReceiverBlock sink_block =
        ^OSStatus(const AudioTimeStamp* timestamp, AVAudioFrameCount frameCount,
                  const AudioBufferList* inputData) {
          RTC_DCHECK(inputData->mNumberBuffers == 1);

          AudioBufferList* converter_buffer_abl =
              const_cast<AudioBufferList*>(converter_buffer_.audioBufferList);
          RTC_DCHECK(converter_buffer_abl->mNumberBuffers == inputData->mNumberBuffers);

          // Fails for conversions where there is a variation between the input and output data
          // buffer sizes.
          converter_buffer_abl->mBuffers[0].mDataByteSize = inputData->mBuffers[0].mDataByteSize;

          RTC_DCHECK(converter_buffer_abl->mBuffers[0].mDataByteSize ==
                     inputData->mBuffers[0].mDataByteSize);

          OSStatus err = AudioConverterConvertComplexBuffer(converter_ref_, frameCount, inputData,
                                                            converter_buffer_abl);
          RTC_DCHECK(err == noErr);

          const int16_t* rtc_buffer = (int16_t*)converter_buffer_abl->mBuffers[0].mData;  // Float32
          const int64_t capture_time_ns = timestamp->mHostTime * machTickUnitsToNanoseconds_;

          fine_audio_buffer_->DeliverRecordedData(
              webrtc::ArrayView<const int16_t>(rtc_buffer, frameCount), kFixedRecordDelayEstimate,
              capture_time_ns);

          return noErr;
        };

    NSMutableArray<AVAudioConnectionPoint*>* input_mixer_connections = [NSMutableArray array];

    if (observer_ != nullptr) {
      NSDictionary* context = @{
        kAudioEngineInputMixerNodeKey : input_mixer_node_,
      };
      int32_t result = observer_->OnEngineWillConnectInput(
          engine_device_, inputNode(), input_mixer_node_, engine_input_format, context);
      if (result != 0) {
        LOGE() << "Call to OnEngineWillConnectInput returned error: " << result;
        return rollback(result);
      }
    }

    for (AVAudioNodeBus bus = 0; bus < input_mixer_node_.numberOfInputs; bus++) {
      AVAudioConnectionPoint* cp = [engine_device_ inputConnectionPointForNode:input_mixer_node_
                                                                      inputBus:bus];
      if (cp) {
        [input_mixer_connections addObject:cp];
      }
    }

    LOGI() << "input mixer connection count: " << input_mixer_connections.count;
    if (input_mixer_connections.count == 0) {
      LOGI() << "Nothing connected to input mixer, connecting input node...";
      // Default implementation.
      [engine_device_ connect:inputNode() to:input_mixer_node_ format:engine_input_format];
    }

    sink_node_ = [[AVAudioSinkNode alloc] initWithReceiverBlock:sink_block];
    [engine_device_ attachNode:sink_node_];

    [engine_device_ connect:input_mixer_node_ to:sink_node_ format:engine_input_format];

  } else if ((state.prev.IsInputEnabled() && !state.next.IsInputEnabled()) &&
             !state.IsEngineRecreateRequired()) {
    LOGI() << "Disabling input for AVAudioEngine...";
    RTC_DCHECK(!engine_device_.running);

    // If disabling input, always unmute the voice-processing input mute.
    if (inputNode().voiceProcessingEnabled && inputNode().voiceProcessingInputMuted) {
      LOGI() << "Update mute (voice processing) unmuting vp for stop-recording";
      inputNode().voiceProcessingInputMuted = false;
    }

    // Detach input mixer node
    if (input_mixer_node_ != nil) {
      if (![engine_device_.attachedNodes containsObject:input_mixer_node_]) {
        LOGW() << "Attempted to detach a node that wasn't attached to the engine";
      } else {
        @try {
          [engine_device_ detachNode:input_mixer_node_];
        } @catch (NSException* exception) {
          LOGW() << "Failed to detach node: " << exception.reason.UTF8String;
        }
        input_mixer_node_ = nil;
      }
    }

    // Detach sink node
    if (sink_node_ != nil) {
      if (![engine_device_.attachedNodes containsObject:sink_node_]) {
        LOGW() << "Attempted to detach a node that wasn't attached to the engine";
      } else {
        @try {
          [engine_device_ detachNode:sink_node_];
        } @catch (NSException* exception) {
          LOGW() << "Failed to detach node: " << exception.reason.UTF8String;
        }
        sink_node_ = nil;
      }
    }

    // Dispose Float32 -> Int16 converter.
    if (converter_ref_ != nullptr) {
      OSStatus err = AudioConverterDispose(converter_ref_);
      RTC_DCHECK(err == noErr);
      converter_ref_ = nullptr;
    }

    // Release buffer for Int16 converter.
    converter_buffer_ = nil;
  }

  // --------------------------------------------------------------------------------------------
  // Step: Trigger "engine did disable" event
  //
  if (state.DidAnyDisable() && observer_ != nullptr) {
    int32_t result = observer_->OnEngineDidDisable(engine_device_, state.next.IsOutputEnabled(),
                                                   state.next.IsInputEnabled());
    if (result != 0) {
      LOGE() << "Call to OnEngineDidDisable returned error: " << result;
      return rollback(result);
    }
  }

  // --------------------------------------------------------------------------------------------
  // Step: Run-time mute toggling if vp mode.
  //
  if (state.next.mute_mode == MuteMode::VoiceProcessing && state.next.IsInputEnabled() &&
      inputNode().voiceProcessingEnabled &&
      inputNode().voiceProcessingInputMuted != state.next.input_muted) {
    LOGI() << "Update mute (voice processing) runtime update" << state.next.input_muted;
    inputNode().voiceProcessingInputMuted = state.next.input_muted;
  }

  // --------------------------------------------------------------------------------------------
  // Step: Run-time mute toggling if mixer mute mode.
  //
  if (state.next.mute_mode == MuteMode::InputMixer && state.next.IsInputEnabled() &&
      input_mixer_node_ != nil) {
    // Only update if the volume has changed.
    float mixer_volume = state.next.input_muted ? 0.0f : 1.0f;
    if (input_mixer_node_.outputVolume != mixer_volume) {
      LOGI() << "Update mute (input mixer) runtime update" << state.next.input_muted;
      input_mixer_node_.outputVolume = mixer_volume;
    }
  }

  // --------------------------------------------------------------------------------------------
  // Step: Configure other audio ducking
  //
#if !TARGET_OS_TV
  if (state.next.IsInputEnabled() && inputNode().voiceProcessingEnabled &&
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
      inputNode().voiceProcessingOtherAudioDuckingConfiguration = ducking_config;
    }
  }
#endif

  // --------------------------------------------------------------------------------------------
  // Step: Bypass voice processing
  //
  if (state.next.IsInputEnabled() && inputNode().voiceProcessingEnabled &&
      inputNode().voiceProcessingBypassed != state.next.voice_processing_bypassed) {
    LOGI() << "setting voiceProcessingBypassed: " << state.next.voice_processing_bypassed;
    inputNode().voiceProcessingBypassed = state.next.voice_processing_bypassed;
  }

  // --------------------------------------------------------------------------------------------
  // Step: Configure AGC
  //
  if (state.next.IsInputEnabled() && inputNode().voiceProcessingEnabled &&
      inputNode().voiceProcessingAGCEnabled != state.next.voice_processing_agc_enabled) {
    LOGI() << "setting voiceProcessingAGCEnabled: " << state.next.voice_processing_agc_enabled;
    inputNode().voiceProcessingAGCEnabled = state.next.voice_processing_agc_enabled;
  }

  // --------------------------------------------------------------------------------------------
  // Step: Configure device (macOS only)
  //
#if TARGET_OS_OSX
  if (state.next.IsAnyEnabled() &&
      (!state.prev.IsAnyEnabled() || state.IsEngineRecreateRequired())) {
    if (state.next.IsInputEnabled()) {
      uint32_t input_device_id = state.next.input_device_id;
      if (input_device_id == kAudioObjectUnknown) {
        LOGI() << "Using default input device";
      } else {
        auto input_device_name = mac_audio_utils::GetDeviceName(input_device_id);
        LOGI() << "Setting input device: " << input_device_name.value_or("Unknown") << " ("
               << input_device_id << ")";
        AudioUnit inputUnit = inputNode().audioUnit;
        OSStatus err = AudioUnitSetProperty(inputUnit, kAudioOutputUnitProperty_CurrentDevice,
                                            kAudioUnitScope_Global, 1, &input_device_id,
                                            sizeof(input_device_id));
        if (err != noErr) {
          LOGE() << "Failed to set input device: " << input_device_id;
        }
      }
    }

    if (state.next.IsOutputEnabled()) {
      uint32_t output_deviceId = state.next.output_device_id;
      if (output_deviceId == kAudioObjectUnknown) {
        LOGI() << "Using default output device";
      } else {
        auto output_device_name = mac_audio_utils::GetDeviceName(output_deviceId);
        LOGI() << "Setting output device: " << output_device_name.value_or("Unknown") << " ("
               << output_deviceId << ")";
        AudioUnit outputUnit = outputNode().audioUnit;
        OSStatus err = AudioUnitSetProperty(outputUnit, kAudioOutputUnitProperty_CurrentDevice,
                                            kAudioUnitScope_Global, 0, &output_deviceId,
                                            sizeof(output_deviceId));
        if (err != noErr) {
          LOGE() << "Failed to set output device: " << output_deviceId;
        }
      }
    }
  }
#endif

  // --------------------------------------------------------------------------------------------
  // Step: Start playout buffer
  //
  if (state.next.IsOutputEnabled() && !audio_device_buffer_->IsPlaying()) {
    if (engine_device_ != nullptr) {
      // Rendering must be stopped first.
      RTC_DCHECK(!engine_device_.running);
    }
    LOGI() << "Starting Playout buffer...";
    audio_device_buffer_->StartPlayout();
    fine_audio_buffer_->ResetPlayout();
  }

  // --------------------------------------------------------------------------------------------
  // Step: Start recording buffer
  //
  if (state.next.IsInputEnabled() && !audio_device_buffer_->IsRecording()) {
    if (engine_device_ != nullptr) {
      // Rendering must be stopped first.
      RTC_DCHECK(!engine_device_.running);
    }
    LOGI() << "Starting Record buffer...";
    audio_device_buffer_->StartRecording();
    fine_audio_buffer_->ResetRecord();
  }

  // --------------------------------------------------------------------------------------------
  // Step: Start engine
  //
  if (state.next.IsAnyRunning()) {
    if (!state.prev.IsAnyRunning() || state.DidEndInterruption() ||
        state.IsEngineRestartRequired() || state.IsEngineRecreateRequired()) {
      if (observer_ != nullptr) {
        int32_t result = observer_->OnEngineWillStart(engine_device_, state.next.IsOutputEnabled(),
                                                      state.next.IsInputEnabled());
        if (result != 0) {
          LOGE() << "Call to OnEngineWillStart returned error: " << result;
          return rollback(result);
        }
      }

      LOGI() << "Starting AVAudioEngine...";
      BOOL start_result = false;
      int start_retry_count = 0;

      // Workaround for error -66637, when recovering from interruptions with categoryMode:
      // .mixWithOthers.
      while (!start_result && start_retry_count < kStartEngineMaxRetries) {
        if (start_retry_count > 0) {
          LOGW() << "Retrying engine start (attempt " << (start_retry_count + 1) << "/"
                 << kStartEngineMaxRetries << ")";
          usleep(kStartEngineRetryDelayMs * 1000);
        }

        NSString* error_string = nil;

        @try {
#if TARGET_OS_OSX
          // Workaround for engine not starting in some cases when other apps are using voice
          // processing already.
          // TODO: Find a better workaround, or a cleaner way to wait the vp config is complete.
          [engine_device_ prepare];

          LOGI() << "Sleeping for 0.1 seconds...";
          usleep(100000);  // 0.1 seconds
#endif

          NSError* error = nil;
          start_result = [engine_device_ startAndReturnError:&error];
          if (!start_result && error != nil) {
            error_string = error.localizedDescription;
          }
        } @catch (NSException* exception) {
          start_result = false;
          error_string = exception.reason ?: @"Unknown exception";
        }

        if (!start_result) {
          if (error_string != nil) {
            LOGE() << "Failed to start engine: " << error_string.UTF8String;
          }
          start_retry_count++;
        }
      }

      if (start_result) {
        RTC_DCHECK(configuration_observer_ == nullptr);
        // Add observer for configuration changes
        NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
        configuration_observer_ = (__bridge_retained void*)[center
            addObserverForName:AVAudioEngineConfigurationChangeNotification
                        object:engine_device_
                         queue:nil
                    usingBlock:^(NSNotification* notification) {
                      LOGI() << "AVAudioEngineConfigurationChangeNotification engineIsRunning: "
                             << engine_device_.running;
                      // Only re-configure if engine stopped.
                      if (!engine_device_.running) {
                        ReconfigureEngine();
                      }
                    }];

      } else {
        LOGE() << "Failed to start engine after " << kStartEngineMaxRetries << " attempts";
        DebugAudioEngine();
      }
    }
  }

  // --------------------------------------------------------------------------------------------
  // Step: Release AVAudioEngine
  //
  if (state.prev.IsAnyEnabled() && !state.next.IsAnyEnabled()) {
    RTC_DCHECK(engine_device_ != nullptr);

    if (observer_ != nullptr) {
      int32_t result = observer_->OnEngineWillRelease(engine_device_);
      if (result != 0) {
        LOGE() << "Call to OnEngineWillRelease returned error: " << result;
        return rollback(result);
      }
    }

    LOGI() << "Releasing AVAudioEngine...";
    engine_device_ = nil;
  }

  return 0;
}

// ----------------------------------------------------------------------------------------------------
// Private - EngineState

void AudioEngineDevice::StartRenderLoop() {
  RTC_DCHECK_RUN_ON(render_thread_.get());

  const double sample_rate = manual_render_rtc_format_.sampleRate;
  const size_t frames_per_buffer = static_cast<size_t>(sample_rate / 100);  // 10ms chunks
  const size_t buffer_size = frames_per_buffer * kAudioSampleSize;
  const int chunk_ms =
      static_cast<int>(std::round(1000.0 * static_cast<double>(frames_per_buffer) / sample_rate));
  int64_t next_wakeup_ms = rtc::TimeMillis();

  while (!render_thread_->IsQuitting()) {
    // Read (Output)
    RTC_DCHECK(read_buffer_ != nullptr);
    AudioBufferList* read_abl = const_cast<AudioBufferList*>(read_buffer_.audioBufferList);
    read_abl->mBuffers[0].mDataByteSize = buffer_size;

    RTC_DCHECK(read_abl->mNumberBuffers == 1);
    int16_t* const read_rtc_buffer =
        static_cast<int16_t*>(static_cast<void*>(read_abl->mBuffers[0].mData));

    // Call GetPlayoutData to pull frames into rtc audio stack even though we won't use it here.
    fine_audio_buffer_->GetPlayoutData(
        webrtc::ArrayView<int16_t>(read_rtc_buffer, frames_per_buffer), kFixedPlayoutDelayEstimate);

    // Render (Input)
    RTC_DCHECK(render_buffer_ != nullptr);
    AudioBufferList* render_abl = const_cast<AudioBufferList*>(render_buffer_.audioBufferList);
    render_abl->mBuffers[0].mDataByteSize = buffer_size;

    OSStatus err = noErr;
    AVAudioEngineManualRenderingStatus result = render_block_(frames_per_buffer, render_abl, &err);

    if (result == AVAudioEngineManualRenderingStatusSuccess) {
      RTC_DCHECK(render_abl->mNumberBuffers == 1);
      const int16_t* rtc_buffer =
          static_cast<const int16_t*>(static_cast<const void*>(render_abl->mBuffers[0].mData));

      const uint64_t capture_time = mach_absolute_time();
      const int64_t capture_time_ns = capture_time * machTickUnitsToNanoseconds_;

      fine_audio_buffer_->DeliverRecordedData(
          webrtc::ArrayView<const int16_t>(rtc_buffer, frames_per_buffer),
          kFixedRecordDelayEstimate, capture_time_ns);
    } else {
      LOGW() << "Render error: " << err << " frames: " << frames_per_buffer;
    }

    if (!render_thread_->IsQuitting()) {
      next_wakeup_ms += chunk_ms;
      const int64_t now_ms = rtc::TimeMillis();
      const int64_t sleep_ms = next_wakeup_ms - now_ms;
      if (sleep_ms > 0) {
        render_thread_->SleepMs(static_cast<int>(sleep_ms));
      }
    }
  }
}

// ----------------------------------------------------------------------------------------------------
// Private - Device access

#if TARGET_OS_OSX

void AudioEngineDevice::UpdateAllDeviceIDs() {
  using namespace webrtc::mac_audio_utils;

  input_device_ids_.clear();
  output_device_ids_.clear();
  input_device_labels_.clear();
  output_device_labels_.clear();

  std::vector<AudioObjectID> all_device_ids = GetAllAudioDeviceIDs();

  for (AudioObjectID device_id : all_device_ids) {
    if (IsInputDevice(device_id)) {
      input_device_ids_.push_back(device_id);
      auto label = GetDeviceLabel(device_id, true);
      if (label) {
        input_device_labels_.push_back(*label);
      } else {
        input_device_labels_.push_back("Unknown Input Device");
      }
    }

    if (IsOutputDevice(device_id)) {
      output_device_ids_.push_back(device_id);
      auto label = GetDeviceLabel(device_id, false);
      if (label) {
        output_device_labels_.push_back(*label);
      } else {
        output_device_labels_.push_back("Unknown Output Device");
      }
    }
  }
}

#endif

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
    RTC_DCHECK_RUN_ON(thread_);
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

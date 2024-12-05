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

#include "audio_device_audioengine.h"

#include <mach/mach_time.h>
#include <cmath>

#include "api/array_view.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "helpers.h"
#include "modules/audio_device/fine_audio_buffer.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/field_trial.h"
#include "system_wrappers/include/metrics.h"

#import "base/RTCLogging.h"

#if defined(WEBRTC_IOS)
#import "components/audio/RTCAudioSession+Private.h"
#import "components/audio/RTCAudioSession.h"
#import "components/audio/RTCAudioSessionConfiguration.h"
#import "components/audio/RTCNativeAudioSessionDelegateAdapter.h"
#endif

namespace webrtc {
namespace ios_adm {

#define LOGI() RTC_LOG(LS_INFO) << "AudioDeviceAudioEngine::"

#define LOG_AND_RETURN_IF_ERROR(error, message)    \
  do {                                             \
    OSStatus err = error;                          \
    if (err) {                                     \
      RTC_LOG(LS_ERROR) << message << ": " << err; \
      return false;                                \
    }                                              \
  } while (0)

#define LOG_IF_ERROR(error, message)               \
  do {                                             \
    OSStatus err = error;                          \
    if (err) {                                     \
      RTC_LOG(LS_ERROR) << message << ": " << err; \
    }                                              \
  } while (0)

// Hardcoded delay estimates based on real measurements.
// TODO(henrika): these value is not used in combination with built-in AEC.
// Can most likely be removed.
const UInt16 kFixedPlayoutDelayEstimate = 30;
const UInt16 kFixedRecordDelayEstimate = 30;

const uint32_t N_REC_SAMPLES_PER_SEC = 48000;
const uint32_t N_PLAY_SAMPLES_PER_SEC = 48000;
const uint32_t N_REC_CHANNELS = 1;   // default is mono recording
const uint32_t N_PLAY_CHANNELS = 1;  // default is stereo playout

using ios::CheckAndLogError;

AudioDeviceAudioEngine::AudioDeviceAudioEngine(bool bypass_voice_processing)
    : bypass_voice_processing_(bypass_voice_processing),
      audio_device_buffer_(nullptr),
      recording_is_initialized_(false),
      recording_(0),
      playout_is_initialized_(false),
      playing_(0),
      initialized_(false),
      is_interrupted_(false),
      has_configured_session_(false),
      audio_engine_input_attached_(false) {
  LOGI() << "bypass_voice_processing " << bypass_voice_processing_;

  io_thread_checker_.Detach();
  thread_ = rtc::Thread::Current();

#if defined(WEBRTC_IOS)
  audio_session_observer_ =
      [[RTC_OBJC_TYPE(RTCNativeAudioSessionDelegateAdapter) alloc] initWithObserver:this];
#endif

  mach_timebase_info_data_t tinfo;
  mach_timebase_info(&tinfo);
  machTickUnitsToNanoseconds_ = (double)tinfo.numer / tinfo.denom;

  // Create AVAudioEngine early so it can be accessed externally.
  EngineCreate();
}

AudioDeviceAudioEngine::~AudioDeviceAudioEngine() {
  RTC_DCHECK_RUN_ON(thread_);

  safety_->SetNotAlive();

  Terminate();
  audio_session_observer_ = nil;
}

void AudioDeviceAudioEngine::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
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

bool AudioDeviceAudioEngine::Initialized() const {
  RTC_DCHECK_RUN_ON(thread_);
  return initialized_;
}

AudioDeviceGeneric::InitStatus AudioDeviceAudioEngine::Init() {
  LOGI() << "Init";
  io_thread_checker_.Detach();

  RTC_DCHECK_RUN_ON(thread_);
  if (initialized_) {
    return InitStatus::OK;
  }

#if defined(WEBRTC_IOS)
  // Store the preferred sample rate and preferred number of channels already
  // here. They have not been set and confirmed yet since configureForWebRTC
  // is not called until audio is about to start. However, it makes sense to
  // store the parameters now and then verify at a later stage.
  RTC_OBJC_TYPE(RTCAudioSessionConfiguration)* config =
      [RTC_OBJC_TYPE(RTCAudioSessionConfiguration) webRTCConfiguration];
  playout_parameters_.reset(config.sampleRate, config.outputNumberOfChannels);
  record_parameters_.reset(config.sampleRate, config.inputNumberOfChannels);
#endif

  AVAudioFormat* output_format = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16
                                                                  sampleRate:48000.0
                                                                    channels:1
                                                                 interleaved:NO];

  // Prepare SourceNode

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

  source_node_ = [[AVAudioSourceNode alloc] initWithFormat:output_format renderBlock:source_block];

  // Prepare SinkNode

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

  AVAudioFormat* input_format = [audio_engine_.inputNode outputFormatForBus:0];
  LOGI() << "Input format - sample rate: " << input_format.sampleRate
         << ", channels: " << input_format.channelCount
         << ", format flags: " << input_format.streamDescription->mFormatFlags
         << ", format ID: " << input_format.streamDescription->mFormatID
         << ", bytes per frame: " << input_format.streamDescription->mBytesPerFrame;

  AVAudioFormat* rtc_record_format =
      [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatInt16
                                       sampleRate:input_format.sampleRate
                                         channels:input_format.channelCount
                                      interleaved:YES];
  LOGI() << "RTC format - sample rate: " << rtc_record_format.sampleRate
         << ", channels: " << rtc_record_format.channelCount
         << ", format flags: " << rtc_record_format.streamDescription->mFormatFlags
         << ", format ID: " << rtc_record_format.streamDescription->mFormatID
         << ", bytes per frame: " << rtc_record_format.streamDescription->mBytesPerFrame;

  // Connect source node
  [audio_engine_ attachNode:source_node_];
  [audio_engine_ attachNode:sink_node_];

  [audio_engine_ connect:source_node_ to:audio_engine_.mainMixerNode format:nil];
  // [audio_engine_ connect:audio_engine_.inputNode to:sink_node_ format:input_format];
  // [audio_engine_ connect:audio_engine_.inputNode to:input_mixer_node_ format:input_format];
  // [audio_engine_ connect:input_mixer_node_ to:sink_node_ format:rtc_record_format];

  // NSError* error = nil;
  // // BOOL set_vp_result = [audio_engine_.inputNode setVoiceProcessingEnabled:YES error:&error];
  // // if (!set_vp_result) {
  // //   NSLog(@"setVoiceProcessingEnabled error: %@", error.localizedDescription);
  // //   RTC_DCHECK(set_vp_result);
  // // }
  // // LOGI() << "setVoiceProcessingEnabled result: " << set_vp_result ? "YES" : "NO";

  // [audio_engine_ startAndReturnError:&error];

  initialized_ = true;

  return InitStatus::OK;
}

int32_t AudioDeviceAudioEngine::Terminate() {
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

bool AudioDeviceAudioEngine::PlayoutIsInitialized() const {
  RTC_DCHECK_RUN_ON(thread_);
  return playout_is_initialized_;
}

bool AudioDeviceAudioEngine::Playing() const { return playing_.load(); }

int32_t AudioDeviceAudioEngine::InitPlayout() {
  LOGI() << "InitPlayout";
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(initialized_);
  RTC_DCHECK(!playout_is_initialized_);
  RTC_DCHECK(!playing_.load());

  if (playout_is_initialized_) {
    // Allow InitPlayout() to be called multiple times.
    return 0;
  }

  if (!recording_is_initialized_) {
    // recording not initialized yet, init with no input
    if (!InitPlayOrRecord(false)) {
      RTC_LOG_F(LS_ERROR) << "InitPlayOrRecord failed for InitPlayout!";
      return -1;
    }
  }

  playout_is_initialized_ = true;

  return 0;
}

int32_t AudioDeviceAudioEngine::StartPlayout() {
  LOGI() << "StartPlayout";
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(playout_is_initialized_);
  RTC_DCHECK(!playing_.load());

  if (fine_audio_buffer_) {
    fine_audio_buffer_->ResetPlayout();
  }

  return 0;
}

int32_t AudioDeviceAudioEngine::StopPlayout() {
  LOGI() << "StopPlayout";
  RTC_DCHECK_RUN_ON(thread_);
  if (!playout_is_initialized_ || !playing_.load()) {
    return 0;
  }

  return 0;
}

// ----------------------------------------------------------------------------------------------------
// Recording

bool AudioDeviceAudioEngine::RecordingIsInitialized() const {
  RTC_DCHECK_RUN_ON(thread_);
  return recording_is_initialized_;
}

bool AudioDeviceAudioEngine::Recording() const { return recording_.load(); }

int32_t AudioDeviceAudioEngine::InitRecording() {
  LOGI() << "InitRecording";
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(initialized_);
  RTC_DCHECK(!recording_is_initialized_);
  RTC_DCHECK(!recording_.load());

  if (recording_is_initialized_) {
    // Allow InitRecording() to be called multiple times.
    return 0;
  }

  if (!playout_is_initialized_) {
    // playout not initialized yet, init with input
    if (!InitPlayOrRecord(true)) {
      RTC_LOG_F(LS_ERROR) << "InitPlayOrRecord failed for InitRecording!";
      return -1;
    }
  }

  recording_is_initialized_ = true;

  return 0;
}

int32_t AudioDeviceAudioEngine::StartRecording() {
  LOGI() << "StartRecording";
  RTC_DCHECK_RUN_ON(thread_);
  RTC_DCHECK(recording_is_initialized_);
  RTC_DCHECK(!recording_.load());

  if (fine_audio_buffer_) {
    fine_audio_buffer_->ResetRecord();
  }

  return 0;
}

int32_t AudioDeviceAudioEngine::StopRecording() {
  LOGI() << "StopRecording";
  RTC_DCHECK_RUN_ON(thread_);

  if (!recording_is_initialized_ || !recording_.load()) {
    return 0;
  }

  return 0;
}


// int AudioDeviceAudioEngine::GetPlayoutAudioParameters(AudioParameters* params) const {
//   LOGI() << "GetPlayoutAudioParameters";
//   RTC_DCHECK(playout_parameters_.is_valid());
//   RTC_DCHECK_RUN_ON(thread_);
//   *params = playout_parameters_;
//   return 0;
// }

// int AudioDeviceAudioEngine::GetRecordAudioParameters(AudioParameters* params) const {
//   LOGI() << "GetRecordAudioParameters";
//   RTC_DCHECK(record_parameters_.is_valid());
//   RTC_DCHECK_RUN_ON(thread_);
//   *params = record_parameters_;
//   return 0;
// }

// ----------------------------------------------------------------------------------------------------
// AudioSessionObserver

void AudioDeviceAudioEngine::OnInterruptionBegin() {
  RTC_DCHECK(thread_);
  LOGI() << "OnInterruptionBegin";
}

void AudioDeviceAudioEngine::OnInterruptionEnd() {
  RTC_DCHECK(thread_);
  LOGI() << "OnInterruptionEnd";
}

void AudioDeviceAudioEngine::OnValidRouteChange() { RTC_DCHECK(thread_); }

void AudioDeviceAudioEngine::OnCanPlayOrRecordChange(bool can_play_or_record) {
  RTC_DCHECK(thread_);
}

void AudioDeviceAudioEngine::OnChangedOutputVolume() { RTC_DCHECK(thread_); }

bool AudioDeviceAudioEngine::InitPlayOrRecord(bool enable_input) {
  LOGI() << "InitPlayOrRecord";
  RTC_DCHECK_RUN_ON(thread_);
  return true;
}

void AudioDeviceAudioEngine::ShutdownPlayOrRecord() {
  LOGI() << "ShutdownPlayOrRecord";
  RTC_DCHECK_RUN_ON(thread_);
}

bool AudioDeviceAudioEngine::IsInterrupted() { return is_interrupted_; }

// ----------------------------------------------------------------------------------------------------
// Not Implemented

int32_t AudioDeviceAudioEngine::ActiveAudioLayer(AudioDeviceModule::AudioLayer& audioLayer) const {
  audioLayer = AudioDeviceModule::kPlatformDefaultAudio;
  return 0;
}

int32_t AudioDeviceAudioEngine::InitSpeaker() { return 0; }

bool AudioDeviceAudioEngine::SpeakerIsInitialized() const { return true; }

int32_t AudioDeviceAudioEngine::SpeakerVolumeIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceAudioEngine::SetSpeakerVolume(uint32_t volume) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceAudioEngine::SpeakerVolume(uint32_t& volume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceAudioEngine::MaxSpeakerVolume(uint32_t& maxVolume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceAudioEngine::MinSpeakerVolume(uint32_t& minVolume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceAudioEngine::SpeakerMuteIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceAudioEngine::SetSpeakerMute(bool enable) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceAudioEngine::SpeakerMute(bool& enabled) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceAudioEngine::InitMicrophone() {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "InitMicrophone";

  return 0;
}

bool AudioDeviceAudioEngine::MicrophoneIsInitialized() const {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "MicrophoneIsInitialized";

  return true;
}

// ----------------------------------------------------------------------------------------------------
// Microphone Muting

int32_t AudioDeviceAudioEngine::MicrophoneMuteIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceAudioEngine::SetMicrophoneMute(bool enable) {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "SetMicrophoneMute";

  if (!audio_engine_input_attached_) {
    LOGI() << "Engine input not attached";
    return -1;
  }

  audio_engine_.inputNode.voiceProcessingInputMuted = enable;

  return 0;
}

int32_t AudioDeviceAudioEngine::MicrophoneMute(bool& enabled) const {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "MicrophoneMute";

  if (!audio_engine_input_attached_) {
    LOGI() << "Engine input not attached";
    return -1;
  }

  enabled = audio_engine_.inputNode.voiceProcessingInputMuted;

  return 0;
}

// ----------------------------------------------------------------------------------------------------
// Stereo Playout

int32_t AudioDeviceAudioEngine::StereoPlayoutIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceAudioEngine::SetStereoPlayout(bool enable) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return -1;
}

int32_t AudioDeviceAudioEngine::StereoPlayout(bool& enabled) const {
  enabled = false;
  return 0;
}

// ----------------------------------------------------------------------------------------------------
// Stereo Recording

int32_t AudioDeviceAudioEngine::StereoRecordingIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceAudioEngine::SetStereoRecording(bool enable) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return -1;
}

int32_t AudioDeviceAudioEngine::StereoRecording(bool& enabled) const {
  enabled = false;
  return 0;
}

// ----------------------------------------------------------------------------------------------------
// Microphone Volume

int32_t AudioDeviceAudioEngine::MicrophoneVolumeIsAvailable(bool& available) {
  available = false;
  return 0;
}

int32_t AudioDeviceAudioEngine::SetMicrophoneVolume(uint32_t volume) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceAudioEngine::MicrophoneVolume(uint32_t& volume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceAudioEngine::MaxMicrophoneVolume(uint32_t& maxVolume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceAudioEngine::MinMicrophoneVolume(uint32_t& minVolume) const {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

// ----------------------------------------------------------------------------------------------------
// Playout Device

int32_t AudioDeviceAudioEngine::PlayoutIsAvailable(bool& available) {
  available = true;
  return 0;
}

int32_t AudioDeviceAudioEngine::SetPlayoutDevice(uint16_t index) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceAudioEngine::SetPlayoutDevice(AudioDeviceModule::WindowsDeviceType) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceAudioEngine::PlayoutDeviceName(uint16_t index, char name[kAdmMaxDeviceNameSize],
                                                  char guid[kAdmMaxGuidSize]) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int16_t AudioDeviceAudioEngine::PlayoutDevices() {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return (int16_t)1;
}

// ----------------------------------------------------------------------------------------------------
// Recording Device

int32_t AudioDeviceAudioEngine::RecordingDeviceName(uint16_t index,
                                                    char name[kAdmMaxDeviceNameSize],
                                                    char guid[kAdmMaxGuidSize]) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceAudioEngine::SetRecordingDevice(uint16_t index) {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return 0;
}

int32_t AudioDeviceAudioEngine::SetRecordingDevice(AudioDeviceModule::WindowsDeviceType) {
  RTC_DCHECK_NOTREACHED() << "Not implemented";
  return -1;
}

int32_t AudioDeviceAudioEngine::RecordingIsAvailable(bool& available) {
  available = true;
  return 0;
}

int16_t AudioDeviceAudioEngine::RecordingDevices() {
  RTC_LOG_F(LS_WARNING) << "Not implemented";
  return (int16_t)1;
}

// ----------------------------------------------------------------------------------------------------
// Private - Engine Related

bool AudioDeviceAudioEngine::EngineCreate() {
  RTC_DCHECK_RUN_ON(thread_);
  LOGI() << "EngineCreate";

  if (audio_engine_ != nil) {
    RTC_LOG_F(LS_WARNING) << "Engine already created";
    return 0;
  }

  audio_engine_ = [[AVAudioEngine alloc] init];

  // Prepare InputMixerNode

  input_mixer_node_ = [[AVAudioMixerNode alloc] init];
  [audio_engine_ attachNode:input_mixer_node_];

  return 0;
}

bool AudioDeviceAudioEngine::EngineCleanUp() {
  audio_engine_ = [[AVAudioEngine alloc] init];

  // Prepare InputMixerNode

  input_mixer_node_ = [[AVAudioMixerNode alloc] init];
  [audio_engine_ attachNode:input_mixer_node_];

  return 0;
}

bool AudioDeviceAudioEngine::AttachEngineInput() { return 0; }

bool AudioDeviceAudioEngine::DetachEngineInput() { return 0; }

}  // namespace ios_adm
}  // namespace webrtc

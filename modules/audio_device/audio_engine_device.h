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

#ifndef SDK_OBJC_NATIVE_SRC_AUDIO_AUDIO_DEVICE_AUDIOENGINE_H_
#define SDK_OBJC_NATIVE_SRC_AUDIO_AUDIO_DEVICE_AUDIOENGINE_H_

#include <atomic>
#include <memory>

#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "modules/audio_device/audio_device_generic.h"
#include "rtc_base/buffer.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "sdk/objc/base/RTCMacros.h"
#include "sdk/objc/native/src/audio/audio_session_observer.h"

RTC_FWD_DECL_OBJC_CLASS(RTC_OBJC_TYPE(RTCNativeAudioSessionDelegateAdapter));
RTC_FWD_DECL_OBJC_CLASS(AVAudioEngine);
RTC_FWD_DECL_OBJC_CLASS(AVAudioSourceNode);
RTC_FWD_DECL_OBJC_CLASS(AVAudioSinkNode);
RTC_FWD_DECL_OBJC_CLASS(AVAudioFormat);
RTC_FWD_DECL_OBJC_CLASS(AVAudioMixerNode);
RTC_FWD_DECL_OBJC_CLASS(AVAudioUnitEQ);

namespace webrtc {

class FineAudioBuffer;

class AudioEngineDevice : public AudioDeviceGeneric,
                          public AudioSessionObserver {
 public:
  explicit AudioEngineDevice(bool bypass_voice_processing);
  ~AudioEngineDevice() override;

  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) override;

  InitStatus Init() override;
  int32_t Terminate() override;
  bool Initialized() const override;

  int32_t InitPlayout() override;
  bool PlayoutIsInitialized() const override;

  int32_t InitRecording() override;
  bool RecordingIsInitialized() const override;

  int32_t StartPlayout() override;
  int32_t StopPlayout() override;
  bool Playing() const override;

  int32_t StartRecording() override;
  int32_t StopRecording() override;
  bool Recording() const override;

  int32_t PlayoutDelay(uint16_t& delayMS) const override;
  int32_t GetPlayoutUnderrunCount() const override { return -1; }

  // int GetPlayoutAudioParameters(AudioParameters* params) const override;
  // int GetRecordAudioParameters(AudioParameters* params) const override;

  int32_t ActiveAudioLayer(
      AudioDeviceModule::AudioLayer& audioLayer) const override;
  int32_t PlayoutIsAvailable(bool& available) override;
  int32_t RecordingIsAvailable(bool& available) override;
  int16_t PlayoutDevices() override;
  int16_t RecordingDevices() override;
  int32_t PlayoutDeviceName(uint16_t index, char name[kAdmMaxDeviceNameSize],
                            char guid[kAdmMaxGuidSize]) override;
  int32_t RecordingDeviceName(uint16_t index, char name[kAdmMaxDeviceNameSize],
                              char guid[kAdmMaxGuidSize]) override;
  int32_t SetPlayoutDevice(uint16_t index) override;
  int32_t SetPlayoutDevice(
      AudioDeviceModule::WindowsDeviceType device) override;
  int32_t SetRecordingDevice(uint16_t index) override;
  int32_t SetRecordingDevice(
      AudioDeviceModule::WindowsDeviceType device) override;
  int32_t InitSpeaker() override;
  bool SpeakerIsInitialized() const override;
  int32_t InitMicrophone() override;
  bool MicrophoneIsInitialized() const override;
  int32_t SpeakerVolumeIsAvailable(bool& available) override;
  int32_t SetSpeakerVolume(uint32_t volume) override;
  int32_t SpeakerVolume(uint32_t& volume) const override;
  int32_t MaxSpeakerVolume(uint32_t& maxVolume) const override;
  int32_t MinSpeakerVolume(uint32_t& minVolume) const override;
  int32_t MicrophoneVolumeIsAvailable(bool& available) override;
  int32_t SetMicrophoneVolume(uint32_t volume) override;
  int32_t MicrophoneVolume(uint32_t& volume) const override;
  int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const override;
  int32_t MinMicrophoneVolume(uint32_t& minVolume) const override;
  int32_t MicrophoneMuteIsAvailable(bool& available) override;
  int32_t SetMicrophoneMute(bool enable) override;
  int32_t MicrophoneMute(bool& enabled) const override;
  int32_t SpeakerMuteIsAvailable(bool& available) override;
  int32_t SetSpeakerMute(bool enable) override;
  int32_t SpeakerMute(bool& enabled) const override;
  int32_t StereoPlayoutIsAvailable(bool& available) override;
  int32_t SetStereoPlayout(bool enable) override;
  int32_t StereoPlayout(bool& enabled) const override;
  int32_t StereoRecordingIsAvailable(bool& available) override;
  int32_t SetStereoRecording(bool enable) override;
  int32_t StereoRecording(bool& enabled) const override;

  // AudioSessionObserver methods. May be called from any thread.
  void OnInterruptionBegin() override;
  void OnInterruptionEnd() override;
  void OnValidRouteChange() override;
  void OnCanPlayOrRecordChange(bool can_play_or_record) override;
  void OnChangedOutputVolume() override;

  bool IsInterrupted();

  int32_t SetObserver(AudioDeviceObserver* observer) override;

 private:
  struct EngineState {
    bool input_enabled = false;
    bool input_running = false;
    bool output_enabled = false;
    bool output_running = false;

    bool input_muted = false;
    bool is_interrupted = false;

    bool operator==(const EngineState& rhs) const;
    bool operator!=(const EngineState& rhs) const;

    bool IsAnyEnabled() const { return input_enabled || output_enabled; }
    bool IsAnyRunning() const { return input_running || output_running; }

    bool IsAllEnabled() const { return input_enabled && output_enabled; }
    bool IsAllRunning() const { return input_running && output_running; }
  };

  EngineState engine_state_ RTC_GUARDED_BY(thread_);

  bool IsMicrophonePermissionGranted();
  void SetEngineState(std::function<EngineState(EngineState)> state_transform);
  void UpdateEngineState(EngineState old_state, EngineState new_state);

  // Configures the audio session for WebRTC.
  bool ConfigureAudioSession();

  // Like above, but requires caller to already hold session lock.
  bool ConfigureAudioSessionLocked();

  // Unconfigures the audio session.
  void UnconfigureAudioSession();

  // AudioEngine observer methods. May be called from any thread.
  void OnEngineConfigurationChange();

  void DebugAudioEngine();

  // Determines whether voice processing should be enabled or disabled.
  const bool bypass_voice_processing_;

  // Native I/O audio thread checker.
  SequenceChecker io_thread_checker_;

  // Thread that this object is created on.
  rtc::Thread* thread_;

  AudioDeviceBuffer* audio_device_buffer_;

  AudioParameters playout_parameters_;
  AudioParameters record_parameters_;

  std::unique_ptr<FineAudioBuffer> fine_audio_buffer_;

  // Set to true after successful call to Init(), false otherwise.
  bool initialized_ RTC_GUARDED_BY(thread_);

  AudioDeviceObserver* observer_ RTC_GUARDED_BY(thread_);

  // Audio interruption observer instance.
  RTC_OBJC_TYPE(RTCNativeAudioSessionDelegateAdapter) * audio_session_observer_
      RTC_GUARDED_BY(thread_);

  // Set to true if we've activated the audio session.
  bool has_configured_session_ RTC_GUARDED_BY(thread_);

  // Avoids running pending task after `this` is Terminated.
  rtc::scoped_refptr<PendingTaskSafetyFlag> safety_ =
      PendingTaskSafetyFlag::Create();

  // Ratio between mach tick units and nanosecond. Used to change mach tick
  // units to nanoseconds.
  double machTickUnitsToNanoseconds_;

  // AVAudioEngine objects
  AVAudioEngine* audio_engine_;
  AVAudioFormat* rtc_internal_format_;     // Int16
  AVAudioFormat* engine_internal_format_;  // Float32

  // Output related
  AVAudioSourceNode* source_node_;

  // Input related nodes
  AVAudioSinkNode* sink_node_;
  AVAudioUnitEQ* input_eq_node_;
  AVAudioMixerNode* input_mixer_node_;

  void* configuration_observer_;
};
}  // namespace webrtc

#endif  // SDK_OBJC_NATIVE_SRC_AUDIO_AUDIO_DEVICE_AUDIOENGINE_H_

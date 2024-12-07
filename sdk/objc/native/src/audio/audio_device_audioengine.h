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
#include "audio_session_observer.h"
#include "modules/audio_device/audio_device_generic.h"
#include "rtc_base/buffer.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_annotations.h"
#include "sdk/objc/base/RTCMacros.h"

RTC_FWD_DECL_OBJC_CLASS(RTC_OBJC_TYPE(RTCNativeAudioSessionDelegateAdapter));
RTC_FWD_DECL_OBJC_CLASS(AVAudioEngine);
RTC_FWD_DECL_OBJC_CLASS(AVAudioSourceNode);
RTC_FWD_DECL_OBJC_CLASS(AVAudioSinkNode);
RTC_FWD_DECL_OBJC_CLASS(AVAudioFormat);
RTC_FWD_DECL_OBJC_CLASS(AVAudioMixerNode);

namespace webrtc {

class FineAudioBuffer;

namespace ios_adm {

class AudioDeviceAudioEngine : public AudioDeviceGeneric,
                               public AudioSessionObserver {
 public:
  explicit AudioDeviceAudioEngine(bool bypass_voice_processing);
  ~AudioDeviceAudioEngine() override;

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

 private:
  // Called by the relevant AudioSessionObserver methods on `thread_`.
  void HandleInterruptionBegin();
  void HandleInterruptionEnd();
  void HandleValidRouteChange();
  void HandleCanPlayOrRecordChange(bool can_play_or_record);
  void HandleSampleRateChange();
  void HandlePlayoutGlitchDetected();
  void HandleOutputVolumeChange();

  bool RestartAudioUnit(bool enable_input);

  // Uses current `playout_parameters_` and `record_parameters_` to inform the
  // audio device buffer (ADB) about our internal audio parameters.
  void UpdateAudioDeviceBuffer();

  // Since the preferred audio parameters are only hints to the OS, the actual
  // values may be different once the AVAudioSession has been activated.
  // This method asks for the current hardware parameters and takes actions
  // if they should differ from what we have asked for initially. It also
  // defines `playout_parameters_` and `record_parameters_`.
  void SetupAudioBuffersForActiveAudioSession();

  // Configures the audio session for WebRTC.
  bool ConfigureAudioSession();

  // Like above, but requires caller to already hold session lock.
  bool ConfigureAudioSessionLocked();

  // Unconfigures the audio session.
  void UnconfigureAudioSession();

  // Activates our audio session, creates and initializes the voice-processing
  // audio unit and verifies that we got the preferred native audio parameters.
  bool InitPlayOrRecord(bool enable_input);

  // Closes and deletes the voice-processing I/O unit.
  void ShutdownPlayOrRecord();

  // Resets thread-checkers before a call is restarted.
  void PrepareForNewStart();

  bool EngineCreate();
  bool EngineCleanUp();

  bool AttachEngineInput();
  bool DetachEngineInput();

  // Determines whether voice processing should be enabled or disabled.
  const bool bypass_voice_processing_;

  // Native I/O audio thread checker.
  SequenceChecker io_thread_checker_;

  // Thread that this object is created on.
  rtc::Thread* thread_;

  // Raw pointer handle provided to us in AttachAudioBuffer(). Owned by the
  // AudioDeviceModuleImpl class and called by AudioDeviceModule::Create().
  // The AudioDeviceBuffer is a member of the AudioDeviceModuleImpl instance
  // and therefore outlives this object.
  AudioDeviceBuffer* audio_device_buffer_;

  // Contains audio parameters (sample rate, #channels, buffer size etc.) for
  // the playout and recording sides. These structure is set in two steps:
  // first, native sample rate and #channels are defined in Init(). Next, the
  // audio session is activated and we verify that the preferred parameters
  // were granted by the OS. At this stage it is also possible to add a third
  // component to the parameters; the native I/O buffer duration.
  // A RTC_CHECK will be hit if we for some reason fail to open an audio session
  // using the specified parameters.
  AudioParameters playout_parameters_;
  AudioParameters record_parameters_;

  std::unique_ptr<FineAudioBuffer> fine_audio_buffer_;

  bool recording_is_initialized_ RTC_GUARDED_BY(thread_);

  // Set to 1 when recording is active and 0 otherwise.
  std::atomic<int> recording_;

  bool playout_is_initialized_ RTC_GUARDED_BY(thread_);

  // Set to 1 when playout is active and 0 otherwise.
  std::atomic<int> playing_;

  // Set to true after successful call to Init(), false otherwise.
  bool initialized_ RTC_GUARDED_BY(thread_);

  // Set to true if audio session is interrupted, false otherwise.
  bool is_interrupted_;

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
  AVAudioFormat* rtc_playout_format_;  // int16
  AVAudioFormat* rtc_record_format_;   // int16

  AVAudioFormat* input_node_format_;
  AVAudioFormat* output_node_format_;

  AVAudioEngine* audio_engine_;
  AVAudioSinkNode* sink_node_;
  AVAudioSourceNode* source_node_;
  AVAudioMixerNode* input_mixer_node_;
};
}  // namespace ios_adm
}  // namespace webrtc

#endif  // SDK_OBJC_NATIVE_SRC_AUDIO_AUDIO_DEVICE_AUDIOENGINE_H_

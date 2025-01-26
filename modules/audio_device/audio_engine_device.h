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

#if defined(__OBJC__)
#import <AVFAudio/AVFAudio.h>
#else
// Forward declarations for C++ code
#ifdef __OBJC__
@class AVAudioEngine;
@class AVAudioInputNode;
@class AVAudioOutputNode;
@class AVAudioSourceNode;
@class AVAudioSinkNode;
@class AVAudioMixerNode;
@class AVAudioPCMBuffer;
@class AVAudioFormat;
typedef void (^AVAudioEngineManualRenderingBlock)(AVAudioFrameCount,
                                                  AudioBufferList*, OSStatus*);
#else
typedef void AVAudioEngine;
typedef void AVAudioInputNode;
typedef void AVAudioOutputNode;
typedef void AVAudioSourceNode;
typedef void AVAudioSinkNode;
typedef void AVAudioMixerNode;
typedef void AVAudioPCMBuffer;
typedef void AVAudioFormat;
typedef void* AVAudioEngineManualRenderingBlock;
#endif
#endif

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

#if TARGET_OS_OSX
#import <CoreAudio/CoreAudio.h>
#endif

RTC_FWD_DECL_OBJC_CLASS(RTC_OBJC_TYPE(RTCNativeAudioSessionDelegateAdapter));

namespace webrtc {

class FineAudioBuffer;

class AudioEngineDevice : public AudioDeviceModule,
                          public AudioSessionObserver {
 public:
  explicit AudioEngineDevice(bool voice_processing_bypassed);
  ~AudioEngineDevice() override;

  int32_t Init() override;
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

  int32_t PlayoutDelay(uint16_t* delayMS) const override;
  int32_t GetPlayoutUnderrunCount() const override { return -1; }

#if defined(WEBRTC_IOS)
  int GetPlayoutAudioParameters(AudioParameters* params) const override;
  int GetRecordAudioParameters(AudioParameters* params) const override;
#endif

  int32_t ActiveAudioLayer(
      AudioDeviceModule::AudioLayer* audioLayer) const override;
  int32_t PlayoutIsAvailable(bool* available) override;
  int32_t RecordingIsAvailable(bool* available) override;
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
  int32_t SpeakerVolumeIsAvailable(bool* available) override;
  int32_t SetSpeakerVolume(uint32_t volume) override;
  int32_t SpeakerVolume(uint32_t* volume) const override;
  int32_t MaxSpeakerVolume(uint32_t* maxVolume) const override;
  int32_t MinSpeakerVolume(uint32_t* minVolume) const override;
  int32_t MicrophoneVolumeIsAvailable(bool* available) override;
  int32_t SetMicrophoneVolume(uint32_t volume) override;
  int32_t MicrophoneVolume(uint32_t* volume) const override;
  int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const override;
  int32_t MinMicrophoneVolume(uint32_t* minVolume) const override;
  int32_t MicrophoneMuteIsAvailable(bool* available) override;
  int32_t SetMicrophoneMute(bool enable) override;
  int32_t MicrophoneMute(bool* enabled) const override;
  int32_t SpeakerMuteIsAvailable(bool* available) override;
  int32_t SetSpeakerMute(bool enable) override;
  int32_t SpeakerMute(bool* enabled) const override;
  int32_t StereoPlayoutIsAvailable(bool* available) const override;
  int32_t SetStereoPlayout(bool enable) override;
  int32_t StereoPlayout(bool* enabled) const override;
  int32_t StereoRecordingIsAvailable(bool* available) const override;
  int32_t SetStereoRecording(bool enable) override;
  int32_t StereoRecording(bool* enabled) const override;

  int32_t RegisterAudioCallback(AudioTransport* audioCallback) override;

  // Only supported on Android.
  bool BuiltInAECIsAvailable() const override;
  bool BuiltInAGCIsAvailable() const override;
  bool BuiltInNSIsAvailable() const override;

  // Enables the built-in audio effects. Only supported on Android.
  int32_t EnableBuiltInAEC(bool enable) override;
  int32_t EnableBuiltInAGC(bool enable) override;
  int32_t EnableBuiltInNS(bool enable) override;

  // AudioSessionObserver methods. May be called from any thread.
  void OnInterruptionBegin() override;
  void OnInterruptionEnd(bool should_resume) override;
  void OnValidRouteChange() override;
  void OnCanPlayOrRecordChange(bool can_play_or_record) override;
  void OnChangedOutputVolume() override;

  bool IsInterrupted();

  int32_t SetObserver(AudioDeviceObserver* observer) override;

  int32_t SetManualRenderingMode(bool enable);
  int32_t ManualRenderingMode(bool* enabled);

  int32_t SetAdvancedDucking(bool enable);
  int32_t AdvancedDucking(bool* enabled);

  int32_t SetDuckingLevel(long level);
  int32_t DuckingLevel(long* level);

  int32_t SetInitRecordingPersistentMode(bool enable);
  int32_t InitRecordingPersistentMode(bool* enabled);

  int32_t SetVoiceProcessingBypassed(bool enable);
  int32_t VoiceProcessingBypassed(bool* enabled);

  int32_t SetVoiceProcessingAGCEnabled(bool enable);
  int32_t VoiceProcessingAGCEnabled(bool* enabled);

  int32_t InitAndStartRecording();

  enum RenderMode { Device, Manual };

 private:
  // Represents the state of the audio engine, including input/output status,
  // rendering mode, and various configuration flags.
  struct EngineState {
    bool input_enabled = false;
    bool input_running = false;
    bool output_enabled = false;
    bool output_running = false;

    // Output will be enabled when input is enabled
    bool input_follow_mode = true;
    bool input_enabled_persistent_mode = false;

    bool input_muted = true;
    bool is_interrupted = false;

    RenderMode render_mode = RenderMode::Device;
    bool voice_processing_enabled = true;
    bool voice_processing_bypassed = false;
    bool voice_processing_agc_enabled = true;
    bool advanced_ducking = true;
    long ducking_level = 0;  // 0 = Default

    uint32_t output_device_id = 0;  // kAudioObjectUnknown
    uint32_t input_device_id = 0;   // kAudioObjectUnknown

    bool operator==(const EngineState& rhs) const {
      return input_enabled == rhs.input_enabled &&
             input_running == rhs.input_running &&
             output_enabled == rhs.output_enabled &&
             output_running == rhs.output_running &&
             input_follow_mode == rhs.input_follow_mode &&
             input_enabled_persistent_mode ==
                 rhs.input_enabled_persistent_mode &&
             input_muted == rhs.input_muted &&
             is_interrupted == rhs.is_interrupted &&
             render_mode == rhs.render_mode &&
             voice_processing_enabled == rhs.voice_processing_enabled &&
             voice_processing_bypassed == rhs.voice_processing_bypassed &&
             voice_processing_agc_enabled == rhs.voice_processing_agc_enabled &&
             advanced_ducking == rhs.advanced_ducking &&
             ducking_level == rhs.ducking_level &&
             output_device_id == rhs.output_device_id &&
             input_device_id == rhs.input_device_id;
    }

    bool operator!=(const EngineState& rhs) const { return !(*this == rhs); }

    bool IsOutputInputLinked() const {
      return input_follow_mode && voice_processing_enabled;
    }

    bool IsOutputEnabled() const {
      return IsOutputInputLinked() ? IsInputEnabled() || output_enabled
                                   : output_enabled;
    }

    bool IsOutputRunning() const {
      return IsOutputInputLinked() ? input_running || output_running
                                   : output_running;
    }

    bool IsInputEnabled() const {
      return input_enabled || input_enabled_persistent_mode;
    }
    bool IsInputRunning() const { return input_running; }

    bool IsAnyEnabled() const { return IsInputEnabled() || output_enabled; }
    bool IsAnyRunning() const { return input_running || output_running; }

    bool IsAllEnabled() const {
      return IsOutputInputLinked() ? IsInputEnabled()
                                   : IsInputEnabled() && output_enabled;
    }

    bool IsAllRunning() const {
      return IsOutputInputLinked() ? input_running
                                   : input_running && output_running;
    }
  };

  struct EngineStateUpdate {
    EngineState prev;
    EngineState next;

    bool HasNoChanges() const { return prev == next; }

    bool DidEnableOutput() const {
      return !prev.IsOutputEnabled() && next.IsOutputEnabled();
    }

    bool DidEnableInput() const {
      return !prev.IsInputEnabled() && next.IsInputEnabled();
    }

    bool DidEnableOutputOrInput() const {
      return DidEnableOutput() || DidEnableInput();
    }

    bool DidDisableOutput() const {
      return prev.IsOutputEnabled() && next.IsOutputEnabled();
    }

    bool DidDisableInput() const {
      return prev.IsInputEnabled() && next.IsInputEnabled();
    }

    bool DidAnyEnable() const { return DidEnableOutput() || DidEnableInput(); }

    bool DidAnyDisable() const {
      return DidDisableOutput() || DidDisableInput();
    }

    bool DidBeginInterruption() const {
      return !prev.is_interrupted && next.is_interrupted;
    }

    bool DidEndInterruption() const {
      return prev.is_interrupted && !next.is_interrupted;
    }

    bool DidUpdateAudioGraph() const {
      return (prev.IsInputEnabled() != next.IsInputEnabled()) ||
             (prev.IsOutputEnabled() != next.IsOutputEnabled());
    }

    bool DidUpdateOutputDevice() const {
      return prev.output_device_id != next.output_device_id;
    }

    bool DidUpdateInputDevice() const {
      return prev.input_device_id != next.input_device_id;
    }

    bool IsEngineRestartRequired() const {
      return DidUpdateAudioGraph() || DidUpdateOutputDevice() ||
             DidUpdateInputDevice();
    }

    // Special case to re-create engine when switching from Speaker & Mic ->
    // Speaker only.
    bool IsEngineRecreateRequired() const {
      return (prev.IsOutputEnabled() && next.IsOutputEnabled()) &&
             (prev.IsInputEnabled() && !next.IsInputEnabled());
    }

    bool DidEnableManualRenderingMode() const {
      return prev.render_mode != RenderMode::Manual &&
             next.render_mode == RenderMode::Manual;
    }

    bool DidEnableDeviceRenderingMode() const {
      return prev.render_mode != RenderMode::Device &&
             next.render_mode == RenderMode::Device;
    }
  };

  EngineState engine_state_ RTC_GUARDED_BY(thread_);

  AVAudioInputNode* InputNode();
  AVAudioOutputNode* OutputNode();

  bool IsMicrophonePermissionGranted();
  void SetEngineState(std::function<EngineState(EngineState)> state_transform);
  void UpdateDeviceEngineState(EngineStateUpdate state);
  void UpdateManualEngineState(EngineStateUpdate state);

  // AudioEngine observer methods. May be called from any thread.
  void ReconfigureEngine(bool is_required);

// Device related
#if TARGET_OS_OSX
  void UpdateDeviceInformation();
  std::vector<AudioObjectID> input_device_ids_;
  std::vector<AudioObjectID> output_device_ids_;
  std::vector<std::string> output_device_labels_;
  std::vector<std::string> input_device_labels_;
#endif

  void DebugAudioEngine();

  void StartRenderLoop();
  AVAudioEngineManualRenderingBlock render_block_;

  // Thread that this object is created on.
  rtc::Thread* thread_;
  std::unique_ptr<rtc::Thread> render_thread_;
  AVAudioPCMBuffer* render_buffer_;

  const std::unique_ptr<TaskQueueFactory> task_queue_factory_;
  std::unique_ptr<AudioDeviceBuffer> audio_device_buffer_;
  std::unique_ptr<FineAudioBuffer> fine_audio_buffer_;

  AudioParameters playout_parameters_;
  AudioParameters record_parameters_;

  // Set to true after successful call to Init(), false otherwise.
  bool initialized_ RTC_GUARDED_BY(thread_);

  AudioDeviceObserver* observer_ RTC_GUARDED_BY(thread_);

#if defined(WEBRTC_IOS)
  // Audio interruption observer instance.
  RTC_OBJC_TYPE(RTCNativeAudioSessionDelegateAdapter) * audio_session_observer_
      RTC_GUARDED_BY(thread_);
#endif

  // Avoids running pending task after `this` is Terminated.
  rtc::scoped_refptr<PendingTaskSafetyFlag> safety_ =
      PendingTaskSafetyFlag::Create();

  // Ratio between mach tick units and nanosecond. Used to change mach tick
  // units to nanoseconds.
  double machTickUnitsToNanoseconds_;

  // AVAudioEngine objects
  AVAudioEngine* engine_device_ RTC_GUARDED_BY(thread_);
  AVAudioEngine* engine_manual_input_ RTC_GUARDED_BY(thread_);

  // Used for manual rendering mode
  AVAudioFormat* manual_render_rtc_format_;  // Int16

  // Output related
  AVAudioSourceNode* source_node_ RTC_GUARDED_BY(thread_);

  // Input related nodes
  AVAudioSinkNode* sink_node_ RTC_GUARDED_BY(thread_);
  AVAudioMixerNode* input_mixer_node_ RTC_GUARDED_BY(thread_);

  void* configuration_observer_ RTC_GUARDED_BY(thread_);
};
}  // namespace webrtc

#endif  // SDK_OBJC_NATIVE_SRC_AUDIO_AUDIO_DEVICE_AUDIOENGINE_H_

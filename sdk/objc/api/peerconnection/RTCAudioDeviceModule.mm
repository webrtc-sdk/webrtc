/*
 * Copyright 2022 LiveKit
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

#include <os/lock.h>

#import "RTCAudioDeviceModule+Private.h"
#import "RTCAudioDeviceModule.h"
#import "RTCIODevice+Private.h"
#import "base/RTCLogging.h"

#import "modules/audio_device/audio_engine_device.h"
#import "sdk/objc/native/api/audio_device_module.h"

NSString *const kRTCAudioEngineInputMixerNodeKey = webrtc::kAudioEngineInputMixerNodeKey;

inline webrtc::AudioEngineDevice::MuteMode MuteModeToRTC(RTCAudioEngineMuteMode mode) {
  return static_cast<webrtc::AudioEngineDevice::MuteMode>(mode);
}

inline RTCAudioEngineMuteMode MuteModeToObjC(webrtc::AudioEngineDevice::MuteMode mode) {
  return static_cast<RTCAudioEngineMuteMode>(mode);
}

@implementation RTC_OBJC_TYPE (RTCAudioEngineState) {
  webrtc::AudioEngineDevice::EngineState _state;
}

- (instancetype)initWithRTCType:(webrtc::AudioEngineDevice::EngineState)state {
  self = [super init];
  if (self) {
    _state = state;
  }
  return self;
}

- (BOOL)isOutputEnabled {
  return _state.IsOutputEnabled();
}

- (BOOL)isOutputRunning {
  return _state.IsOutputRunning();
}

- (BOOL)isInputEnabled {
  return _state.IsInputEnabled();
}

- (BOOL)isInputRunning {
  return _state.IsInputRunning();
}

- (BOOL)isInputMuted {
  return _state.input_muted;
}

- (RTCAudioEngineMuteMode)muteMode {
  return MuteModeToObjC(_state.mute_mode);
}

@end

@implementation RTC_OBJC_TYPE (RTCAudioEngineStateTransition) {
  webrtc::AudioEngineDevice::EngineStateTransition _state_transition;
}

- (instancetype)initWithStateTransition:
    (webrtc::AudioEngineDevice::EngineStateTransition)state_transition {
  self = [super init];
  if (self) {
    _state_transition = state_transition;
  }
  return self;
}

- (RTC_OBJC_TYPE(RTCAudioEngineState) *)prev {
  return [[RTC_OBJC_TYPE(RTCAudioEngineState) alloc] initWithRTCType:_state_transition.prev];
}

- (RTC_OBJC_TYPE(RTCAudioEngineState) *)next {
  return [[RTC_OBJC_TYPE(RTCAudioEngineState) alloc] initWithRTCType:_state_transition.next];
}

@end

class AudioDeviceObserver : public webrtc::AudioDeviceObserver,
                            public webrtc::AudioEngineDevice::EngineObserver {
 public:
  AudioDeviceObserver(RTC_OBJC_TYPE(RTCAudioDeviceModule) * adm) { adm_ = adm; }

  void OnDevicesUpdated() override { [delegate_ audioDeviceModuleDidUpdateDevices:adm_]; }

  void OnEngineDidReceiveMutedSpeechActivityEvent(
      webrtc::AudioEngineDevice::SpeechActivityEvent event) override {
    [delegate_ audioDeviceModule:adm_
        didReceiveMutedSpeechActivityEvent:ConvertSpeechActivityEvent(event)];
  }

  int32_t OnEngineDidCreate(
      AVAudioEngine *engine,
      webrtc::AudioEngineDevice::EngineStateTransition state_transition) override {
    if (delegate_ == nil) return 0;
    return [delegate_ audioDeviceModule:adm_
                        didCreateEngine:engine
                        stateTransition:[[RTC_OBJC_TYPE(RTCAudioEngineStateTransition) alloc]
                                            initWithStateTransition:state_transition]];
  }

  int32_t OnEngineWillEnable(
      AVAudioEngine *engine,
      webrtc::AudioEngineDevice::EngineStateTransition state_transition) override {
    if (delegate_ == nil) return 0;
    return [delegate_ audioDeviceModule:adm_
                       willEnableEngine:engine
                        stateTransition:[[RTC_OBJC_TYPE(RTCAudioEngineStateTransition) alloc]
                                            initWithStateTransition:state_transition]];
  }

  int32_t OnEngineWillStart(
      AVAudioEngine *engine,
      webrtc::AudioEngineDevice::EngineStateTransition state_transition) override {
    if (delegate_ == nil) return 0;
    return [delegate_ audioDeviceModule:adm_
                        willStartEngine:engine
                        stateTransition:[[RTC_OBJC_TYPE(RTCAudioEngineStateTransition) alloc]
                                            initWithStateTransition:state_transition]];
  }

  int32_t OnEngineDidStop(
      AVAudioEngine *engine,
      webrtc::AudioEngineDevice::EngineStateTransition state_transition) override {
    if (delegate_ == nil) return 0;
    return [delegate_ audioDeviceModule:adm_
                          didStopEngine:engine
                        stateTransition:[[RTC_OBJC_TYPE(RTCAudioEngineStateTransition) alloc]
                                            initWithStateTransition:state_transition]];
  }

  int32_t OnEngineDidDisable(
      AVAudioEngine *engine,
      webrtc::AudioEngineDevice::EngineStateTransition state_transition) override {
    if (delegate_ == nil) return 0;
    return [delegate_ audioDeviceModule:adm_
                       didDisableEngine:engine
                        stateTransition:[[RTC_OBJC_TYPE(RTCAudioEngineStateTransition) alloc]
                                            initWithStateTransition:state_transition]];
  }

  int32_t OnEngineWillRelease(
      AVAudioEngine *engine,
      webrtc::AudioEngineDevice::EngineStateTransition state_transition) override {
    if (delegate_ == nil) return 0;
    return [delegate_ audioDeviceModule:adm_
                      willReleaseEngine:engine
                        stateTransition:[[RTC_OBJC_TYPE(RTCAudioEngineStateTransition) alloc]
                                            initWithStateTransition:state_transition]];
  }

  int32_t OnEngineWillConnectInput(
      AVAudioEngine *engine, AVAudioNode *src, AVAudioNode *dst, AVAudioFormat *format,
      webrtc::AudioEngineDevice::EngineStateTransition state_transition,
      NSDictionary *context) override {
    if (delegate_ == nil) return 0;
    return [delegate_ audioDeviceModule:adm_
                                 engine:engine
               configureInputFromSource:src
                          toDestination:dst
                             withFormat:format
                        stateTransition:[[RTC_OBJC_TYPE(RTCAudioEngineStateTransition) alloc]
                                            initWithStateTransition:state_transition]
                                context:context];
  }

  int32_t OnEngineWillConnectOutput(
      AVAudioEngine *engine, AVAudioNode *src, AVAudioNode *dst, AVAudioFormat *format,
      webrtc::AudioEngineDevice::EngineStateTransition state_transition,
      NSDictionary *context) override {
    if (delegate_ == nil) return 0;
    return [delegate_ audioDeviceModule:adm_
                                 engine:engine
              configureOutputFromSource:src
                          toDestination:dst
                             withFormat:format
                        stateTransition:[[RTC_OBJC_TYPE(RTCAudioEngineStateTransition) alloc]
                                            initWithStateTransition:state_transition]
                                context:context];
  }

  __weak id<RTC_OBJC_TYPE(RTCAudioDeviceModuleDelegate)> delegate_;

 private:
  __weak RTC_OBJC_TYPE(RTCAudioDeviceModule) * adm_;

  RTCSpeechActivityEvent ConvertSpeechActivityEvent(
      webrtc::AudioEngineDevice::SpeechActivityEvent event) {
    switch (event) {
      case webrtc::AudioEngineDevice::SpeechActivityEvent::kStarted:
        return RTCSpeechActivityEvent::RTCSpeechActivityEventStarted;
      case webrtc::AudioEngineDevice::SpeechActivityEvent::kEnded:
        return RTCSpeechActivityEvent::RTCSpeechActivityEventEnded;
      default:
        return RTCSpeechActivityEvent::RTCSpeechActivityEventEnded;
    }
  }
};

@implementation RTC_OBJC_TYPE (RTCAudioDeviceModule) {
  rtc::Thread *_workerThread;
  rtc::scoped_refptr<webrtc::AudioDeviceModule> _native;
  AudioDeviceObserver *_observer;
}

- (id<RTC_OBJC_TYPE(RTCAudioDeviceModuleDelegate)>)observer {
  return _workerThread->BlockingCall([self] { return _observer->delegate_; });
}

- (void)setObserver:(id<RTC_OBJC_TYPE(RTCAudioDeviceModuleDelegate)>)observer {
  webrtc::AudioEngineDevice *engine_device =
      dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());

  _workerThread->BlockingCall([self, observer, engine_device] {
    _observer->delegate_ = observer;
    _native->SetObserver(observer != nil ? _observer : nullptr);
    engine_device->SetEngineObserver(observer != nil ? _observer : nullptr);
  });
}

- (instancetype)initWithNativeModule:(rtc::scoped_refptr<webrtc::AudioDeviceModule>)module
                        workerThread:(rtc::Thread *)workerThread {
  RTCLogInfo(@"RTCAudioDeviceModule initWithNativeModule:workerThread:");

  self = [super init];
  _native = module;
  _workerThread = workerThread;

  _observer = new AudioDeviceObserver(self);

  return self;
}

- (NSArray<RTC_OBJC_TYPE(RTCIODevice) *> *)outputDevices {
  return _workerThread->BlockingCall([self] { return [self _outputDevices]; });
}

- (NSArray<RTC_OBJC_TYPE(RTCIODevice) *> *)inputDevices {
  return _workerThread->BlockingCall([self] { return [self _inputDevices]; });
}

- (RTC_OBJC_TYPE(RTCIODevice) *)outputDevice {
  return _workerThread->BlockingCall([self] {
    NSArray<RTC_OBJC_TYPE(RTCIODevice) *> *devices = [self _outputDevices];
    int16_t devicesCount = (int16_t)([devices count]);
    int16_t index = _native->GetPlayoutDevice();

    if (devicesCount == 0 || index <= -1 || index > (devicesCount - 1)) {
      return (RTC_OBJC_TYPE(RTCIODevice) *)nil;
    }

    return (RTC_OBJC_TYPE(RTCIODevice) *)[devices objectAtIndex:index];
  });
}

- (void)setOutputDevice:(RTC_OBJC_TYPE(RTCIODevice) *)device {
  [self trySetOutputDevice:device];
}

- (BOOL)trySetOutputDevice:(RTC_OBJC_TYPE(RTCIODevice) *)device {
  return _workerThread->BlockingCall([self, device] {
    NSUInteger index = 0;
    NSArray *devices = [self _outputDevices];

    if ([devices count] == 0) {
      return NO;
    }

    if (device != nil) {
      index = [devices indexOfObjectPassingTest:^BOOL(RTC_OBJC_TYPE(RTCIODevice) * e, NSUInteger i,
                                                      BOOL * stop) {
        return (*stop = [e.deviceId isEqualToString:device.deviceId]);
      }];
      if (index == NSNotFound) {
        return NO;
      }
    }

    if (_native->SetPlayoutDevice(index)) {
      return YES;
    }

    return NO;
  });
}

- (RTC_OBJC_TYPE(RTCIODevice) *)inputDevice {
  return _workerThread->BlockingCall([self] {
    NSArray<RTC_OBJC_TYPE(RTCIODevice) *> *devices = [self _inputDevices];
    int16_t devicesCount = (int16_t)([devices count]);
    int16_t index = _native->GetRecordingDevice();

    if (devicesCount == 0 || index <= -1 || index > (devicesCount - 1)) {
      return (RTC_OBJC_TYPE(RTCIODevice) *)nil;
    }

    return (RTC_OBJC_TYPE(RTCIODevice) *)[devices objectAtIndex:index];
  });
}

- (void)setInputDevice:(RTC_OBJC_TYPE(RTCIODevice) *)device {
  [self trySetInputDevice:device];
}

- (BOOL)trySetInputDevice:(RTC_OBJC_TYPE(RTCIODevice) *)device {
  return _workerThread->BlockingCall([self, device] {
    NSUInteger index = 0;
    NSArray *devices = [self _inputDevices];

    if ([devices count] == 0) {
      return NO;
    }

    if (device != nil) {
      index = [devices indexOfObjectPassingTest:^BOOL(RTC_OBJC_TYPE(RTCIODevice) * e, NSUInteger i,
                                                      BOOL * stop) {
        return (*stop = [e.deviceId isEqualToString:device.deviceId]);
      }];
      if (index == NSNotFound) {
        return NO;
      }
    }

    if (_native->SetRecordingDevice(index)) {
      return YES;
    }

    return NO;
  });
}

- (BOOL)playing {
  return _workerThread->BlockingCall([self] { return _native->Playing(); });
}

- (BOOL)recording {
  return _workerThread->BlockingCall([self] { return _native->Recording(); });
}

#pragma mark - Low-level access

- (NSInteger)startPlayout {
  return _workerThread->BlockingCall([self] { return _native->StartPlayout(); });
}

- (NSInteger)stopPlayout {
  return _workerThread->BlockingCall([self] { return _native->StopPlayout(); });
}

- (NSInteger)initPlayout {
  return _workerThread->BlockingCall([self] { return _native->InitPlayout(); });
}

- (NSInteger)startRecording {
  return _workerThread->BlockingCall([self] { return _native->StartRecording(); });
}

- (NSInteger)stopRecording {
  return _workerThread->BlockingCall([self] { return _native->StopRecording(); });
}

- (NSInteger)initRecording {
  return _workerThread->BlockingCall([self] { return _native->InitRecording(); });
}

- (NSInteger)initAndStartRecording {
  webrtc::AudioEngineDevice *module = dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());
  if (module == nullptr) return -1;

  return _workerThread->BlockingCall([module] { return module->InitAndStartRecording(); });
}

- (BOOL)isPlayoutInitialized {
  return _workerThread->BlockingCall([self] { return _native->PlayoutIsInitialized(); });
}

- (BOOL)isRecordingInitialized {
  return _workerThread->BlockingCall([self] { return _native->RecordingIsInitialized(); });
}

- (BOOL)isPlaying {
  return _workerThread->BlockingCall([self] { return _native->Playing(); });
}

- (BOOL)isRecording {
  return _workerThread->BlockingCall([self] { return _native->Recording(); });
}

- (BOOL)isEngineRunning {
  webrtc::AudioEngineDevice *module = dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());
  if (module == nullptr) return false;

  return _workerThread->BlockingCall([module] { return module->IsEngineRunning(); });
}

- (BOOL)isMicrophoneMuted {
  return _workerThread->BlockingCall([self] {
    bool value = false;
    return _native->MicrophoneMute(&value) == 0 ? value : NO;
  });
}

- (NSInteger)setMicrophoneMuted:(BOOL)muted {
  return _workerThread->BlockingCall([self, muted] { return _native->SetMicrophoneMute(muted); });
}

- (RTC_OBJC_TYPE(RTCAudioEngineState) *)engineState {
  webrtc::AudioEngineDevice *module = dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());
  if (module == nullptr) return [[RTC_OBJC_TYPE(RTCAudioEngineState) alloc] init];

  return _workerThread->BlockingCall([module] {
    webrtc::AudioEngineDevice::EngineState state;
    if (module->GetEngineState(&state) != 0)
      return [[RTC_OBJC_TYPE(RTCAudioEngineState) alloc] init];

    return [[RTC_OBJC_TYPE(RTCAudioEngineState) alloc] initWithRTCType:state];
  });
}

#pragma mark - Unique to AudioEngineDevice

- (BOOL)isRecordingAlwaysPreparedMode {
  webrtc::AudioEngineDevice *module = dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());
  if (module == nullptr) return NO;

  return _workerThread->BlockingCall([module] {
    bool value = false;
    return module->InitRecordingPersistentMode(&value) == 0 ? value : NO;
  });
}

- (NSInteger)setRecordingAlwaysPreparedMode:(BOOL)enabled {
  webrtc::AudioEngineDevice *module = dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());
  if (module == nullptr) return -1;

  return _workerThread->BlockingCall(
      [module, enabled] { return module->SetInitRecordingPersistentMode(enabled); });
}

- (BOOL)isManualRenderingMode {
  webrtc::AudioEngineDevice *module = dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());
  if (module == nullptr) return NO;

  return _workerThread->BlockingCall([module] {
    bool value = false;
    return module->ManualRenderingMode(&value) == 0 ? value : NO;
  });
}

- (NSInteger)setManualRenderingMode:(BOOL)enabled {
  webrtc::AudioEngineDevice *module = dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());
  if (module == nullptr) return -1;

  return _workerThread->BlockingCall(
      [module, enabled] { return module->SetManualRenderingMode(enabled); });
}

- (BOOL)isAdvancedDuckingEnabled {
  webrtc::AudioEngineDevice *module = dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());
  if (module == nullptr) return NO;

  return _workerThread->BlockingCall([module] {
    bool value = false;
    return module->AdvancedDucking(&value) == 0 ? value : NO;
  });
}

- (void)setAdvancedDuckingEnabled:(BOOL)enabled {
  webrtc::AudioEngineDevice *module = dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());
  if (module == nullptr) return;

  _workerThread->BlockingCall(
      [module, enabled] { return module->SetAdvancedDucking(enabled) == 0; });
}

- (NSInteger)duckingLevel {
  webrtc::AudioEngineDevice *module = dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());
  if (module == nullptr) return 0;

  return _workerThread->BlockingCall([module] {
    long value = false;
    return module->DuckingLevel(&value) == 0 ? value : 0;
  });
}

- (void)setDuckingLevel:(NSInteger)value {
  webrtc::AudioEngineDevice *module = dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());
  if (module == nullptr) return;

  _workerThread->BlockingCall([module, value] { return module->SetDuckingLevel(value) == 0; });
}

- (RTCAudioEngineMuteMode)muteMode {
  webrtc::AudioEngineDevice *module = dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());
  if (module == nullptr) return RTCAudioEngineMuteModeUnknown;

  return _workerThread->BlockingCall([module] {
    webrtc::AudioEngineDevice::MuteMode mode;
    return module->GetMuteMode(&mode) == 0 ? MuteModeToObjC(mode) : RTCAudioEngineMuteModeUnknown;
  });
}

- (NSInteger)setMuteMode:(RTCAudioEngineMuteMode)mode {
  webrtc::AudioEngineDevice *module = dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());
  if (module == nullptr) return -1;

  return _workerThread->BlockingCall(
      [module, mode] { return module->SetMuteMode(MuteModeToRTC(mode)); });
}

- (BOOL)isVoiceProcessingEnabled {
  webrtc::AudioEngineDevice *module = dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());
  if (module == nullptr) return NO;

  return _workerThread->BlockingCall([module] {
    bool value = false;
    return module->VoiceProcessingEnabled(&value) == 0 ? value : NO;
  });
}

- (NSInteger)setVoiceProcessingEnabled:(BOOL)enabled {
  webrtc::AudioEngineDevice *module = dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());
  if (module == nullptr) return -1;

  return _workerThread->BlockingCall(
      [module, enabled] { return module->SetVoiceProcessingEnabled(enabled); });
}

- (BOOL)isVoiceProcessingBypassed {
  webrtc::AudioEngineDevice *module = dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());
  if (module == nullptr) return NO;

  return _workerThread->BlockingCall([module] {
    bool value = false;
    return module->VoiceProcessingBypassed(&value) == 0 ? value : NO;
  });
}

- (void)setVoiceProcessingBypassed:(BOOL)enabled {
  webrtc::AudioEngineDevice *module = dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());
  if (module == nullptr) return;

  _workerThread->BlockingCall(
      [module, enabled] { return module->SetVoiceProcessingBypassed(enabled) == 0; });
}

- (BOOL)isVoiceProcessingAGCEnabled {
  webrtc::AudioEngineDevice *module = dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());
  if (module == nullptr) return NO;

  return _workerThread->BlockingCall([module] {
    bool value = false;
    return module->VoiceProcessingAGCEnabled(&value) == 0 ? value : NO;
  });
}

- (void)setVoiceProcessingAGCEnabled:(BOOL)enabled {
  webrtc::AudioEngineDevice *module = dynamic_cast<webrtc::AudioEngineDevice *>(_native.get());
  if (module == nullptr) return;

  _workerThread->BlockingCall(
      [module, enabled] { return module->SetVoiceProcessingAGCEnabled(enabled) == 0; });
}

#pragma mark - Private

- (NSArray<RTC_OBJC_TYPE(RTCIODevice) *> *)_outputDevices {
  char guid[webrtc::kAdmMaxGuidSize + 1] = {0};
  char name[webrtc::kAdmMaxDeviceNameSize + 1] = {0};

  NSMutableArray *result = [NSMutableArray array];

  int16_t count = _native->PlayoutDevices();

  if (count > 0) {
    for (int i = 0; i < count; i++) {
      _native->PlayoutDeviceName(i, name, guid);
      NSString *strGUID = [[NSString alloc] initWithCString:guid encoding:NSUTF8StringEncoding];
      NSString *strName = [[NSString alloc] initWithCString:name encoding:NSUTF8StringEncoding];
      RTC_OBJC_TYPE(RTCIODevice) *device =
          [[RTC_OBJC_TYPE(RTCIODevice) alloc] initWithType:RTCIODeviceTypeOutput
                                                  deviceId:strGUID
                                                      name:strName];
      [result addObject:device];
    }
  }

  return result;
}

- (NSArray<RTC_OBJC_TYPE(RTCIODevice) *> *)_inputDevices {
  char guid[webrtc::kAdmMaxGuidSize + 1] = {0};
  char name[webrtc::kAdmMaxDeviceNameSize + 1] = {0};

  NSMutableArray *result = [NSMutableArray array];

  int16_t count = _native->RecordingDevices();

  if (count > 0) {
    for (int i = 0; i < count; i++) {
      _native->RecordingDeviceName(i, name, guid);
      NSString *strGUID = [[NSString alloc] initWithCString:guid encoding:NSUTF8StringEncoding];
      NSString *strName = [[NSString alloc] initWithCString:name encoding:NSUTF8StringEncoding];
      RTC_OBJC_TYPE(RTCIODevice) *device =
          [[RTC_OBJC_TYPE(RTCIODevice) alloc] initWithType:RTCIODeviceTypeInput
                                                  deviceId:strGUID
                                                      name:strName];
      [result addObject:device];
    }
  }

  return result;
}

@end

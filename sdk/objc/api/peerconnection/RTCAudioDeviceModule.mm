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

#include <AudioUnit/AudioUnit.h>

#import "RTCAudioDeviceModule.h"
#import "RTCAudioDeviceModule+Private.h"
#import "RTCIODevice+Private.h"

#import "sdk/objc/native/api/audio_device_module.h"

@implementation RTC_OBJC_TYPE (RTCAudioDeviceModule) {
  rtc::Thread *_workerThread;
  rtc::scoped_refptr<webrtc::AudioDeviceModule> _native;
}

- (instancetype)initWithNativeModule:(rtc::scoped_refptr<webrtc::AudioDeviceModule> )module
                        workerThread:(rtc::Thread * )workerThread {
  self = [super init];
  _native = module;
  _workerThread = workerThread;
  return self;
}

- (NSArray<RTC_OBJC_TYPE(RTCAudioDevice) *> *)outputDevices {

  return _workerThread->Invoke<NSArray<RTC_OBJC_TYPE(RTCAudioDevice) *> *>(RTC_FROM_HERE, [self] {
    return [self _outputDevices];
  });
}

- (NSArray<RTC_OBJC_TYPE(RTCAudioDevice) *> *)inputDevices {

  return _workerThread->Invoke<NSArray<RTC_OBJC_TYPE(RTCAudioDevice) *> *>(RTC_FROM_HERE, [self] {
    return [self _inputDevices];
  });
}

- (BOOL)setOutputDevice: (nullable RTCAudioDevice *)device {

  return _workerThread->Invoke<BOOL>(RTC_FROM_HERE, [self, device] {

    NSUInteger index = 0;
    NSArray *devices = [self _outputDevices];

    if ([devices count] == 0) {
      return NO;
    }

    if (device != nil) {
      index = [devices indexOfObjectPassingTest:^BOOL(RTCAudioDevice *e, NSUInteger i, BOOL *stop) {
        return (*stop = [e.deviceId isEqualToString:device.deviceId]);
      }];
      if (index == NSNotFound) {
        return NO;
      }
    }

    _native->StopPlayout();

    if (_native->SetPlayoutDevice(index) == 0 
        && _native->InitPlayout() == 0
        && _native->StartPlayout() == 0) {

        return YES;
    }

    return NO;
  });
}

- (BOOL)setInputDevice: (nullable RTCAudioDevice *)device {

  return _workerThread->Invoke<BOOL>(RTC_FROM_HERE, [self, device] {

    NSUInteger index = 0;
    NSArray *devices = [self _inputDevices];

    if ([devices count] == 0) {
      return NO;
    }

    if (device != nil) {
      index = [devices indexOfObjectPassingTest:^BOOL(RTCAudioDevice *e, NSUInteger i, BOOL *stop) {
        return (*stop = [e.deviceId isEqualToString:device.deviceId]);
      }];
      if (index == NSNotFound) {
        return NO;
      }
    }

    _native->StopRecording();

    if (_native->SetRecordingDevice(index) == 0 
        && _native->InitRecording() == 0
        && _native->StartRecording() == 0) {

        return YES;
    }

    return NO;
  });
}

- (BOOL)playing {

  return _workerThread->Invoke<BOOL>(RTC_FROM_HERE, [self] {
    return _native->Playing();
  });
}

- (BOOL)recording {

  return _workerThread->Invoke<BOOL>(RTC_FROM_HERE, [self] {
    return _native->Recording();
  });
}

#pragma mark - Low-level access

- (BOOL)startPlayout {

  return _workerThread->Invoke<BOOL>(RTC_FROM_HERE, [self] {
    return _native->StartPlayout() == 0;
  });
}

- (BOOL)stopPlayout {

  return _workerThread->Invoke<BOOL>(RTC_FROM_HERE, [self] {
    return _native->StopPlayout() == 0;
  });
}

- (BOOL)initPlayout {

  return _workerThread->Invoke<BOOL>(RTC_FROM_HERE, [self] {
    return _native->InitPlayout() == 0;
  });
}

- (BOOL)startRecording {

  return _workerThread->Invoke<BOOL>(RTC_FROM_HERE, [self] {
    return _native->StartRecording() == 0;
  });
}

- (BOOL)stopRecording {

  return _workerThread->Invoke<BOOL>(RTC_FROM_HERE, [self] {
    return _native->StopRecording() == 0;
  });
}

- (BOOL)initRecording {

  return _workerThread->Invoke<BOOL>(RTC_FROM_HERE, [self] {
    return _native->InitRecording() == 0;
  });
}

#pragma mark - Private

- (NSArray<RTC_OBJC_TYPE(RTCAudioDevice) *> *)_outputDevices {

  char guid[webrtc::kAdmMaxGuidSize + 1] = {0};
  char name[webrtc::kAdmMaxDeviceNameSize + 1] = {0};
  
  NSMutableArray *result = [NSMutableArray array];

  int16_t count = _native->PlayoutDevices();

  if (count > 0) {
    for (int i = 0; i < count; i++) {
      _native->PlayoutDeviceName(i, name, guid);
      NSString *strGUID = [[NSString alloc] initWithCString:guid encoding:NSUTF8StringEncoding];
      NSString *strName = [[NSString alloc] initWithCString:name encoding:NSUTF8StringEncoding];
      RTCAudioDevice *device = [[RTCAudioDevice alloc] initWithType:RTCIODeviceTypeOutput deviceId:strGUID name:strName];
      [result addObject: device];
    }
  }

  return result;
}

- (NSArray<RTC_OBJC_TYPE(RTCAudioDevice) *> *)_inputDevices {
  
  char guid[webrtc::kAdmMaxGuidSize + 1] = {0};
  char name[webrtc::kAdmMaxDeviceNameSize + 1] = {0};
  
  NSMutableArray *result = [NSMutableArray array];

  int16_t count = _native->RecordingDevices();

  if (count > 0) {
    for (int i = 0; i < count; i++) {
      _native->RecordingDeviceName(i, name, guid);
      NSString *strGUID = [[NSString alloc] initWithCString:guid encoding:NSUTF8StringEncoding];
      NSString *strName = [[NSString alloc] initWithCString:name encoding:NSUTF8StringEncoding];
      RTCAudioDevice *device = [[RTCAudioDevice alloc] initWithType:RTCIODeviceTypeInput deviceId:strGUID name:strName];
      [result addObject: device];
    }
  }

  return result;
}

@end

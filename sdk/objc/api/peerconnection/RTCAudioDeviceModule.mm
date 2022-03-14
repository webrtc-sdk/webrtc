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

#import "RTCAudioDeviceModule+Private.h"
#import "RTCIODevice+Private.h"

#import "sdk/objc/native/api/audio_device_module.h"

@implementation RTCAudioDeviceModule {
  rtc::scoped_refptr<webrtc::AudioDeviceModule> _native;
}

- (instancetype)initWithNativeModule:(rtc::scoped_refptr<webrtc::AudioDeviceModule> )module {
  self = [super init];
  _native = module;
  return self;
}

- (void)captureSampleBuffer:(CMSampleBufferRef)sampleBuffer {
  // TODO: Implement
  // _native->CaptureSampleBuffer(sampleBuffer);
}

- (NSArray<RTC_OBJC_TYPE(RTCAudioDevice) *> *)outputDevices {
  
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

- (NSArray<RTC_OBJC_TYPE(RTCAudioDevice) *> *)inputDevices {
  
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

- (BOOL)setOutputDevice: (nullable RTCAudioDevice *)device {

  NSUInteger index = 0;
  NSArray *devices = [self outputDevices];

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
}

- (BOOL)setInputDevice: (nullable RTCAudioDevice *)device {

  NSUInteger index = 0;
  NSArray *devices = [self inputDevices];

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
}

- (BOOL)playing {
  return _native->Playing();
}

- (BOOL)recording {
  return _native->Recording();
}

@end

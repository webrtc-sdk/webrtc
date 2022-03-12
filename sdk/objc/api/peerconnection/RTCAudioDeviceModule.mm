/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <AudioUnit/AudioUnit.h>

#import "RTCAudioDeviceModule+Private.h"
#import "RTCDevice+Private.h"

#include "rtc_base/ref_counted_object.h"
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

- (NSArray<RTC_OBJC_TYPE(RTCDevice) *> *)playoutDevices {
  
  char guid[webrtc::kAdmMaxGuidSize + 1] = {0};
  char name[webrtc::kAdmMaxDeviceNameSize + 1] = {0};
  
  NSMutableArray *result = [NSMutableArray array];

  int16_t count = _native->PlayoutDevices();

  if (count > 0) {
    for (int i = 0; i < count; i++) {
      _native->PlayoutDeviceName(i, name, guid);
      NSString *strGUID = [[NSString alloc] initWithCString:guid encoding:NSUTF8StringEncoding];
      NSString *strName = [[NSString alloc] initWithCString:name encoding:NSUTF8StringEncoding];
      RTCDevice *device = [[RTCDevice alloc] initWithType:RTCDeviceTypeOutput guid:strGUID name:strName];
      [result addObject: device];
    }
  }

  return result;
}

- (NSArray<RTC_OBJC_TYPE(RTCDevice) *> *)recordingDevices {
  
  char guid[webrtc::kAdmMaxGuidSize + 1] = {0};
  char name[webrtc::kAdmMaxDeviceNameSize + 1] = {0};
  
  NSMutableArray *result = [NSMutableArray array];

  int16_t count = _native->RecordingDevices();

  if (count > 0) {
    for (int i = 0; i < count; i++) {
      _native->RecordingDeviceName(i, name, guid);
      NSString *strGUID = [[NSString alloc] initWithCString:guid encoding:NSUTF8StringEncoding];
      NSString *strName = [[NSString alloc] initWithCString:name encoding:NSUTF8StringEncoding];
      RTCDevice *device = [[RTCDevice alloc] initWithType:RTCDeviceTypeInput guid:strGUID name:strName];
      [result addObject: device];
    }
  }

  return result;
}

- (BOOL)switchPlayoutDevice: (nullable RTCDevice *)device {

  NSUInteger index = 0;
  NSArray *devices = [self playoutDevices];

  if ([devices count] == 0) {
    return NO;
  }

  if (device != nil) {
    index = [devices indexOfObjectPassingTest:^BOOL(RTCDevice *e, NSUInteger i, BOOL *stop) {
      return (*stop = [e.guid isEqualToString:device.guid]);
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

- (BOOL)switchRecordingDevice: (nullable RTCDevice *)device {

  NSUInteger index = 0;
  NSArray *devices = [self recordingDevices];

  if ([devices count] == 0) {
    return NO;
  }

  if (device != nil) {
    index = [devices indexOfObjectPassingTest:^BOOL(RTCDevice *e, NSUInteger i, BOOL *stop) {
      return (*stop = [e.guid isEqualToString:device.guid]);
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

- (BOOL)setPlayoutDevice:(uint16_t) index {
  return _native->SetPlayoutDevice(index) == 0;
}

- (BOOL)startPlayout {
  return _native->StartPlayout() == 0;
}

- (BOOL)stopPlayout {
  return _native->StopPlayout() == 0;
}

- (BOOL)initPlayout {
  return _native->InitPlayout() == 0;
}

- (BOOL)recording {
  return _native->Recording();
}

- (BOOL)setRecordingDevice:(uint16_t) index {
  return _native->SetRecordingDevice(index) == 0;
}

- (BOOL)startRecording {
  return _native->StartRecording() == 0;
}

- (BOOL)stopRecording {
  return _native->StopRecording() == 0;
}

- (BOOL)initRecording {
  return _native->InitRecording() == 0;
}

@end

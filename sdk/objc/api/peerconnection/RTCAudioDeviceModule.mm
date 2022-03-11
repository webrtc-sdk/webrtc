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
  rtc::scoped_refptr<webrtc::AudioDeviceModule> _nativeModule;
}

- (instancetype)initWithNativeModule:(rtc::scoped_refptr<webrtc::AudioDeviceModule> )module {
  self = [super init];
  _nativeModule = module;
  return self;
}

- (void)captureSampleBuffer:(CMSampleBufferRef)sampleBuffer {
  // TODO: Implement
  // _nativeModule->CaptureSampleBuffer(sampleBuffer);
}

- (NSArray<RTC_OBJC_TYPE(RTCDevice) *> *)playoutDevices {
  
  char guid[webrtc::kAdmMaxGuidSize + 1] = {0};
  char name[webrtc::kAdmMaxDeviceNameSize + 1] = {0};
  
  NSMutableArray *result = [NSMutableArray array];

  int16_t count = _nativeModule->PlayoutDevices();

  if (count > 0) {
    for (int i = 0; i < count; i++) {
      _nativeModule->PlayoutDeviceName(i, name, guid);
      NSString *strGUID = [[NSString alloc] initWithCString:guid encoding:NSUTF8StringEncoding];
      NSString *strName = [[NSString alloc] initWithCString:name encoding:NSUTF8StringEncoding];
      RTCDevice *device = [[RTCDevice alloc] initWithGUID:strGUID name:strName];
      [result addObject: device];
    }
  }

  return result;
}

- (NSArray<RTC_OBJC_TYPE(RTCDevice) *> *)recordingDevices {
  
  char guid[webrtc::kAdmMaxGuidSize + 1] = {0};
  char name[webrtc::kAdmMaxDeviceNameSize + 1] = {0};
  
  NSMutableArray *result = [NSMutableArray array];

  int16_t count = _nativeModule->RecordingDevices();

  if (count > 0) {
    for (int i = 0; i < count; i++) {
      _nativeModule->RecordingDeviceName(i, name, guid);
      NSString *strGUID = [[NSString alloc] initWithCString:guid encoding:NSUTF8StringEncoding];
      NSString *strName = [[NSString alloc] initWithCString:name encoding:NSUTF8StringEncoding];
      RTCDevice *device = [[RTCDevice alloc] initWithGUID:strGUID name:strName];
      [result addObject: device];
    }
  }

  return result;
}

- (BOOL)playing {
  return _nativeModule->Playing();
}

- (BOOL)setPlayoutDevice:(uint16_t) index {
  return _nativeModule->SetPlayoutDevice(index) == 0;
}

- (BOOL)startPlayout {
  return _nativeModule->StartPlayout() == 0;
}

- (BOOL)stopPlayout {
  return _nativeModule->StopPlayout() == 0;
}

- (BOOL)initPlayout {
  return _nativeModule->InitPlayout() == 0;
}

- (BOOL)recording {
  return _nativeModule->Recording();
}

- (BOOL)setRecordingDevice:(uint16_t) index {
  return _nativeModule->SetRecordingDevice(index) == 0;
}

- (BOOL)startRecording {
  return _nativeModule->StartRecording() == 0;
}

- (BOOL)stopRecording {
  return _nativeModule->StopRecording() == 0;
}

- (BOOL)initRecording {
  return _nativeModule->InitRecording() == 0;
}

@end

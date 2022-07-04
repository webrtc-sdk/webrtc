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

#import "RTCAudioDevice.h"
#import "RTCIODevice+Private.h"

#include "rtc_base/thread.h"
#import "base/RTCLogging.h"
#import "sdk/objc/native/api/audio_device_module.h"

@implementation RTC_OBJC_TYPE (RTCAudioDevice) {
  rtc::Thread *_workerThread;
  rtc::scoped_refptr<webrtc::AudioDeviceModule> _nativeAudioDeviceModule;
}

- (instancetype)initWithNativeModule: (rtc::scoped_refptr<webrtc::AudioDeviceModule> )module
                        workerThread: (rtc::Thread * )workerThread
                                type: (RTCIODeviceType)type
                            deviceId: (NSString *)deviceId
                                name: (NSString* )name {

  RTCLogInfo(@"RTCAudioDevice initWithNativeModule:workerThread:type:deviceId:name:");

  self = [super initWithType:type deviceId:deviceId name:name];
  _nativeAudioDeviceModule = module;
  _workerThread = workerThread;

  return self;
}

@end

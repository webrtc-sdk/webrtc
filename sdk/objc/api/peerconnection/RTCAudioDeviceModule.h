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

#import <CoreMedia/CoreMedia.h>
#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCAudioDevice.h"

NS_ASSUME_NONNULL_BEGIN

typedef void (^RTCOnAudioDevicesDidUpdate)();

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCAudioDeviceModule) : NSObject

@property(nonatomic, readonly) NSArray<RTC_OBJC_TYPE(RTCAudioDevice) *> *outputDevices;
@property(nonatomic, readonly) NSArray<RTC_OBJC_TYPE(RTCAudioDevice) *> *inputDevices;

@property(nonatomic, readonly) BOOL playing;
@property(nonatomic, readonly) BOOL recording;

// Executes low-level API's in sequence to switch the device
- (BOOL)setOutputDevice: (nullable RTCAudioDevice *)device;
- (BOOL)setInputDevice: (nullable RTCAudioDevice *)device;

- (BOOL)setDevicesUpdatedHandler: (nullable RTCOnAudioDevicesDidUpdate) handler;

- (BOOL)startPlayout;
- (BOOL)stopPlayout;
- (BOOL)initPlayout;
- (BOOL)startRecording;
- (BOOL)stopRecording;
- (BOOL)initRecording;

@end

NS_ASSUME_NONNULL_END

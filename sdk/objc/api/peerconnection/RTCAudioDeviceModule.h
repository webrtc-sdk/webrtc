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

#import <AVFAudio/AVFAudio.h>
#import <CoreMedia/CoreMedia.h>
#import <Foundation/Foundation.h>

#import "RTCIODevice.h"
#import "RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, RTCSpeechActivityEvent) {
  RTCSpeechActivityEventStarted,
  RTCSpeechActivityEventEnded,
};

typedef void (^RTCDevicesDidUpdateCallback)();
typedef void (^RTCSpeechActivityCallback)(RTCSpeechActivityEvent);

typedef void (^RTCOnEngineDidCreate)(AVAudioEngine *);
typedef void (^RTCOnEngineWillEnable)(AVAudioEngine *, BOOL, BOOL);
typedef void (^RTCOnEngineWillStart)(AVAudioEngine *, BOOL, BOOL);
typedef void (^RTCOnEngineDidStop)(AVAudioEngine *, BOOL, BOOL);
typedef void (^RTCOnEngineDidDisable)(AVAudioEngine *, BOOL, BOOL);
typedef void (^RTCOnEngineWillRelease)(AVAudioEngine *);

typedef bool (^RTCOnEngineWillConnectInput)(AVAudioEngine *, AVAudioNode *, AVAudioNode *,
                                            AVAudioFormat *);
typedef bool (^RTCOnEngineWillConnectOutput)(AVAudioEngine *, AVAudioNode *, AVAudioNode *,
                                             AVAudioFormat *);

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCAudioDeviceModule) : NSObject

@property(nonatomic, readonly) NSArray<RTC_OBJC_TYPE(RTCIODevice) *> *outputDevices;
@property(nonatomic, readonly) NSArray<RTC_OBJC_TYPE(RTCIODevice) *> *inputDevices;

@property(nonatomic, readonly) BOOL playing;
@property(nonatomic, readonly) BOOL recording;

@property(nonatomic, assign) RTC_OBJC_TYPE(RTCIODevice) * outputDevice;
@property(nonatomic, assign) RTC_OBJC_TYPE(RTCIODevice) * inputDevice;

// Executes low-level API's in sequence to switch the device
// Use outputDevice / inputDevice property unless you need to know if setting the device is
// successful.
- (BOOL)trySetOutputDevice:(nullable RTC_OBJC_TYPE(RTCIODevice) *)device;
- (BOOL)trySetInputDevice:(nullable RTC_OBJC_TYPE(RTCIODevice) *)device;

- (BOOL)setDevicesDidUpdateCallback:(nullable RTCDevicesDidUpdateCallback)callback;
- (BOOL)setSpeechActivityCallback:(nullable RTCSpeechActivityCallback)callback;

- (BOOL)setOnEngineDidCreateCallback:(nullable RTCOnEngineDidCreate)callback;
- (BOOL)setOnEngineWillEnableCallback:(nullable RTCOnEngineWillEnable)callback;
- (BOOL)setOnEngineWillStartCallback:(nullable RTCOnEngineWillStart)callback;
- (BOOL)setOnEngineDidStopCallback:(nullable RTCOnEngineDidStop)callback;
- (BOOL)setOnEngineDidDisableCallback:(nullable RTCOnEngineDidDisable)callback;
- (BOOL)setOnEngineWillReleaseCallback:(nullable RTCOnEngineWillRelease)callback;

- (BOOL)setOnEngineWillConnectInputCallback:(nullable RTCOnEngineWillConnectInput)callback;
- (BOOL)setOnEngineWillConnectOutputCallback:(nullable RTCOnEngineWillConnectOutput)callback;

- (BOOL)startPlayout;
- (BOOL)stopPlayout;
- (BOOL)initPlayout;
- (BOOL)startRecording;
- (BOOL)stopRecording;
- (BOOL)initRecording;

- (BOOL)initAndStartRecording;

// Manual rendering.
@property(nonatomic, readonly, getter=isManualRenderingMode) BOOL manualRenderingMode;
- (BOOL)setManualRenderingMode:(BOOL)enabled;

// Advanced other audio ducking.
@property(nonatomic, assign, getter=isAdvancedDuckingEnabled) BOOL advancedDuckingEnabled;

@property(nonatomic, assign) NSInteger duckingLevel;

@end

NS_ASSUME_NONNULL_END

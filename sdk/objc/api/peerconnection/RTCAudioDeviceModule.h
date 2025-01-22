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

@class RTC_OBJC_TYPE(RTCAudioDeviceModule);

RTC_OBJC_EXPORT @protocol RTC_OBJC_TYPE(RTCAudioDeviceModuleDelegate)<NSObject>

- (void)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
    didReceiveSpeechActivityEvent:(RTCSpeechActivityEvent)speechActivityEvent
    NS_SWIFT_NAME(audioDeviceModule(_:didReceiveSpeechActivityEvent:));

// Engine events
- (void)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
          didCreateEngine:(AVAudioEngine *)engine
    NS_SWIFT_NAME(audioDeviceModule(_:didCreateEngine:));

- (void)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
         willEnableEngine:(AVAudioEngine *)engine
         isPlayoutEnabled:(BOOL)isPlayoutEnabled
       isRecordingEnabled:(BOOL)isRecordingEnabled
    NS_SWIFT_NAME(audioDeviceModule(_:willEnableEngine:isPlayoutEnabled:isRecordingEnabled:));

- (void)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
          willStartEngine:(AVAudioEngine *)engine
         isPlayoutEnabled:(BOOL)isPlayoutEnabled
       isRecordingEnabled:(BOOL)isRecordingEnabled
    NS_SWIFT_NAME(audioDeviceModule(_:willStartEngine:isPlayoutEnabled:isRecordingEnabled:));

- (void)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
            didStopEngine:(AVAudioEngine *)engine
         isPlayoutEnabled:(BOOL)isPlayoutEnabled
       isRecordingEnabled:(BOOL)isRecordingEnabled
    NS_SWIFT_NAME(audioDeviceModule(_:didStopEngine:isPlayoutEnabled:isRecordingEnabled:));

- (void)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
         didDisableEngine:(AVAudioEngine *)engine
         isPlayoutEnabled:(BOOL)isPlayoutEnabled
       isRecordingEnabled:(BOOL)isRecordingEnabled
    NS_SWIFT_NAME(audioDeviceModule(_:didDisableEngine:isPlayoutEnabled:isRecordingEnabled:));

- (void)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
        willReleaseEngine:(AVAudioEngine *)engine
    NS_SWIFT_NAME(audioDeviceModule(_:willReleaseEngine:));

- (BOOL)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
                      engine:(AVAudioEngine *)engine
    configureInputFromSource:(AVAudioNode *)source
               toDestination:(AVAudioNode *)destination
                  withFormat:(AVAudioFormat *)format
    NS_SWIFT_NAME(audioDeviceModule(_:engine:configureInputFromSource:toDestination:format:));

- (BOOL)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
                       engine:(AVAudioEngine *)engine
    configureOutputFromSource:(AVAudioNode *)source
                toDestination:(AVAudioNode *)destination
                   withFormat:(AVAudioFormat *)format
    NS_SWIFT_NAME(audioDeviceModule(_:engine:configureOutputFromSource:toDestination:format:));

- (void)audioDeviceModuleDidUpdateDevices:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
    NS_SWIFT_NAME(audioDeviceModuleDidUpdateDevices(_:));

@end

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

- (BOOL)startPlayout;
- (BOOL)stopPlayout;
- (BOOL)initPlayout;
- (BOOL)startRecording;
- (BOOL)stopRecording;
- (BOOL)initRecording;

- (BOOL)initAndStartRecording;

@property(nonatomic, readonly) BOOL isPlayoutInitialized;
@property(nonatomic, readonly) BOOL isRecordingInitialized;
@property(nonatomic, readonly) BOOL isPlaying;
@property(nonatomic, readonly) BOOL isRecording;

@property(nonatomic, getter=isInitRecordingPersistentMode) BOOL initRecordingPersistentMode;

@property(nonatomic, weak, nullable) id<RTC_OBJC_TYPE(RTCAudioDeviceModuleDelegate)> observer;

// Manual rendering.
@property(nonatomic, readonly, getter=isManualRenderingMode) BOOL manualRenderingMode;
- (BOOL)setManualRenderingMode:(BOOL)enabled;

// Advanced other audio ducking.
@property(nonatomic, assign, getter=isAdvancedDuckingEnabled) BOOL advancedDuckingEnabled;

@property(nonatomic, assign) NSInteger duckingLevel;

@end

NS_ASSUME_NONNULL_END

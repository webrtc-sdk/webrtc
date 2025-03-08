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

typedef NS_ENUM(NSInteger, RTCAudioEngineMuteMode) {
  RTCAudioEngineMuteModeUnknown = -1,
  RTCAudioEngineMuteModeVoiceProcessing = 0,
  RTCAudioEngineMuteModeRestartEngine = 1,
};

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCAudioEngineState) : NSObject

@property(nonatomic, readonly) BOOL isOutputEnabled;
@property(nonatomic, readonly) BOOL isOutputRunning;
@property(nonatomic, readonly) BOOL isInputEnabled;
@property(nonatomic, readonly) BOOL isInputRunning;
@property(nonatomic, readonly) BOOL isInputMuted;
@property(nonatomic, readonly) RTCAudioEngineMuteMode muteMode;

@end

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCAudioEngineStateTransition) : NSObject

@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioEngineState) * prev;
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioEngineState) * next;

@end

RTC_EXTERN NSString *const kRTCAudioEngineInputMixerNodeKey;

@class RTC_OBJC_TYPE(RTCAudioDeviceModule);

RTC_OBJC_EXPORT @protocol RTC_OBJC_TYPE
(RTCAudioDeviceModuleDelegate)<NSObject>

    - (void)audioDeviceModule
    : (RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule didReceiveMutedSpeechActivityEvent
    : (RTCSpeechActivityEvent)speechActivityEvent NS_SWIFT_NAME(audioDeviceModule(_:didReceiveMutedSpeechActivityEvent:));

// Engine events
- (NSInteger)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
               didCreateEngine:(AVAudioEngine *)engine
               stateTransition:(RTC_OBJC_TYPE(RTCAudioEngineStateTransition) *)stateTransition
    NS_SWIFT_NAME(audioDeviceModule(_:didCreateEngine:stateTransition:));

- (NSInteger)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
              willEnableEngine:(AVAudioEngine *)engine
               stateTransition:(RTC_OBJC_TYPE(RTCAudioEngineStateTransition) *)stateTransition
    NS_SWIFT_NAME(audioDeviceModule(_:willEnableEngine:stateTransition:));

- (NSInteger)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
               willStartEngine:(AVAudioEngine *)engine
               stateTransition:(RTC_OBJC_TYPE(RTCAudioEngineStateTransition) *)stateTransition
    NS_SWIFT_NAME(audioDeviceModule(_:willStartEngine:stateTransition:));

- (NSInteger)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
                 didStopEngine:(AVAudioEngine *)engine
               stateTransition:(RTC_OBJC_TYPE(RTCAudioEngineStateTransition) *)stateTransition
    NS_SWIFT_NAME(audioDeviceModule(_:didStopEngine:stateTransition:));

- (NSInteger)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
              didDisableEngine:(AVAudioEngine *)engine
               stateTransition:(RTC_OBJC_TYPE(RTCAudioEngineStateTransition) *)stateTransition
    NS_SWIFT_NAME(audioDeviceModule(_:didDisableEngine:stateTransition:));

- (NSInteger)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
             willReleaseEngine:(AVAudioEngine *)engine
               stateTransition:(RTC_OBJC_TYPE(RTCAudioEngineStateTransition) *)stateTransition
    NS_SWIFT_NAME(audioDeviceModule(_:willReleaseEngine:stateTransition:));

- (NSInteger)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
                        engine:(AVAudioEngine *)engine
      configureInputFromSource:(nullable AVAudioNode *)source
                 toDestination:(AVAudioNode *)destination
                    withFormat:(AVAudioFormat *)format
               stateTransition:(RTC_OBJC_TYPE(RTCAudioEngineStateTransition) *)stateTransition
                       context:(NSDictionary *)context
    NS_SWIFT_NAME(audioDeviceModule(_:engine:configureInputFromSource:toDestination:format:stateTransition:context:));

- (NSInteger)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
                        engine:(AVAudioEngine *)engine
     configureOutputFromSource:(AVAudioNode *)source
                 toDestination:(nullable AVAudioNode *)destination
                    withFormat:(AVAudioFormat *)format
               stateTransition:(RTC_OBJC_TYPE(RTCAudioEngineStateTransition) *)stateTransition
                       context:(NSDictionary *)context
    NS_SWIFT_NAME(audioDeviceModule(_:engine:configureOutputFromSource:toDestination:format:stateTransition:context:));

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

- (NSInteger)startPlayout;
- (NSInteger)stopPlayout;
- (NSInteger)initPlayout;
- (NSInteger)startRecording;
- (NSInteger)stopRecording;
- (NSInteger)initRecording;

- (NSInteger)initAndStartRecording;

// For testing purposes
@property(nonatomic, readonly) BOOL isPlayoutInitialized;
@property(nonatomic, readonly) BOOL isRecordingInitialized;
@property(nonatomic, readonly) BOOL isPlaying;
@property(nonatomic, readonly) BOOL isRecording;
@property(nonatomic, readonly) BOOL isEngineRunning;
@property(nonatomic, assign, getter=isMicrophoneMuted) BOOL microphoneMuted;

// Directly get & set engine state.
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioEngineState) * engineState;

@property(nonatomic, readonly, getter=isRecordingAlwaysPreparedMode)
    BOOL recordingAlwaysPreparedMode;
- (NSInteger)setRecordingAlwaysPreparedMode:(BOOL)enabled;

@property(nonatomic, weak, nullable) id<RTC_OBJC_TYPE(RTCAudioDeviceModuleDelegate)> observer;

// Manual rendering.
@property(nonatomic, readonly, getter=isManualRenderingMode) BOOL manualRenderingMode;
- (NSInteger)setManualRenderingMode:(BOOL)enabled;

// Advanced other audio ducking.
@property(nonatomic, assign, getter=isAdvancedDuckingEnabled) BOOL advancedDuckingEnabled;

// Audio ducking level. See `AVAudioVoiceProcessingOtherAudioDuckingLevel` enum for valid values.
@property(nonatomic, assign) NSInteger duckingLevel;

@property(nonatomic, readonly) RTCAudioEngineMuteMode muteMode;
- (NSInteger)setMuteMode:(RTCAudioEngineMuteMode)mode;

/// Indicates whether Voice-Processing I/O is enabled. Requires restarting the Audio Engine to
/// toggle. Defaults to true.
@property(nonatomic, readonly, getter=isVoiceProcessingEnabled) BOOL voiceProcessingEnabled;
- (NSInteger)setVoiceProcessingEnabled:(BOOL)enabled;

/// Temporarily bypasses Voice-Processing I/O. Can be toggled at runtime without restarting the
/// Audio Engine. Defaults to false.
@property(nonatomic, assign, getter=isVoiceProcessingBypassed) BOOL voiceProcessingBypassed;

/// Indicates whether Automatic Gain Control (AGC) is enabled. Requires Voice-Processing I/O to be
/// enabled. Enabled by default when VPIO is enabled.
@property(nonatomic, assign, getter=isVoiceProcessingAGCEnabled) BOOL voiceProcessingAGCEnabled;

@end

NS_ASSUME_NONNULL_END

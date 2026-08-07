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

typedef NS_ENUM(NSInteger, RTC_OBJC_TYPE(RTCAudioDeviceModuleType)) {
  RTC_OBJC_TYPE(RTCAudioDeviceModuleTypePlatformDefault),
  RTC_OBJC_TYPE(RTCAudioDeviceModuleTypeAudioEngine),
};

typedef NS_ENUM(NSInteger, RTC_OBJC_TYPE(RTCSpeechActivityEvent)) {
  RTC_OBJC_TYPE(RTCSpeechActivityEventStarted),
  RTC_OBJC_TYPE(RTCSpeechActivityEventEnded),
};

typedef NS_ENUM(NSInteger, RTC_OBJC_TYPE(RTCAudioEngineMuteMode)) {
  RTC_OBJC_TYPE(RTCAudioEngineMuteModeUnknown) = -1,
  RTC_OBJC_TYPE(RTCAudioEngineMuteModeVoiceProcessing) = 0,
  RTC_OBJC_TYPE(RTCAudioEngineMuteModeRestartEngine) = 1,
  RTC_OBJC_TYPE(RTCAudioEngineMuteModeInputMixer) = 2,
};

// Ducking level for voice processing.
// Maps to AVAudioVoiceProcessingOtherAudioDuckingLevel (iOS 17.0+, macOS 14.0+).
typedef NS_ENUM(NSInteger, RTC_OBJC_TYPE(RTCAudioDuckingLevel)) {
  RTC_OBJC_TYPE(RTCAudioDuckingLevelDefault) = 0,
  RTC_OBJC_TYPE(RTCAudioDuckingLevelMin) = 1,
  RTC_OBJC_TYPE(RTCAudioDuckingLevelMid) = 2,
  RTC_OBJC_TYPE(RTCAudioDuckingLevelMax) = 3,
};

typedef struct {
  BOOL outputEnabled;
  BOOL outputRunning;
  BOOL inputEnabled;
  BOOL inputRunning;
  BOOL inputMuted;
  RTC_OBJC_TYPE(RTCAudioEngineMuteMode) muteMode;
} RTC_OBJC_TYPE(RTCAudioEngineState);

typedef struct {
  BOOL isInputAvailable;
  BOOL isOutputAvailable;
} RTC_OBJC_TYPE(RTCAudioEngineAvailability);

// Values must match webrtc::AudioDeviceModule::PlatformAudioProcessingTopology.
typedef NS_ENUM(NSInteger, RTC_OBJC_TYPE(RTCPlatformAudioProcessingTopology)) {
  RTC_OBJC_TYPE(RTCPlatformAudioProcessingTopologyIndependent) = 0,
  RTC_OBJC_TYPE(RTCPlatformAudioProcessingTopologyEchoCancellationAndNoiseSuppressionCoupled) = 1,
};

/** Diagnostic state of one platform (device built-in) processing effect.
 *  Requested/active read NO when the ADM or OS path cannot report the value.
 */
RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCPlatformAudioProcessingComponentState) : NSObject

/** Whether the device offers this effect at all. */
@property(nonatomic, readonly, getter=isAvailable) BOOL available;
/** The last state requested from the ADM. */
@property(nonatomic, readonly, getter=isRequested) BOOL requested;
/** Live OS readback when the ADM can query the effect. */
@property(nonatomic, readonly, getter=isActive) BOOL active;

- (instancetype)init NS_UNAVAILABLE;

@end

/** Device-level snapshot of platform audio processing.
 *
 *  On Apple AudioEngine, AEC and NS are coupled through Voice Processing I/O,
 *  so one shared platform path can affect both components (see `topology`).
 *
 *  The voiceProcessing* properties reflect the Apple Voice Processing I/O
 *  unit. Requested values are the state stored by the ADM and can be known
 *  before input is configured: enabled maps to AVAudioInputNode
 *  setVoiceProcessingEnabled (turning it off removes the VPIO graph
 *  entirely); bypassed maps to voiceProcessingBypassed while VPIO is enabled
 *  (disables Apple's coupled AEC/NS path without rebuilding the engine); AGC
 *  has a separate switch that only has an effect while VPIO is active.
 *  Active values are live readback from the platform input node; they read NO
 *  before input is configured, after the input path is torn down, or where
 *  the value is not observable, and can temporarily differ from requested
 *  while the engine applies a transition or if the OS rejects a request.
 */
RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCPlatformAudioProcessingState) : NSObject

@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCPlatformAudioProcessingTopology) topology;

@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCPlatformAudioProcessingComponentState) *echoCancellation;
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCPlatformAudioProcessingComponentState) *noiseSuppression;
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCPlatformAudioProcessingComponentState) *autoGainControl;

@property(nonatomic, readonly, getter=isVoiceProcessingEnabledRequested) BOOL voiceProcessingEnabledRequested;
@property(nonatomic, readonly, getter=isVoiceProcessingBypassedRequested) BOOL voiceProcessingBypassedRequested;
@property(nonatomic, readonly, getter=isVoiceProcessingAGCEnabledRequested) BOOL voiceProcessingAGCEnabledRequested;
@property(nonatomic, readonly, getter=isVoiceProcessingEnabledActive) BOOL voiceProcessingEnabledActive;
@property(nonatomic, readonly, getter=isVoiceProcessingBypassedActive) BOOL voiceProcessingBypassedActive;
@property(nonatomic, readonly, getter=isVoiceProcessingAGCEnabledActive) BOOL voiceProcessingAGCEnabledActive;

- (instancetype)init NS_UNAVAILABLE;

@end

RTC_EXTERN NSString *const RTC_CONSTANT_TYPE(RTCAudioEngineInputMixerNodeKey);

@class RTC_OBJC_TYPE(RTCAudioDeviceModule);
@class RTC_OBJC_TYPE(RTCAudioProcessingOptions);

RTC_OBJC_EXPORT @protocol RTC_OBJC_TYPE
(RTCAudioDeviceModuleDelegate)<NSObject>

    - (void)audioDeviceModule
    : (RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule didReceiveSpeechActivityEvent
    : (RTC_OBJC_TYPE(RTCSpeechActivityEvent))speechActivityEvent NS_SWIFT_NAME(audioDeviceModule(_:didReceiveSpeechActivityEvent:));

// Engine events
- (NSInteger)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
               didCreateEngine:(AVAudioEngine *)engine
    NS_SWIFT_NAME(audioDeviceModule(_:didCreateEngine:));

- (NSInteger)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
               willStartEngine:(AVAudioEngine *)engine
              isPlayoutEnabled:(BOOL)isPlayoutEnabled
            isRecordingEnabled:(BOOL)isRecordingEnabled
    NS_SWIFT_NAME(audioDeviceModule(_:willStartEngine:isPlayoutEnabled:isRecordingEnabled:));

- (NSInteger)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
                 didStopEngine:(AVAudioEngine *)engine
              isPlayoutEnabled:(BOOL)isPlayoutEnabled
            isRecordingEnabled:(BOOL)isRecordingEnabled
    NS_SWIFT_NAME(audioDeviceModule(_:didStopEngine:isPlayoutEnabled:isRecordingEnabled:));

- (NSInteger)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
              didDisableEngine:(AVAudioEngine *)engine
              isPlayoutEnabled:(BOOL)isPlayoutEnabled
            isRecordingEnabled:(BOOL)isRecordingEnabled
    NS_SWIFT_NAME(audioDeviceModule(_:didDisableEngine:isPlayoutEnabled:isRecordingEnabled:));

- (NSInteger)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
             willReleaseEngine:(AVAudioEngine *)engine
    NS_SWIFT_NAME(audioDeviceModule(_:willReleaseEngine:));

- (NSInteger)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
                        engine:(AVAudioEngine *)engine
      configureInputFromSource:(nullable AVAudioNode *)source
                 toDestination:(AVAudioNode *)destination
                    withFormat:(AVAudioFormat *)format
                       context:(NSDictionary *)context
    NS_SWIFT_NAME(audioDeviceModule(_:engine:configureInputFromSource:toDestination:format:context:));

- (NSInteger)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
                        engine:(AVAudioEngine *)engine
     configureOutputFromSource:(AVAudioNode *)source
                 toDestination:(nullable AVAudioNode *)destination
                    withFormat:(AVAudioFormat *)format
                       context:(NSDictionary *)context
    NS_SWIFT_NAME(audioDeviceModule(_:engine:configureOutputFromSource:toDestination:format:context:));

- (void)audioDeviceModuleDidUpdateDevices:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
    NS_SWIFT_NAME(audioDeviceModuleDidUpdateDevices(_:));

@optional

// Exactly one of the two `willEnableEngine` variants below should be
// implemented. They are optional only so that a single delegate can be
// written against both this release and earlier ones; if both are
// implemented, only the first is called.

// `isVoiceProcessingEnabled` reports the resolved Apple Voice Processing I/O
// state the engine is transitioning to, which is not yet readable from the
// module when this fires. Observers that configure the audio session use it
// to pick a session mode matching the processing implementation.
- (NSInteger)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
              willEnableEngine:(AVAudioEngine *)engine
              isPlayoutEnabled:(BOOL)isPlayoutEnabled
            isRecordingEnabled:(BOOL)isRecordingEnabled
      isVoiceProcessingEnabled:(BOOL)isVoiceProcessingEnabled
    NS_SWIFT_NAME(audioDeviceModule(_:willEnableEngine:isPlayoutEnabled:isRecordingEnabled:isVoiceProcessingEnabled:));

// Deprecated: called only when the variant above is not implemented.
- (NSInteger)audioDeviceModule:(RTC_OBJC_TYPE(RTCAudioDeviceModule) *)audioDeviceModule
              willEnableEngine:(AVAudioEngine *)engine
              isPlayoutEnabled:(BOOL)isPlayoutEnabled
            isRecordingEnabled:(BOOL)isRecordingEnabled
    NS_SWIFT_NAME(audioDeviceModule(_:willEnableEngine:isPlayoutEnabled:isRecordingEnabled:))
        __attribute__((deprecated("Use the isVoiceProcessingEnabled: variant.")));

@end

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE(RTCAudioDeviceModule) : NSObject

@property(nonatomic, readonly) NSArray<RTC_OBJC_TYPE(RTCIODevice) *> *outputDevices;
@property(nonatomic, readonly) NSArray<RTC_OBJC_TYPE(RTCIODevice) *> *inputDevices;

@property(nonatomic, readonly) BOOL playing;
@property(nonatomic, readonly) BOOL recording;

@property(nonatomic, strong) RTC_OBJC_TYPE(RTCIODevice) * outputDevice;
@property(nonatomic, strong) RTC_OBJC_TYPE(RTCIODevice) * inputDevice;

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
- (NSInteger)initAndStartRecordingWithAudioProcessingOptions:
    (nullable RTC_OBJC_TYPE(RTCAudioProcessingOptions) *)options
    NS_SWIFT_NAME(initAndStartRecording(audioProcessingOptions:));

- (NSInteger)setEngineAvailability:(RTC_OBJC_TYPE(RTCAudioEngineAvailability))availability;

// For testing purposes
@property(nonatomic, readonly) BOOL isPlayoutInitialized;
@property(nonatomic, readonly) BOOL isRecordingInitialized;
@property(nonatomic, readonly) BOOL isPlaying;
@property(nonatomic, readonly) BOOL isRecording;
@property(nonatomic, readonly) BOOL isEngineRunning;
@property(nonatomic, readonly) BOOL isMicrophoneMuted;
- (NSInteger)setMicrophoneMuted:(BOOL)muted;

// Directly get & set engine state.
@property(nonatomic, assign) RTC_OBJC_TYPE(RTCAudioEngineState) engineState;

@property(nonatomic, readonly, getter=isRecordingAlwaysPreparedMode)
    BOOL recordingAlwaysPreparedMode;
- (NSInteger)setRecordingAlwaysPreparedMode:(BOOL)enabled;
- (NSInteger)setRecordingAlwaysPreparedMode:(BOOL)enabled
                     audioProcessingOptions:(nullable RTC_OBJC_TYPE(RTCAudioProcessingOptions) *)options
    NS_SWIFT_NAME(setRecordingAlwaysPreparedMode(_:audioProcessingOptions:));

@property(nonatomic, weak, nullable) id<RTC_OBJC_TYPE(RTCAudioDeviceModuleDelegate)> observer;

// Manual rendering.
@property(nonatomic, readonly, getter=isManualRenderingMode) BOOL manualRenderingMode;
- (NSInteger)setManualRenderingMode:(BOOL)enabled;

// Advanced other audio ducking.
@property(nonatomic, assign, getter=isAdvancedDuckingEnabled) BOOL advancedDuckingEnabled;

// Audio ducking level. Maps to AVAudioVoiceProcessingOtherAudioDuckingLevel (iOS 17.0+, macOS 14.0+).
@property(nonatomic, assign) RTC_OBJC_TYPE(RTCAudioDuckingLevel) duckingLevel;

@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioEngineMuteMode) muteMode;
- (NSInteger)setMuteMode:(RTC_OBJC_TYPE(RTCAudioEngineMuteMode))mode;

/// App-level policy for Apple's platform voice processing. Defaults to true.
///
/// When this is false, runtime audio-processing options treat Apple Voice
/// Processing I/O as unavailable. Automatic mode falls back to WebRTC software
/// processing and platform mode is rejected by APIs that validate against this
/// ADM, including track requests created by factories that expose this ADM.
/// Factory paths without an exposed ADM may still store a platform request. In
/// that case the platform request resolves disabled at apply time. Turning this
/// off also tears down any currently requested VPIO path.
@property(nonatomic, readonly, getter=isPlatformVoiceProcessingAllowed) BOOL platformVoiceProcessingAllowed;
- (NSInteger)setPlatformVoiceProcessingAllowed:(BOOL)allowed;

/// Temporarily bypasses Voice-Processing I/O. Can be toggled at runtime without restarting the
/// Audio Engine. Defaults to false.
@property(nonatomic, assign, getter=isVoiceProcessingBypassed) BOOL voiceProcessingBypassed;

/// Indicates whether Automatic Gain Control (AGC) is enabled. Requires Voice-Processing I/O to be
/// enabled. Enabled by default when VPIO is enabled.
@property(nonatomic, assign, getter=isVoiceProcessingAGCEnabled) BOOL voiceProcessingAGCEnabled;

@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioEngineAvailability) engineAvailability;

/// Diagnostic snapshot of platform audio processing state. Requested values are
/// the last state requested from the ADM. Active values are live OS readback
/// when the ADM can query the effect.
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCPlatformAudioProcessingState) *platformAudioProcessingState;

@end

NS_ASSUME_NONNULL_END

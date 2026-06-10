/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMediaStreamTrack.h"
#import "sdk/objc/base/RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

@protocol RTC_OBJC_TYPE (RTCAudioRenderer);
@class RTC_OBJC_TYPE(RTCAudioSource);

/** Selects the implementation for one enabled audio processing component.
 *
 * Disabled components do not use platform or software processing regardless of
 * mode. Automatic uses platform processing when available and otherwise falls
 * back to WebRTC software processing. Platform uses only platform processing, so
 * unavailable or physically impossible platform requests are rejected. Software
 * disables the matching platform effect and uses WebRTC software processing.
 * Some ADMs expose coupled platform effects, such as Apple Voice Processing I/O
 * for AEC and NS. High-pass filter has no platform implementation today.
 *
 * Values must match webrtc::AudioProcessingMode.
 */
typedef NS_ENUM(NSInteger, RTC_OBJC_TYPE(RTCAudioProcessingMode)) {
  RTC_OBJC_TYPE(RTCAudioProcessingModeAutomatic) = 0,
  RTC_OBJC_TYPE(RTCAudioProcessingModePlatform) = 1,
  RTC_OBJC_TYPE(RTCAudioProcessingModeSoftware) = 2,
};

// Values must match webrtc::AudioProcessingOptionsResultCode.
typedef NS_ENUM(NSInteger, RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCode)) {
  /** Options were applied immediately by the component handling the request. */
  RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeApplied) = 0,
  /** Options were accepted and stored. Active senders reapply them separately. */
  RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeStored) = 1,
  /** The track is remote; processing options only apply to local tracks. */
  RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeRejectedRemoteTrack) = 2,
  /** The per-component modes conflict, e.g. requesting platform for one of a
   *  coupled AEC/NS pair while disabling or forcing software on the other. */
  RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeRejectedInvalidCombination) = 3,
  /** Platform mode was requested for a component this device cannot provide,
   *  or platform voice processing is disallowed by policy. */
  RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeRejectedPlatformUnavailable) = 4,
  /** The request was valid but could not be applied. */
  RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeApplyFailed) = 5,
};

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCAudioProcessingOptionsResult) : NSObject

@property(nonatomic, readonly, getter=isSuccess) BOOL success;
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCode) code;
@property(nonatomic, readonly) NSString *message;

- (instancetype)init NS_UNAVAILABLE;

@end

/** Enabled flag and implementation mode for one audio processing component. */
RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCAudioProcessingComponentOptions) : NSObject

@property(nonatomic, readonly, getter=isEnabled) BOOL enabled;
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioProcessingMode) mode;

- (instancetype)initWithEnabled:(BOOL)enabled;

- (instancetype)initWithEnabled:(BOOL)enabled
                           mode:(RTC_OBJC_TYPE(RTCAudioProcessingMode))mode NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCAudioProcessingOptions) : NSObject

@property(nonatomic, readonly) BOOL echoCancellation;
@property(nonatomic, readonly) BOOL noiseSuppression;
@property(nonatomic, readonly) BOOL autoGainControl;
@property(nonatomic, readonly) BOOL highPassFilter;
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioProcessingMode) echoCancellationMode;
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioProcessingMode) noiseSuppressionMode;
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioProcessingMode) autoGainControlMode;
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioProcessingMode) highPassFilterMode;

- (instancetype)initWithEchoCancellation:(BOOL)echoCancellation
                        noiseSuppression:(BOOL)noiseSuppression
                         autoGainControl:(BOOL)autoGainControl
                          highPassFilter:(BOOL)highPassFilter;

- (instancetype)
    initWithEchoCancellationOptions:(RTC_OBJC_TYPE(RTCAudioProcessingComponentOptions) *)echoCancellationOptions
            noiseSuppressionOptions:(RTC_OBJC_TYPE(RTCAudioProcessingComponentOptions) *)noiseSuppressionOptions
             autoGainControlOptions:(RTC_OBJC_TYPE(RTCAudioProcessingComponentOptions) *)autoGainControlOptions
              highPassFilterOptions:(RTC_OBJC_TYPE(RTCAudioProcessingComponentOptions) *)highPassFilterOptions
    NS_DESIGNATED_INITIALIZER;

+ (instancetype)communicationOptions;
+ (instancetype)rawOptions;

- (instancetype)init NS_UNAVAILABLE;

@end

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCAudioTrack) : RTC_OBJC_TYPE(RTCMediaStreamTrack)

- (instancetype)init NS_UNAVAILABLE;

/** The audio source for this audio track. */
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioSource) * source;

- (void)addRenderer:(id<RTC_OBJC_TYPE(RTCAudioRenderer)>)renderer;

- (void)removeRenderer:(id<RTC_OBJC_TYPE(RTCAudioRenderer)>)renderer;

- (void)removeAllRenderers;

/** Updates local source audio processing options without restarting capture.
 *
 * Returns Stored when the request was accepted and stored on the local source.
 * This validates static platform constraints, such as Apple AEC/NS coupling and
 * simulator support. When the factory exposes its audio device module, this also
 * validates app-level platform policy such as
 * RTCAudioDeviceModule.platformVoiceProcessingAllowed. Factory paths without an
 * exposed ADM still validate statically, and runtime platform policy can make a
 * stored platform request resolve disabled when the voice engine applies it.
 *
 * If the track is already being sent, active senders observe the track update
 * and reapply the updated options through the voice engine. Rejections mean
 * the options were not stored. Use the peer connection factory's
 * audioProcessingState to inspect the effective software/platform state after
 * application. The audio processing module configuration is shared by the
 * voice engine/channel, so conflicting updates from multiple local tracks are
 * not isolated per track.
 */
- (RTC_OBJC_TYPE(RTCAudioProcessingOptionsResult) *)setAudioProcessingOptions:
    (RTC_OBJC_TYPE(RTCAudioProcessingOptions) *)options;

@end

NS_ASSUME_NONNULL_END

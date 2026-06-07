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
 * unavailable platform support resolves to disabled. Software disables the
 * matching platform effect and uses WebRTC software processing. Some ADMs expose
 * coupled platform effects, such as Apple Voice Processing I/O for AEC and NS.
 * High-pass filter has no platform implementation today, so platform HPF
 * resolves to disabled.
 */
typedef NS_ENUM(NSInteger, RTC_OBJC_TYPE(RTCAudioProcessingMode)) {
  RTC_OBJC_TYPE(RTCAudioProcessingModeAutomatic) = 0,
  RTC_OBJC_TYPE(RTCAudioProcessingModePlatform) = 1,
  RTC_OBJC_TYPE(RTCAudioProcessingModeSoftware) = 2,
};

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCAudioProcessingOptions) : NSObject

@property(nonatomic, readonly) BOOL echoCancellation;
@property(nonatomic, readonly) BOOL noiseSuppression;
@property(nonatomic, readonly) BOOL autoGainControl;
@property(nonatomic, readonly) BOOL highPassFilter;
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioProcessingMode)
    echoCancellationMode;
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioProcessingMode)
    noiseSuppressionMode;
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioProcessingMode)
    autoGainControlMode;
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioProcessingMode)
    highPassFilterMode;

- (instancetype)initWithEchoCancellation:(BOOL)echoCancellation
                        noiseSuppression:(BOOL)noiseSuppression
                         autoGainControl:(BOOL)autoGainControl
                          highPassFilter:(BOOL)highPassFilter;

- (instancetype)
    initWithEchoCancellation:(BOOL)echoCancellation
            noiseSuppression:(BOOL)noiseSuppression
             autoGainControl:(BOOL)autoGainControl
              highPassFilter:(BOOL)highPassFilter
        echoCancellationMode:
            (RTC_OBJC_TYPE(RTCAudioProcessingMode))echoCancellationMode
        noiseSuppressionMode:
            (RTC_OBJC_TYPE(RTCAudioProcessingMode))noiseSuppressionMode
         autoGainControlMode:
             (RTC_OBJC_TYPE(RTCAudioProcessingMode))autoGainControlMode
          highPassFilterMode:
              (RTC_OBJC_TYPE(RTCAudioProcessingMode))highPassFilterMode
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
 * If the track is already being sent, active senders observe the track update
 * and reapply the updated options. The effective audio processing module
 * configuration is shared by the voice engine/channel, so conflicting updates
 * from multiple local tracks are not isolated per track. Returns YES when the
 * request was accepted and stored on the local source. Platform availability
 * and the effective implementation are reported through runtime diagnostics.
 */
- (BOOL)setAudioProcessingOptions:
    (RTC_OBJC_TYPE(RTCAudioProcessingOptions) *)options;

- (BOOL)setAudioProcessingOptionsWithEchoCancellation:(BOOL)echoCancellation
                                    noiseSuppression:(BOOL)noiseSuppression
                                     autoGainControl:(BOOL)autoGainControl
                                      highPassFilter:(BOOL)highPassFilter;

@end

NS_ASSUME_NONNULL_END

/*
 * Copyright 2026 LiveKit
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

#ifndef SDK_OBJC_API_PEERCONNECTION_RTCAUDIOPROCESSINGSTATE_H_
#define SDK_OBJC_API_PEERCONNECTION_RTCAUDIOPROCESSINGSTATE_H_

#import <Foundation/Foundation.h>

#import "RTCAudioTrack.h"
#import "RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

/** The implementation that is in effect for an audio processing component.
 *
 *  Values must match webrtc::AudioProcessingImplementation.
 */
typedef NS_ENUM(NSInteger, RTC_OBJC_TYPE(RTCAudioProcessingImplementation)) {
  RTC_OBJC_TYPE(RTCAudioProcessingImplementationUnknown) = 0,
  RTC_OBJC_TYPE(RTCAudioProcessingImplementationDisabled) = 1,
  RTC_OBJC_TYPE(RTCAudioProcessingImplementationSoftware) = 2,
  RTC_OBJC_TYPE(RTCAudioProcessingImplementationPlatform) = 3,
  RTC_OBJC_TYPE(RTCAudioProcessingImplementationSoftwareAndPlatform) = 4,
};

/** Diagnostic state of one audio processing component (echo cancellation,
 *  noise suppression, auto gain control or high-pass filter), observed at
 *  three stages of one pipeline: requested (caller intent) -> resolved (the
 *  engine's per-path decision) -> active (live truth), with `effective` as
 *  the merged verdict.
 *
 *  Example: echo cancellation enabled with automatic mode on a device with
 *  platform AEC reads requested={YES, automatic}, isSoftwareResolved=NO,
 *  isPlatformResolved=YES, isPlatformActive=YES, effective=Platform. On a
 *  device without platform AEC the same request reads isSoftwareResolved=YES,
 *  isSoftwareActive=YES, effective=Software. isPlatformResolved=YES with
 *  isPlatformActive=NO means the OS has not finished applying the request or
 *  rejected it.
 */
RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCAudioProcessingComponentState) : NSObject

/** The caller's most recent request for this component, as passed to
 *  -[RTCAudioTrack setAudioProcessingOptions:] - enabled flag plus
 *  implementation mode. Nil when no audio processing options have ever been
 *  applied, which distinguishes "nobody asked" from "asked for disabled".
 *  The mode reads automatic when the request did not specify one.
 */
@property(nonatomic, readonly, nullable)
    RTC_OBJC_TYPE(RTCAudioProcessingComponentOptions) *requested;

/** Whether the resolver decided the WebRTC software (APM) implementation
 *  should run for this component, after weighing the requested mode against
 *  platform availability, coupling, and policy. Automatic mode resolves to
 *  software exactly when the platform path is unavailable or disallowed.
 *  NO also covers "the resolver has not run yet" - check `requested` to tell
 *  the two apart.
 */
@property(nonatomic, readonly, getter=isSoftwareResolved) BOOL softwareResolved;

/** Whether APM's live configuration currently has this component enabled.
 *  Normally equals isSoftwareResolved once options are applied; differs while
 *  an apply is in flight, if applying failed, or if something else has since
 *  reconfigured the shared APM.
 */
@property(nonatomic, readonly, getter=isSoftwareActive) BOOL softwareActive;

/** Whether this device/OS offers a built-in implementation of this component
 *  at all (e.g. Apple Voice Processing I/O provides AEC and NS; no platform
 *  implements the high-pass filter). Capability only - says nothing about
 *  whether it is in use.
 */
@property(nonatomic, readonly, getter=isPlatformAvailable) BOOL platformAvailable;

/** Whether the resolver decided the platform implementation should run, as
 *  submitted to the OS. Unlike the software path, the OS owns the outcome:
 *  it can decline, defer, or couple this with another component (Apple ties
 *  AEC and NS through one Voice Processing switch).
 */
@property(nonatomic, readonly, getter=isPlatformResolved) BOOL platformResolved;

/** Whether the device reports the platform implementation actually running
 *  right now. Lags isPlatformResolved during engine transitions; stays NO if
 *  the OS rejected the request or the input path is not configured.
 */
@property(nonatomic, readonly, getter=isPlatformActive) BOOL platformActive;

/** The verdict: which implementation is in effect for this component right
 *  now. Derived from the active states (active wins over resolved when they
 *  disagree). Unknown when the pipeline state cannot be determined.
 */
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioProcessingImplementation) effective;

- (instancetype)init NS_UNAVAILABLE;

@end

/** Diagnostic snapshot of the resolved audio processing state for the shared
 *  audio processing module. The module is owned by the peer connection
 *  factory and shared engine-wide, so this reflects factory-scoped state
 *  rather than the state of a single track or peer connection.
 *
 *  Device-level platform processing detail - topology, raw per-effect
 *  availability, Apple Voice Processing I/O state - is intentionally not
 *  embedded here; read RTCAudioDeviceModule.platformAudioProcessingState
 *  instead.
 */
RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCAudioProcessingState) : NSObject

@property(nonatomic, readonly) BOOL hasAudioProcessingModule;

@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioProcessingComponentState) *echoCancellation;
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioProcessingComponentState) *noiseSuppression;
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioProcessingComponentState) *autoGainControl;
@property(nonatomic, readonly) RTC_OBJC_TYPE(RTCAudioProcessingComponentState) *highPassFilter;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // SDK_OBJC_API_PEERCONNECTION_RTCAUDIOPROCESSINGSTATE_H_

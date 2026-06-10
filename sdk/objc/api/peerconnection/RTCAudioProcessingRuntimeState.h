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

#ifndef SDK_OBJC_API_PEERCONNECTION_RTCAUDIOPROCESSINGRUNTIMESTATE_H_
#define SDK_OBJC_API_PEERCONNECTION_RTCAUDIOPROCESSINGRUNTIMESTATE_H_

#import <Foundation/Foundation.h>

#import "RTCAudioDeviceModule.h"
#import "RTCAudioTrack.h"
#import "RTCMacros.h"

// Diagnostic snapshot of the requested vs. resolved audio processing state for
// the shared audio processing module (owned by the peer connection factory).

// Values must match webrtc::AudioProcessingImplementation.
typedef NS_ENUM(NSInteger, RTC_OBJC_TYPE(RTCAudioProcessingImplementation)) {
  RTC_OBJC_TYPE(RTCAudioProcessingImplementationUnknown) = 0,
  RTC_OBJC_TYPE(RTCAudioProcessingImplementationDisabled) = 1,
  RTC_OBJC_TYPE(RTCAudioProcessingImplementationSoftware) = 2,
  RTC_OBJC_TYPE(RTCAudioProcessingImplementationPlatform) = 3,
  RTC_OBJC_TYPE(RTCAudioProcessingImplementationSoftwareAndPlatform) = 4,
};

typedef struct {
  RTC_OBJC_TYPE(RTCOptionalBool) requestedEnabled;
  BOOL hasRequestedMode;
  RTC_OBJC_TYPE(RTCAudioProcessingMode) requestedMode;

  RTC_OBJC_TYPE(RTCOptionalBool) resolvedSoftwareEnabled;
  RTC_OBJC_TYPE(RTCOptionalBool) softwareEnabled;
  BOOL isPlatformAvailable;
  RTC_OBJC_TYPE(RTCOptionalBool) platformRequested;
  RTC_OBJC_TYPE(RTCOptionalBool) platformActive;

  RTC_OBJC_TYPE(RTCAudioProcessingImplementation) effective;
} RTC_OBJC_TYPE(RTCAudioProcessingComponentRuntimeState);

typedef struct {
  RTC_OBJC_TYPE(RTCBuiltInAudioProcessingTopology) topology;

  BOOL hasAudioProcessingModule;
  BOOL hasAudioProcessingConfig;
  BOOL hasRequestedAudioProcessingOptions;
  BOOL hasResolvedAudioProcessingOptions;

  RTC_OBJC_TYPE(RTCAudioProcessingComponentRuntimeState) echoCancellation;
  RTC_OBJC_TYPE(RTCAudioProcessingComponentRuntimeState) noiseSuppression;
  RTC_OBJC_TYPE(RTCAudioProcessingComponentRuntimeState) autoGainControl;
  RTC_OBJC_TYPE(RTCAudioProcessingComponentRuntimeState) highPassFilter;

  RTC_OBJC_TYPE(RTCBuiltInAudioProcessingState) builtIn;
} RTC_OBJC_TYPE(RTCAudioProcessingRuntimeState);

#endif  // SDK_OBJC_API_PEERCONNECTION_RTCAUDIOPROCESSINGRUNTIMESTATE_H_

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

#import "RTCAudioProcessingState+Private.h"

#include <optional>

#include "api/audio/audio_processing_options_result.h"
#include "api/audio_options.h"

// The ObjC enums travel across the language boundary by numeric value; verify
// they stay in sync with their webrtc counterparts at compile time.
static_assert(static_cast<NSInteger>(RTC_OBJC_TYPE(RTCAudioProcessingImplementationUnknown)) ==
                  static_cast<NSInteger>(webrtc::AudioProcessingImplementation::kUnknown),
              "RTCAudioProcessingImplementation out of sync");
static_assert(static_cast<NSInteger>(RTC_OBJC_TYPE(RTCAudioProcessingImplementationDisabled)) ==
                  static_cast<NSInteger>(webrtc::AudioProcessingImplementation::kDisabled),
              "RTCAudioProcessingImplementation out of sync");
static_assert(static_cast<NSInteger>(RTC_OBJC_TYPE(RTCAudioProcessingImplementationSoftware)) ==
                  static_cast<NSInteger>(webrtc::AudioProcessingImplementation::kSoftware),
              "RTCAudioProcessingImplementation out of sync");
static_assert(static_cast<NSInteger>(RTC_OBJC_TYPE(RTCAudioProcessingImplementationPlatform)) ==
                  static_cast<NSInteger>(webrtc::AudioProcessingImplementation::kPlatform),
              "RTCAudioProcessingImplementation out of sync");
static_assert(static_cast<NSInteger>(RTC_OBJC_TYPE(RTCAudioProcessingImplementationSoftwareAndPlatform)) ==
                  static_cast<NSInteger>(webrtc::AudioProcessingImplementation::kSoftwareAndPlatform),
              "RTCAudioProcessingImplementation out of sync");

static_assert(static_cast<NSInteger>(RTC_OBJC_TYPE(RTCAudioProcessingModeAutomatic)) ==
                  static_cast<NSInteger>(webrtc::AudioProcessingMode::kAutomatic),
              "RTCAudioProcessingMode out of sync");
static_assert(static_cast<NSInteger>(RTC_OBJC_TYPE(RTCAudioProcessingModePlatform)) ==
                  static_cast<NSInteger>(webrtc::AudioProcessingMode::kPlatform),
              "RTCAudioProcessingMode out of sync");
static_assert(static_cast<NSInteger>(RTC_OBJC_TYPE(RTCAudioProcessingModeSoftware)) ==
                  static_cast<NSInteger>(webrtc::AudioProcessingMode::kSoftware),
              "RTCAudioProcessingMode out of sync");

static_assert(static_cast<NSInteger>(RTC_OBJC_TYPE(RTCPlatformAudioProcessingTopologyIndependent)) ==
                  static_cast<NSInteger>(webrtc::AudioDeviceModule::PlatformAudioProcessingTopology::kIndependent),
              "RTCPlatformAudioProcessingTopology out of sync");
static_assert(
    static_cast<NSInteger>(RTC_OBJC_TYPE(RTCPlatformAudioProcessingTopologyEchoCancellationAndNoiseSuppressionCoupled)) ==
        static_cast<NSInteger>(
            webrtc::AudioDeviceModule::PlatformAudioProcessingTopology::kEchoCancellationAndNoiseSuppressionCoupled),
    "RTCPlatformAudioProcessingTopology out of sync");

static_assert(static_cast<NSInteger>(RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeApplied)) ==
                  static_cast<NSInteger>(webrtc::AudioProcessingOptionsResultCode::kApplied),
              "RTCAudioProcessingOptionsResultCode out of sync");
static_assert(static_cast<NSInteger>(RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeStored)) ==
                  static_cast<NSInteger>(webrtc::AudioProcessingOptionsResultCode::kStored),
              "RTCAudioProcessingOptionsResultCode out of sync");
static_assert(static_cast<NSInteger>(RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeRejectedRemoteTrack)) ==
                  static_cast<NSInteger>(webrtc::AudioProcessingOptionsResultCode::kRejectedRemoteTrack),
              "RTCAudioProcessingOptionsResultCode out of sync");
static_assert(static_cast<NSInteger>(RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeRejectedInvalidCombination)) ==
                  static_cast<NSInteger>(webrtc::AudioProcessingOptionsResultCode::kRejectedInvalidCombination),
              "RTCAudioProcessingOptionsResultCode out of sync");
static_assert(static_cast<NSInteger>(RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeRejectedPlatformUnavailable)) ==
                  static_cast<NSInteger>(webrtc::AudioProcessingOptionsResultCode::kRejectedPlatformUnavailable),
              "RTCAudioProcessingOptionsResultCode out of sync");
static_assert(static_cast<NSInteger>(RTC_OBJC_TYPE(RTCAudioProcessingOptionsResultCodeApplyFailed)) ==
                  static_cast<NSInteger>(webrtc::AudioProcessingOptionsResultCode::kApplyFailed),
              "RTCAudioProcessingOptionsResultCode out of sync");

namespace {

// Bindings collapse tri-state diagnostics: unknown reads as false. The
// lossless representation stays in C++.
BOOL CollapsedBool(std::optional<bool> value) {
  return value.value_or(false) ? YES : NO;
}

RTC_OBJC_TYPE(RTCAudioProcessingMode) ModeToObjC(webrtc::AudioProcessingMode mode) {
  return static_cast<RTC_OBJC_TYPE(RTCAudioProcessingMode)>(mode);
}

}  // namespace

@interface RTC_OBJC_TYPE (RTCAudioProcessingComponentState) ()
- (instancetype)initWithNativeState:(const webrtc::AudioProcessingComponentState &)state;
@end

@implementation RTC_OBJC_TYPE (RTCAudioProcessingComponentState)

@synthesize requested = _requested;
@synthesize softwareResolved = _softwareResolved;
@synthesize softwareActive = _softwareActive;
@synthesize platformAvailable = _platformAvailable;
@synthesize platformResolved = _platformResolved;
@synthesize platformActive = _platformActive;
@synthesize effective = _effective;

- (instancetype)initWithNativeState:(const webrtc::AudioProcessingComponentState &)state {
  self = [super init];
  if (self) {
    if (state.requested_enabled.has_value()) {
      _requested = [[RTC_OBJC_TYPE(RTCAudioProcessingComponentOptions) alloc]
          initWithEnabled:*state.requested_enabled
                     mode:ModeToObjC(state.requested_mode.value_or(webrtc::AudioProcessingMode::kAutomatic))];
    }
    _softwareResolved = CollapsedBool(state.software_resolved);
    _softwareActive = CollapsedBool(state.software_active);
    _platformAvailable = state.platform_available ? YES : NO;
    _platformResolved = CollapsedBool(state.platform_resolved);
    _platformActive = CollapsedBool(state.platform_active);
    _effective = static_cast<RTC_OBJC_TYPE(RTCAudioProcessingImplementation)>(state.effective);
  }
  return self;
}

- (NSString *)description {
  NSString *requested = self.requested == nil
      ? @"none"
      : [NSString stringWithFormat:@"{enabled:%d mode:%ld}", self.requested.isEnabled, (long)self.requested.mode];
  return [NSString stringWithFormat:@"<%@: requested:%@ software:{resolved:%d active:%d} "
                                    @"platform:{available:%d resolved:%d active:%d} effective:%ld>",
                                    NSStringFromClass([self class]), requested, self.isSoftwareResolved,
                                    self.isSoftwareActive, self.isPlatformAvailable, self.isPlatformResolved,
                                    self.isPlatformActive, (long)self.effective];
}

@end

@interface RTC_OBJC_TYPE (RTCAudioProcessingState) ()
- (instancetype)initWithNativeState:(const webrtc::AudioProcessingState &)state;
@end

@implementation RTC_OBJC_TYPE (RTCAudioProcessingState)

@synthesize hasAudioProcessingModule = _hasAudioProcessingModule;
@synthesize echoCancellation = _echoCancellation;
@synthesize noiseSuppression = _noiseSuppression;
@synthesize autoGainControl = _autoGainControl;
@synthesize highPassFilter = _highPassFilter;

- (instancetype)initWithNativeState:(const webrtc::AudioProcessingState &)state {
  self = [super init];
  if (self) {
    _hasAudioProcessingModule = state.has_audio_processing_module ? YES : NO;
    _echoCancellation =
        [[RTC_OBJC_TYPE(RTCAudioProcessingComponentState) alloc] initWithNativeState:state.echo_cancellation];
    _noiseSuppression =
        [[RTC_OBJC_TYPE(RTCAudioProcessingComponentState) alloc] initWithNativeState:state.noise_suppression];
    _autoGainControl =
        [[RTC_OBJC_TYPE(RTCAudioProcessingComponentState) alloc] initWithNativeState:state.auto_gain_control];
    _highPassFilter =
        [[RTC_OBJC_TYPE(RTCAudioProcessingComponentState) alloc] initWithNativeState:state.high_pass_filter];
  }
  return self;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"<%@: hasAPM:%d\n ec:%@\n ns:%@\n agc:%@\n hpf:%@>",
                                    NSStringFromClass([self class]), self.hasAudioProcessingModule,
                                    self.echoCancellation, self.noiseSuppression, self.autoGainControl,
                                    self.highPassFilter];
}

@end

@interface RTC_OBJC_TYPE (RTCPlatformAudioProcessingComponentState) ()
- (instancetype)initWithAvailable:(bool)available
                        requested:(std::optional<bool>)requested
                           active:(std::optional<bool>)active;
@end

@implementation RTC_OBJC_TYPE (RTCPlatformAudioProcessingComponentState)

@synthesize available = _available;
@synthesize requested = _requested;
@synthesize active = _active;

- (instancetype)initWithAvailable:(bool)available
                        requested:(std::optional<bool>)requested
                           active:(std::optional<bool>)active {
  self = [super init];
  if (self) {
    _available = available ? YES : NO;
    _requested = CollapsedBool(requested);
    _active = CollapsedBool(active);
  }
  return self;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"<%@: available:%d requested:%d active:%d>",
                                    NSStringFromClass([self class]), self.isAvailable, self.isRequested,
                                    self.isActive];
}

@end

@interface RTC_OBJC_TYPE (RTCPlatformAudioProcessingState) ()
- (instancetype)initWithNativeState:(const webrtc::AudioDeviceModule::PlatformAudioProcessingState &)state;
@end

@implementation RTC_OBJC_TYPE (RTCPlatformAudioProcessingState)

@synthesize topology = _topology;
@synthesize echoCancellation = _echoCancellation;
@synthesize noiseSuppression = _noiseSuppression;
@synthesize autoGainControl = _autoGainControl;
@synthesize voiceProcessingEnabledRequested = _voiceProcessingEnabledRequested;
@synthesize voiceProcessingBypassedRequested = _voiceProcessingBypassedRequested;
@synthesize voiceProcessingAGCEnabledRequested = _voiceProcessingAGCEnabledRequested;
@synthesize voiceProcessingEnabledActive = _voiceProcessingEnabledActive;
@synthesize voiceProcessingBypassedActive = _voiceProcessingBypassedActive;
@synthesize voiceProcessingAGCEnabledActive = _voiceProcessingAGCEnabledActive;

- (instancetype)initWithNativeState:(const webrtc::AudioDeviceModule::PlatformAudioProcessingState &)state {
  self = [super init];
  if (self) {
    _topology = static_cast<RTC_OBJC_TYPE(RTCPlatformAudioProcessingTopology)>(state.topology);
    _echoCancellation = [[RTC_OBJC_TYPE(RTCPlatformAudioProcessingComponentState) alloc]
        initWithAvailable:state.is_echo_cancellation_available
                requested:state.is_echo_cancellation_requested
                   active:state.is_echo_cancellation_active];
    _noiseSuppression = [[RTC_OBJC_TYPE(RTCPlatformAudioProcessingComponentState) alloc]
        initWithAvailable:state.is_noise_suppression_available
                requested:state.is_noise_suppression_requested
                   active:state.is_noise_suppression_active];
    _autoGainControl = [[RTC_OBJC_TYPE(RTCPlatformAudioProcessingComponentState) alloc]
        initWithAvailable:state.is_auto_gain_control_available
                requested:state.is_auto_gain_control_requested
                   active:state.is_auto_gain_control_active];
    _voiceProcessingEnabledRequested = CollapsedBool(state.is_voice_processing_enabled_requested);
    _voiceProcessingBypassedRequested = CollapsedBool(state.is_voice_processing_bypassed_requested);
    _voiceProcessingAGCEnabledRequested = CollapsedBool(state.is_voice_processing_agc_enabled_requested);
    _voiceProcessingEnabledActive = CollapsedBool(state.is_voice_processing_enabled_active);
    _voiceProcessingBypassedActive = CollapsedBool(state.is_voice_processing_bypassed_active);
    _voiceProcessingAGCEnabledActive = CollapsedBool(state.is_voice_processing_agc_enabled_active);
  }
  return self;
}

- (NSString *)description {
  return [NSString
      stringWithFormat:@"<%@: topology:%ld ec:%@ ns:%@ agc:%@ vp:{enabled:%d/%d bypassed:%d/%d agc:%d/%d}>",
                       NSStringFromClass([self class]), (long)self.topology, self.echoCancellation,
                       self.noiseSuppression, self.autoGainControl, self.isVoiceProcessingEnabledRequested,
                       self.isVoiceProcessingEnabledActive, self.isVoiceProcessingBypassedRequested,
                       self.isVoiceProcessingBypassedActive, self.isVoiceProcessingAGCEnabledRequested,
                       self.isVoiceProcessingAGCEnabledActive];
}

@end

namespace webrtc {
namespace objc {

RTC_OBJC_TYPE(RTCAudioProcessingState) *
AudioProcessingStateToObjC(const AudioProcessingState &state) {
  return [[RTC_OBJC_TYPE(RTCAudioProcessingState) alloc] initWithNativeState:state];
}

RTC_OBJC_TYPE(RTCPlatformAudioProcessingState) *
PlatformAudioProcessingStateToObjC(const AudioDeviceModule::PlatformAudioProcessingState &state) {
  return [[RTC_OBJC_TYPE(RTCPlatformAudioProcessingState) alloc] initWithNativeState:state];
}

}  // namespace objc
}  // namespace webrtc

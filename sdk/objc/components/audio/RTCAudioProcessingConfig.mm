/*
 * Copyright 2023 LiveKit
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

#import "RTCAudioProcessingConfig.h"

#include "modules/audio_processing/include/audio_processing.h"

@implementation RTC_OBJC_TYPE (RTCAudioProcessingConfig) {
  webrtc::AudioProcessing::Config _config;
}

// config.echo_canceller.enabled

- (BOOL)echoCancellerEnabled {
  return _config.echo_canceller.enabled;
}

- (void)setEchoCancellerEnabled:(BOOL)value {
  _config.echo_canceller.enabled = value;
}

// config.echo_canceller.mobile_mode

- (BOOL)echoCancellerMobileMode {
  return _config.echo_canceller.mobile_mode;
}

- (void)setEchoCancellerMobileMode:(BOOL)value {
  _config.echo_canceller.mobile_mode = value;
}

#pragma mark - Private

- (webrtc::AudioProcessing::Config)nativeAudioProcessingConfig {
  return _config;
}

@end

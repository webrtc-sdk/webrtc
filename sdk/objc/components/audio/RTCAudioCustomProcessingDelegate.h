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

#import <Foundation/Foundation.h>

#import "RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

// // Interface for a custom processing submodule.
// class CustomProcessing {
//  public:
//   // (Re-)Initializes the submodule.
//   virtual void Initialize(int sample_rate_hz, int num_channels) = 0;
//   // Processes the given capture or render signal.
//   virtual void Process(AudioBuffer* audio) = 0;
//   // Returns a string representation of the module state.
//   virtual std::string ToString() const = 0;
//   // Handles RuntimeSettings. TODO(webrtc:9262): make pure virtual
//   // after updating dependencies.
//   virtual void SetRuntimeSetting(AudioProcessing::RuntimeSetting setting);
//   virtual ~CustomProcessing() {}
// };

@class RTC_OBJC_TYPE(RTCAudioBuffer);

RTC_OBJC_EXPORT @protocol RTC_OBJC_TYPE (RTCAudioCustomProcessingDelegate)<NSObject>
- (void)initializeWithSampleRateHz:(size_t)sampleRateHz
                          channels:(size_t)channels NS_SWIFT_NAME(initialize(sampleRateHz:channels:));
- (void)processAudioBuffer:(RTCAudioBuffer *)audioBuffer NS_SWIFT_NAME(process(audioBuffer:));
- (void)destroy;

@end

NS_ASSUME_NONNULL_END

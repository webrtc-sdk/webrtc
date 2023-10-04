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

#import "RTCAudioCustomProcessingAdapter.h"
#import "RTCAudioCustomProcessingDelegate.h"
#import "RTCMacros.h"

#include "modules/audio_processing/include/audio_processing.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTC_OBJC_TYPE(RTCAudioCustomProcessingAdapter) ()

// Thread safe set/get with os_unfair_lock.
@property(nonatomic, weak, nullable) id<RTCAudioCustomProcessingDelegate>
    audioCustomProcessingDelegate;

// Direct read access without lock.
@property(nonatomic, readonly, weak, nullable) id<RTCAudioCustomProcessingDelegate>
    rawAudioCustomProcessingDelegate;

@property(nonatomic, readonly) std::unique_ptr<webrtc::CustomProcessing>
    nativeAudioCustomProcessingModule;

- (instancetype)initWithDelegate:
    (nullable id<RTCAudioCustomProcessingDelegate>)audioCustomProcessingDelegate;

@end

NS_ASSUME_NONNULL_END

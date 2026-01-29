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

#import "RTCPushAudioSource.h"

#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTC_OBJC_TYPE (RTCPushAudioSource) ()

/**
 * The native PushAudioSource (as AudioSourceInterface) for creating audio tracks.
 */
@property(nonatomic, readonly)
    rtc::scoped_refptr<webrtc::AudioSourceInterface> nativeAudioSource;

@end

NS_ASSUME_NONNULL_END

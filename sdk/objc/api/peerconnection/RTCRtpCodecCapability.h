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

typedef NS_ENUM(NSInteger, RTCRtpMediaType);

NS_ASSUME_NONNULL_BEGIN

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCRtpCodecCapability) : NSObject

// Build MIME "type/subtype" string from `name` and `kind`.
@property(nonatomic, readonly) NSString *mimeType;

// Used to identify the codec. Equivalent to MIME subtype.
@property(nonatomic, copy) NSString *name;

// The media type of this codec. Equivalent to MIME top-level type.
@property(nonatomic, assign) RTCRtpMediaType kind;

// Clock rate in Hertz. If unset, the codec is applicable to any clock rate.
@property(nonatomic, copy, nullable) NSNumber *clockRate;

// Default payload type for this codec. Mainly needed for codecs that use
// that have statically assigned payload types.
@property(nonatomic, copy, nullable) NSNumber *preferredPayloadType;

// The number of audio channels supported. Unused for video codecs.
@property(nonatomic, copy, nullable) NSNumber *numChannels;

// Codec-specific parameters that must be signaled to the remote party.
//
// Corresponds to "a=fmtp" parameters in SDP.
//
// Contrary to ORTC, these parameters are named using all lowercase strings.
// This helps make the mapping to SDP simpler, if an application is using SDP.
// Boolean values are represented by the string "1".
// std::map<std::string, std::string> parameters;
@property(nonatomic, copy) NSDictionary<NSString *, NSString *> *parameters;

// Feedback mechanisms supported for this codec.
// std::vector<RtcpFeedback> rtcp_feedback;
// Not implemented.

@end

NS_ASSUME_NONNULL_END

/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
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
// absl::optional<int> clock_rate;

// Default payload type for this codec. Mainly needed for codecs that use
// that have statically assigned payload types.
// absl::optional<int> preferred_payload_type;

// The number of audio channels supported. Unused for video codecs.
// absl::optional<int> num_channels;

// Feedback mechanisms supported for this codec.
// std::vector<RtcpFeedback> rtcp_feedback;

// Codec-specific parameters that must be signaled to the remote party.
//
// Corresponds to "a=fmtp" parameters in SDP.
//
// Contrary to ORTC, these parameters are named using all lowercase strings.
// This helps make the mapping to SDP simpler, if an application is using SDP.
// Boolean values are represented by the string "1".
// std::map<std::string, std::string> parameters;

@end

NS_ASSUME_NONNULL_END

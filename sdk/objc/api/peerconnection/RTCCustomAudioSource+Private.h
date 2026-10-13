/*
 *  Copyright 2026 LiveKit
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCCustomAudioSource.h"

#include "api/scoped_refptr.h"
#include "pc/custom_audio_source.h"

NS_ASSUME_NONNULL_BEGIN

@class RTC_OBJC_TYPE(RTCPeerConnectionFactory);

// clang-format off
@interface RTC_OBJC_TYPE(RTCCustomAudioSource) ()

/** The native CustomAudioSource this object wraps. */
@property(nonatomic, readonly) webrtc::scoped_refptr<webrtc::CustomAudioSource> nativeCustomAudioSource;

/** Initialize with a native CustomAudioSource. */
- (instancetype)initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
        nativeCustomAudioSource:(webrtc::scoped_refptr<webrtc::CustomAudioSource>)nativeSource
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
              nativeAudioSource:(webrtc::scoped_refptr<webrtc::AudioSourceInterface>)nativeAudioSource
    NS_UNAVAILABLE;

@end
// clang-format on

NS_ASSUME_NONNULL_END

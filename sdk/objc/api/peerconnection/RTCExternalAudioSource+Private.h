/*
 *  Copyright 2026 LiveKit
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCExternalAudioSource.h"

#include "api/scoped_refptr.h"
#include "pc/external_audio_source.h"

NS_ASSUME_NONNULL_BEGIN

@class RTC_OBJC_TYPE(RTCPeerConnectionFactory);

@interface RTC_OBJC_TYPE (RTCExternalAudioSource)
()

    /** The native ExternalAudioSource this object wraps. */
    @property(nonatomic, readonly)
        webrtc::scoped_refptr<webrtc::ExternalAudioSource>
            nativeExternalAudioSource;

/** Initialize with a native ExternalAudioSource. */
- (instancetype)
        initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
    nativeExternalAudioSource:
        (webrtc::scoped_refptr<webrtc::ExternalAudioSource>)nativeSource
    NS_DESIGNATED_INITIALIZER;

- (instancetype)
      initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
    nativeAudioSource:
        (webrtc::scoped_refptr<webrtc::AudioSourceInterface>)nativeAudioSource
    NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

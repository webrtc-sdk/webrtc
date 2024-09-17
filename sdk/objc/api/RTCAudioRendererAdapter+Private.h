/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCAudioRendererAdapter.h"

#import "base/RTCAudioRenderer.h"

#include "api/media_stream_interface.h"

NS_ASSUME_NONNULL_BEGIN

@interface RTC_OBJC_TYPE(RTCAudioRendererAdapter) ()

@property(nonatomic, readonly) id<RTC_OBJC_TYPE(RTCAudioRenderer)> audioRenderer;

@property(nonatomic, readonly) webrtc::AudioTrackSinkInterface *nativeAudioRenderer;

- (instancetype)initWithNativeRenderer:(id<RTC_OBJC_TYPE(RTCAudioRenderer)>)audioRenderer
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

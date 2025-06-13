/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCTracing.h"

#include "rtc_base/event_tracer.h"

void RTC_OBJC_TYPE(RTCSetupInternalTracer)(void) {
  webrtc::tracing::SetupInternalTracer();
}

BOOL RTC_OBJC_TYPE(RTCStartInternalCapture)(NSString *filePath) {
  return webrtc::tracing::StartInternalCapture(filePath.UTF8String);
}

void RTC_OBJC_TYPE(RTCStopInternalCapture)(void) {
  webrtc::tracing::StopInternalCapture();
}

void RTC_OBJC_TYPE(RTCShutdownInternalTracer)(void) {
  webrtc::tracing::ShutdownInternalTracer();
}

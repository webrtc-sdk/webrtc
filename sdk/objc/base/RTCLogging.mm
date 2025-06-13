/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCLogging.h"

#include "rtc_base/logging.h"

webrtc::LoggingSeverity RTC_OBJC_TYPE(RTCGetNativeLoggingSeverity)(RTC_OBJC_TYPE(RTCLoggingSeverity) severity) {
  switch (severity) {
    case RTC_OBJC_TYPE(RTCLoggingSeverityVerbose):
      return webrtc::LS_VERBOSE;
    case RTC_OBJC_TYPE(RTCLoggingSeverityInfo):
      return webrtc::LS_INFO;
    case RTC_OBJC_TYPE(RTCLoggingSeverityWarning):
      return webrtc::LS_WARNING;
    case RTC_OBJC_TYPE(RTCLoggingSeverityError):
      return webrtc::LS_ERROR;
    case RTC_OBJC_TYPE(RTCLoggingSeverityNone):
      return webrtc::LS_NONE;
  }
}

void RTC_OBJC_TYPE(RTCLogEx)(RTC_OBJC_TYPE(RTCLoggingSeverity) severity, NSString* log_string) {
  if (log_string.length) {
    const char* utf8_string = log_string.UTF8String;
    RTC_LOG_V(RTC_OBJC_TYPE(RTCGetNativeLoggingSeverity)(severity)) << utf8_string;
  }
}

void RTC_OBJC_TYPE(RTCSetMinDebugLogLevel)(RTC_OBJC_TYPE(RTCLoggingSeverity) severity) {
  webrtc::LogMessage::LogToDebug(RTC_OBJC_TYPE(RTCGetNativeLoggingSeverity)(severity));
}

NSString* RTC_OBJC_TYPE(RTCFileName)(const char* file_path) {
  NSString* ns_file_path =
      [[NSString alloc] initWithBytesNoCopy:const_cast<char*>(file_path)
                                     length:strlen(file_path)
                                   encoding:NSUTF8StringEncoding
                               freeWhenDone:NO];
  return ns_file_path.lastPathComponent;
}

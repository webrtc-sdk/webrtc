/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCFieldTrials.h"

#import <os/lock.h>
#include <memory>
#import "base/RTCLogging.h"

#include "system_wrappers/include/field_trial.h"

NSString *const kRTCFieldTrialAudioForceABWENoTWCCKey = @"WebRTC-Audio-ABWENoTWCC";
NSString *const kRTCFieldTrialFlexFec03AdvertisedKey = @"WebRTC-FlexFEC-03-Advertised";
NSString *const kRTCFieldTrialFlexFec03Key = @"WebRTC-FlexFEC-03";
NSString *const kRTCFieldTrialH264HighProfileKey = @"WebRTC-H264HighProfile";
NSString *const kRTCFieldTrialMinimizeResamplingOnMobileKey =
    @"WebRTC-Audio-MinimizeResamplingOnMobile";
NSString *const kRTCFieldTrialUseNWPathMonitor = @"WebRTC-Network-UseNWPathMonitor";
NSString *const kRTCFieldTrialEnabledValue = @"Enabled";

void RTCInitFieldTrialDictionary(NSDictionary<NSString *, NSString *> *fieldTrials) {
  if (!fieldTrials) {
    RTCLogWarning(@"No fieldTrials provided.");
    return;
  }

  // Assemble the keys and values into the field trial string.
  NSMutableString *nsString = [NSMutableString string];
  for (NSString *key in fieldTrials) {
    NSString *fieldTrialEntry = [NSString stringWithFormat:@"%@/%@/", key, fieldTrials[key]];
    [nsString appendString:fieldTrialEntry];
  }

  size_t cStringLen = [nsString lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1;
  char *cString = new char[cStringLen];

  bool isSuccess = [nsString getCString:cString maxLength:cStringLen encoding:NSUTF8StringEncoding];
  if (!isSuccess) {
    RTCLogError(@"Failed to convert field trial string.");
    delete[] cString;
    return;
  }

  webrtc::field_trial::InitFieldTrialsFromString(cString);
  delete[] cString;
}

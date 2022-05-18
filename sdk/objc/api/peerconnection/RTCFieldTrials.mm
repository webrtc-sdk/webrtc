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

// InitFieldTrialsFromString stores the char*, so the char array must outlive
// the application.
static char *gFieldTrialInitString = nullptr;
static os_unfair_lock fieldTrialLock = OS_UNFAIR_LOCK_INIT;

void RTCInitFieldTrialDictionary(NSDictionary<NSString *, NSString *> *fieldTrials) {
  if (!fieldTrials) {
    RTCLogWarning(@"No fieldTrials provided.");
    return;
  }

  // Assemble the keys and values into the field trial string.
  NSMutableString *fieldTrialInitString = [NSMutableString string];
  for (NSString *key in fieldTrials) {
    NSString *fieldTrialEntry = [NSString stringWithFormat:@"%@/%@/", key, fieldTrials[key]];
    [fieldTrialInitString appendString:fieldTrialEntry];
  }

  size_t len = fieldTrialInitString.length + 1;

  // Locking before modifying global variable
  os_unfair_lock_lock(&fieldTrialLock);
  if (gFieldTrialInitString != nullptr) {
    delete[] gFieldTrialInitString;
    gFieldTrialInitString = nullptr;
  }

  gFieldTrialInitString = new char[len];
  bool success = [fieldTrialInitString getCString:gFieldTrialInitString
                                        maxLength:len
                                         encoding:NSUTF8StringEncoding];
  if (!success) {
    RTCLogError(@"Failed to convert field trial string.");
    os_unfair_lock_unlock(&fieldTrialLock);
    return;
  }

  webrtc::field_trial::InitFieldTrialsFromString(gFieldTrialInitString);
  os_unfair_lock_unlock(&fieldTrialLock);
}

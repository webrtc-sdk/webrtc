/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCDevice+Private.h"
#include "rtc_base/ref_counted_object.h"

NSString *const kDefaultDeviceId = @"default";

@implementation RTCDevice

@synthesize guid = _guid;
@synthesize name = _name;

+ (instancetype)defaultDevice {
  return [[self alloc] initWithGUID: kDefaultDeviceId
                               name: @""];
}

- (instancetype)initWithGUID:(NSString *)guid
                        name:(NSString* )name {
  if (self = [super init]) {
    _guid = guid;
    _name = name;
  }
  return self;
}


- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (object == nil) {
    return NO;
  }
  if (![object isMemberOfClass:[self class]]) {
    return NO;
  }

  return [_guid isEqualToString:((RTC_OBJC_TYPE(RTCDevice) *)object).guid];
}

- (NSUInteger)hash {
  return [_guid hash];
}

@end

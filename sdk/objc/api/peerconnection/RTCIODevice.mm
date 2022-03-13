/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCIODevice.h"
#import "RTCIODevice+Private.h"

NSString *const kDefaultDeviceId = @"default";

@implementation RTCIODevice

@synthesize type = _type;
@synthesize deviceId = _deviceId;
@synthesize name = _name;

+ (instancetype)defaultDeviceWithType: (RTCIODeviceType)type {
  return [[self alloc] initWithType: type 
                           deviceId: kDefaultDeviceId
                               name: @""];
}

- (instancetype)initWithType: (RTCIODeviceType)type
                    deviceId: (NSString *)deviceId
                        name: (NSString* )name {
  if (self = [super init]) {
    _type = type;
    _deviceId = deviceId;
    _name = name;
  }
  return self;
}

#pragma mark - IODevice

- (BOOL)isDefault {
  return [_deviceId isEqualToString: kDefaultDeviceId];
}

#pragma mark - Equatable

- (BOOL)isEqual: (id)object {
  if (self == object) {
    return YES;
  }
  if (object == nil) {
    return NO;
  }
  if (![object isMemberOfClass:[self class]]) {
    return NO;
  }

  return [_deviceId isEqualToString:((RTC_OBJC_TYPE(RTCIODevice) *)object).deviceId];
}

- (NSUInteger)hash {
  return [_deviceId hash];
}

@end

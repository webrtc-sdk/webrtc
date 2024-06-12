/*
 * Copyright 2022 LiveKit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#import "RTCIODevice.h"
#import "RTCIODevice+Private.h"

NSString *const kDefaultDeviceId = @"default";

@implementation RTC_OBJC_TYPE(RTCIODevice)

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

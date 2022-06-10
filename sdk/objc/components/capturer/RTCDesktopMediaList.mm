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
 
#import "RTCDesktopMediaList.h"

#import "RTCDesktopSource+Private.h"
#import "RTCDesktopMediaList+Private.h"

@implementation RTCDesktopMediaList {
     RTCDesktopSourceType _sourceType;
}

@synthesize sourceType = _sourceType;
@synthesize nativeMediaList = _nativeMediaList;

- (instancetype)initWithDelegate:(__weak id<RTC_OBJC_TYPE(RTCDesktopMediaListDelegate)>)delegate type:(RTCDesktopSourceType)type {
    if (self = [super init]) {
        webrtc::ObjCDesktopMediaList::DesktopType captureType = webrtc::ObjCDesktopMediaList::kScreen;
        if(type == RTCDesktopSourceTypeWindow) {
            captureType = webrtc::ObjCDesktopMediaList::kWindow;
        }
        _nativeMediaList = std::make_shared<webrtc::ObjCDesktopMediaList>(captureType, self);
    }
    return self;
}

- (void)UpdateSourceList {
    _nativeMediaList->UpdateSourceList();
}

-(NSArray<RTCDesktopSource *>*) getSources {
    NSMutableArray *sources = [NSMutableArray array];
    int sourceCount = _nativeMediaList->GetSourceCount();
    for (int i = 0; i < sourceCount; i++) {
        [sources addObject:[[RTCDesktopSource alloc] initWithNativeSource:&_nativeMediaList->GetSource(i) sourceType:_sourceType]];
    }
    return [NSArray arrayWithArray:sources];
}

@end
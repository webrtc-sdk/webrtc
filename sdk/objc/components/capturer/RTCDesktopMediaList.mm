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

@implementation RTC_OBJC_TYPE(RTCDesktopMediaList) {
     RTCDesktopSourceType _sourceType;
     NSMutableArray<RTC_OBJC_TYPE(RTCDesktopSource) *>* _sources;
     __weak id<RTC_OBJC_TYPE(RTCDesktopMediaListDelegate)> _delegate;
}

@synthesize sourceType = _sourceType;
@synthesize nativeMediaList = _nativeMediaList;

- (instancetype)initWithType:(RTCDesktopSourceType)type delegate:(__weak id<RTC_OBJC_TYPE(RTCDesktopMediaListDelegate)>)delegate{
    if (self = [super init]) {
        webrtc::DesktopType captureType = webrtc::kScreen;
        if(type == RTCDesktopSourceTypeWindow) {
            captureType = webrtc::kWindow;
        }
        _nativeMediaList = std::make_shared<webrtc::ObjCDesktopMediaList>(captureType, self);
        _sourceType = type;
        _delegate = delegate;
    }
    return self;
}

- (int32_t)UpdateSourceList:(BOOL)forceReload  updateAllThumbnails:(BOOL)updateThumbnail {
    return _nativeMediaList->UpdateSourceList(forceReload, updateThumbnail);
}

-(NSArray<RTC_OBJC_TYPE(RTCDesktopSource) *>*) getSources {
    _sources = [NSMutableArray array];
    int sourceCount = _nativeMediaList->GetSourceCount();
    for (int i = 0; i < sourceCount; i++) {
        webrtc::MediaSource *mediaSource = _nativeMediaList->GetSource(i);
        [_sources addObject:[[RTC_OBJC_TYPE(RTCDesktopSource) alloc] initWithNativeSource:mediaSource sourceType:_sourceType]];
    }
    return _sources;
}

-(void)mediaSourceAdded:(webrtc::MediaSource *) source {
    RTC_OBJC_TYPE(RTCDesktopSource) *desktopSource = [[RTC_OBJC_TYPE(RTCDesktopSource) alloc] initWithNativeSource:source sourceType:_sourceType];
    [_sources addObject:desktopSource];
    [_delegate didDesktopSourceAdded:desktopSource];
}

-(void)mediaSourceRemoved:(webrtc::MediaSource *) source {
    RTC_OBJC_TYPE(RTCDesktopSource) *desktopSource = [self getSourceById:source];
    if(desktopSource != nil) {
        [_sources removeObject:desktopSource];
        [_delegate didDesktopSourceRemoved:desktopSource];
    }
}

-(void)mediaSourceNameChanged:(webrtc::MediaSource *) source {
    RTC_OBJC_TYPE(RTCDesktopSource) *desktopSource = [self getSourceById:source];
    if(desktopSource != nil) {
        [desktopSource setName:source->name().c_str()];
        [_delegate didDesktopSourceNameChanged:desktopSource];
    }
}

-(void)mediaSourceThumbnailChanged:(webrtc::MediaSource *) source {
    RTC_OBJC_TYPE(RTCDesktopSource) *desktopSource = [self getSourceById:source];
    if(desktopSource != nil) {
        [desktopSource setThumbnail:source->thumbnail()];
        [_delegate didDesktopSourceThumbnailChanged:desktopSource];
    }
}

-(RTC_OBJC_TYPE(RTCDesktopSource) *)getSourceById:(webrtc::MediaSource *) source {
    NSEnumerator *enumerator = [_sources objectEnumerator];
    RTC_OBJC_TYPE(RTCDesktopSource) *object;
    while ((object = enumerator.nextObject) != nil) {
        if(object.nativeMediaSource == source) {
            return object;
        }
    }
    return nil;
}

@end
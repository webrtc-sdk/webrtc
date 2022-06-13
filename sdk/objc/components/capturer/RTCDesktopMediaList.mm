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
     NSMutableArray<RTCDesktopSource *>* _sources;
     __weak id<RTC_OBJC_TYPE(RTCDesktopMediaListDelegate)> _delegate;
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
        _delegate = delegate;
        _sourceType = type;
    }
    return self;
}

- (int32_t)UpdateSourceList {
    return _nativeMediaList->UpdateSourceList();
}

-(NSArray<RTCDesktopSource *>*) getSources {
    _sources = [NSMutableArray array];
    int sourceCount = _nativeMediaList->GetSourceCount();
    for (int i = 0; i < sourceCount; i++) {
        webrtc::ObjCDesktopMediaList::MediaSource *mediaSource = _nativeMediaList->GetSource(i);
        [_sources addObject:[[RTCDesktopSource alloc] initWithNativeSource:mediaSource sourceType:_sourceType]];
    }
    return _sources;
}

-(void)mediaSourceAdded:(int)index {
    if(_delegate) {
        //[_delegate mediaSourceAdded:index fromSource:_sources[index]];
    }
}

-(void)mediaSourceRemoved:(int)index {
    if(_delegate) {
        //[_delegate mediaSourceRemoved:index fromSource:_sources[index]];
    }
}

-(void)mediaSourceMoved:(int) oldIndex newIndex:(int) newIndex {
    if(_delegate) {
        //[_delegate mediaSourceMoved:oldIndex newIndex:newIndex fromSource:_sources[oldIndex]];
    }
}

-(void)mediaSourceNameChanged:(int)index {
    if(_delegate) {
        //[_delegate mediaSourceNameChanged:index fromSource:_sources[index]];
    }
}

-(void)mediaSourceThumbnailChanged:(int)index {
    if(_delegate) {
        //[_delegate mediaSourceThumbnailChanged:index fromSource:_sources[index]];
    }
}

@end
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
 
#import <Foundation/Foundation.h>

#import "RTCDesktopSource.h"
#import "RTCDesktopSource+Private.h"

@implementation RTC_OBJC_TYPE(RTCDesktopSource) {
    NSString *_sourceId;
    NSString *_name;
    NSImage *_thumbnail;
    RTCDesktopSourceType _sourceType;
}

@synthesize sourceId = _sourceId;
@synthesize name = _name;
@synthesize thumbnail = _thumbnail;
@synthesize sourceType = _sourceType;
@synthesize nativeMediaSource = _nativeMediaSource;

- (instancetype)initWithNativeSource:(webrtc::MediaSource*)nativeSource 
                          sourceType:(RTCDesktopSourceType) sourceType {
    if (self = [super init]) {
        _nativeMediaSource = nativeSource;
        _sourceId = [NSString stringWithUTF8String:std::to_string(nativeSource->id()).c_str()];
        _name = [NSString stringWithUTF8String:nativeSource->name().c_str()];
        _thumbnail = [self createThumbnailFromNativeSource:nativeSource->thumbnail()];
        _sourceType = sourceType;
    }
    return self;
}

-(NSImage*)createThumbnailFromNativeSource:(std::vector<unsigned char>)thumbnail {
    NSData* data = [[NSData alloc] initWithBytes:thumbnail.data() length:thumbnail.size()];
    NSImage *image = [[NSImage alloc] initWithData:data];
    return image;
}

-( NSImage *)UpdateThumbnail {
    if(_nativeMediaSource->UpdateThumbnail()) {
        _thumbnail = [self createThumbnailFromNativeSource:_nativeMediaSource->thumbnail()];
    }
    return _thumbnail;
}

-(void)setName:(const char *) name {
    _name = [NSString stringWithUTF8String:name];
}

-(void)setThumbnail:(std::vector<unsigned char>) thumbnail {
    _thumbnail = [self createThumbnailFromNativeSource:thumbnail];
}

@end

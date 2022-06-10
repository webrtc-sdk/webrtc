#import <Foundation/Foundation.h>

#import "RTCDesktopSource.h"
#import "RTCDesktopSource+Private.h"

@implementation RTCDesktopSource {
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

- (instancetype)initWithNativeSource:(const webrtc::ObjCDesktopMediaList::MediaSource*)nativeSource 
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

@end
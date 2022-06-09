#import <Foundation/Foundation.h>

#import "RTCDesktopCapturerSource.h"

@implementation RTCDesktopCapturerSource {
    NSString *_sourceId;
    NSString *_name;
    CGImage *_thumbnail;
    RTCDesktopCapturerSourceType _sourceType;
}

@synthesize sourceId = _sourceId;
@synthesize name = _name;
@synthesize thumbnail = _thumbnail;
@synthesize sourceType = _sourceType;

@end
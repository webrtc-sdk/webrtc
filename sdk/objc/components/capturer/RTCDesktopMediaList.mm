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
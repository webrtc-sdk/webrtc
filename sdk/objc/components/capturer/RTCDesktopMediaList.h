#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#import "RTCMacros.h"
#import "RTCDesktopSource.h"

NS_ASSUME_NONNULL_BEGIN

RTC_OBJC_EXPORT
@protocol RTC_OBJC_TYPE
(RTCDesktopMediaListDelegate) <NSObject>
-(void)mediaSourceAdded:(int)index;
-(void)mediaSourceRemoved:(int)index;
-(void)mediaSourceMoved:(int) oldIndex newIndex:(int) newIndex;
-(void)mediaSourceNameChanged:(int)index;
-(void)mediaSourceThumbnailChanged:(int)index;
@end

RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCDesktopMediaList) : NSObject

- (instancetype)initWithDelegate:(__weak id<RTC_OBJC_TYPE(RTCDesktopMediaListDelegate)>)delegate type:(RTCDesktopSourceType)type;

@property(nonatomic, readonly) RTCDesktopSourceType sourceType;

- (NSArray<RTCDesktopSource *>*) getSources;

- (void)UpdateSourceList;

@end

NS_ASSUME_NONNULL_END

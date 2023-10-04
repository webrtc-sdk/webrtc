/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMTLVideoView.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#import "base/RTCLogging.h"
#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"

#import "RTCMTLI420Renderer.h"
#import "RTCMTLNV12Renderer.h"
#import "RTCMTLRGBRenderer.h"

// To avoid unreconized symbol linker errors, we're taking advantage of the objc runtime.
// Linking errors occur when compiling for architectures that don't support Metal.
#define MTKViewClass NSClassFromString(@"MTKView")
#define RTCMTLNV12RendererClass NSClassFromString(@"RTCMTLNV12Renderer")
#define RTCMTLI420RendererClass NSClassFromString(@"RTCMTLI420Renderer")
#define RTCMTLRGBRendererClass NSClassFromString(@"RTCMTLRGBRenderer")

@interface RTC_OBJC_TYPE (RTCMTLVideoView) ()<MTKViewDelegate> 
@property(nonatomic) RTC_OBJC_TYPE(RTCMTLI420Renderer) *rendererI420;
@property(nonatomic) RTC_OBJC_TYPE(RTCMTLNV12Renderer) * rendererNV12;
@property(nonatomic) RTC_OBJC_TYPE(RTCMTLRGBRenderer) * rendererRGB;
@property(nonatomic) MTKView *metalView;
@property(atomic) RTC_OBJC_TYPE(RTCVideoFrame) * videoFrame;
@property(nonatomic) CGSize videoFrameSize;
@property(nonatomic) int64_t lastFrameTimeNs;
@end

@implementation RTC_OBJC_TYPE (RTCMTLVideoView)

@synthesize delegate = _delegate;
@synthesize rendererI420 = _rendererI420;
@synthesize rendererNV12 = _rendererNV12;
@synthesize rendererRGB = _rendererRGB;
@synthesize metalView = _metalView;
@synthesize videoFrame = _videoFrame;
@synthesize videoFrameSize = _videoFrameSize;
@synthesize lastFrameTimeNs = _lastFrameTimeNs;
@synthesize rotationOverride = _rotationOverride;

+ (BOOL)isMetalAvailable {
#if TARGET_OS_IPHONE
  return MTLCreateSystemDefaultDevice() != nil;
#elif TARGET_OS_OSX
  return [MTLCopyAllDevices() count] > 0;
#endif
}

- (instancetype)initWithFrame:(CGRect)frameRect {
  self = [super initWithFrame:frameRect];
  if (self) {
    [self configure];
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder *)aCoder {
  self = [super initWithCoder:aCoder];
  if (self) {
    [self configure];
  }
  return self;
}

- (BOOL)isEnabled {
  return !self.metalView.paused;
}

- (void)setEnabled:(BOOL)enabled {
  self.metalView.paused = !enabled;
}

#if TARGET_OS_IPHONE
- (UIViewContentMode)videoContentMode {
  return self.metalView.contentMode;
}

- (void)setVideoContentMode:(UIViewContentMode)mode {
  self.metalView.contentMode = mode;
}
#endif

#pragma mark - Private

+ (MTKView *)createMetalView:(CGRect)frame {
  return [[MTKViewClass alloc] initWithFrame:frame];
}

+ (RTC_OBJC_TYPE(RTCMTLNV12Renderer) *)createNV12Renderer {
  return [[RTCMTLNV12RendererClass alloc] init];
}

+ (RTC_OBJC_TYPE(RTCMTLI420Renderer) *)createI420Renderer {
  return [[RTCMTLI420RendererClass alloc] init];
}

+ (RTC_OBJC_TYPE(RTCMTLRGBRenderer) *)createRGBRenderer {
  return [[RTCMTLRGBRendererClass alloc] init];
}

- (void)configure {
  NSAssert([RTC_OBJC_TYPE(RTCMTLVideoView) isMetalAvailable],
           @"Metal not availiable on this device");

  self.metalView = [RTC_OBJC_TYPE(RTCMTLVideoView) createMetalView:self.bounds];
  self.metalView.delegate = self;
#if TARGET_OS_IPHONE
  self.metalView.contentMode = UIViewContentModeScaleAspectFill;
#elif TARGET_OS_OSX
  self.metalView.layerContentsPlacement = NSViewLayerContentsPlacementScaleProportionallyToFit;
#endif

  [self addSubview:self.metalView];
  self.videoFrameSize = CGSizeZero;
}

#if TARGET_OS_IPHONE
- (void)setMultipleTouchEnabled:(BOOL)multipleTouchEnabled {
  [super setMultipleTouchEnabled:multipleTouchEnabled];
  self.metalView.multipleTouchEnabled = multipleTouchEnabled;
}
#endif

- (void)performLayout {
  CGRect bounds = self.bounds;
  self.metalView.frame = bounds;
  if (!CGSizeEqualToSize(self.videoFrameSize, CGSizeZero)) {
    self.metalView.drawableSize = [self drawableSize];
  } else {
    self.metalView.drawableSize = bounds.size;
  }
}

#pragma mark - MTKViewDelegate methods

- (void)drawInMTKView:(nonnull MTKView *)view {
  NSAssert(view == self.metalView, @"Receiving draw callbacks from foreign instance.");
  RTC_OBJC_TYPE(RTCVideoFrame) *videoFrame = self.videoFrame;
  // Skip rendering if we've already rendered this frame.
  if (!videoFrame || videoFrame.width <= 0 || videoFrame.height <= 0 ||
      videoFrame.timeStampNs == self.lastFrameTimeNs) {
    return;
  }

  if (CGRectIsEmpty(view.bounds)) {
    return;
  }

  RTC_OBJC_TYPE(RTCMTLRenderer) * renderer;
  if ([videoFrame.buffer isKindOfClass:[RTC_OBJC_TYPE(RTCCVPixelBuffer) class]]) {
    RTC_OBJC_TYPE(RTCCVPixelBuffer) *buffer = (RTC_OBJC_TYPE(RTCCVPixelBuffer) *)videoFrame.buffer;
    const OSType pixelFormat = CVPixelBufferGetPixelFormatType(buffer.pixelBuffer);
    if (pixelFormat == kCVPixelFormatType_32BGRA || pixelFormat == kCVPixelFormatType_32ARGB) {
      if (!self.rendererRGB) {
        self.rendererRGB = [RTC_OBJC_TYPE(RTCMTLVideoView) createRGBRenderer];
        if (![self.rendererRGB addRenderingDestination:self.metalView]) {
          self.rendererRGB = nil;
          RTCLogError(@"Failed to create RGB renderer");
          return;
        }
      }
      renderer = self.rendererRGB;
    } else {
      if (!self.rendererNV12) {
        self.rendererNV12 = [RTC_OBJC_TYPE(RTCMTLVideoView) createNV12Renderer];
        if (![self.rendererNV12 addRenderingDestination:self.metalView]) {
          self.rendererNV12 = nil;
          RTCLogError(@"Failed to create NV12 renderer");
          return;
        }
      }
      renderer = self.rendererNV12;
    }
  } else {
    if (!self.rendererI420) {
      self.rendererI420 = [RTC_OBJC_TYPE(RTCMTLVideoView) createI420Renderer];
      if (![self.rendererI420 addRenderingDestination:self.metalView]) {
        self.rendererI420 = nil;
        RTCLogError(@"Failed to create I420 renderer");
        return;
      }
    }
    renderer = self.rendererI420;
  }

  renderer.rotationOverride = self.rotationOverride;

  [renderer drawFrame:videoFrame];
  self.lastFrameTimeNs = videoFrame.timeStampNs;
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
}

#pragma mark -

- (void)setRotationOverride:(NSValue *)rotationOverride {
  _rotationOverride = rotationOverride;

  self.metalView.drawableSize = [self drawableSize];
  [self setNeedsLayout];
}

- (RTCVideoRotation)videoRotation {
  if (self.rotationOverride) {
    RTCVideoRotation rotation;
    if (@available(iOS 11, macos 10.13, *)) {
      [self.rotationOverride getValue:&rotation size:sizeof(rotation)];
    } else {
      [self.rotationOverride getValue:&rotation];
    }
    return rotation;
  }

  return self.videoFrame.rotation;
}

- (CGSize)drawableSize {
  // Flip width/height if the rotations are not the same.
  CGSize videoFrameSize = self.videoFrameSize;
  RTCVideoRotation videoRotation = [self videoRotation];

  BOOL useLandscape =
      (videoRotation == RTCVideoRotation_0) || (videoRotation == RTCVideoRotation_180);
  BOOL sizeIsLandscape = (self.videoFrame.rotation == RTCVideoRotation_0) ||
      (self.videoFrame.rotation == RTCVideoRotation_180);

  if (useLandscape == sizeIsLandscape) {
    return videoFrameSize;
  } else {
    return CGSizeMake(videoFrameSize.height, videoFrameSize.width);
  }
}

#pragma mark - RTC_OBJC_TYPE(RTCVideoRenderer)

- (void)setSize:(CGSize)size {
  __weak RTC_OBJC_TYPE(RTCMTLVideoView) *weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    RTC_OBJC_TYPE(RTCMTLVideoView) *strongSelf = weakSelf;

    strongSelf.videoFrameSize = size;
    CGSize drawableSize = [strongSelf drawableSize];

    strongSelf.metalView.drawableSize = drawableSize;
    [strongSelf setNeedsLayout];
    [strongSelf.delegate videoView:self didChangeVideoSize:size];
  });
}

- (void)renderFrame:(nullable RTC_OBJC_TYPE(RTCVideoFrame) *)frame {
  if (!self.isEnabled) {
    return;
  }

  if (frame == nil) {
    RTCLogInfo(@"Incoming frame is nil. Exiting render callback.");
    return;
  }

  // Workaround to support RTCCVPixelBuffer rendering.
  // RTCMTLRGBRenderer seems to be broken at the moment.
  BOOL useI420 = NO;
  if ([frame.buffer isKindOfClass:[RTC_OBJC_TYPE(RTCCVPixelBuffer) class]]) {
    RTC_OBJC_TYPE(RTCCVPixelBuffer) *buffer = (RTC_OBJC_TYPE(RTCCVPixelBuffer) *)frame.buffer;
    const OSType pixelFormat = CVPixelBufferGetPixelFormatType(buffer.pixelBuffer);
    useI420 = pixelFormat == kCVPixelFormatType_32BGRA || pixelFormat == kCVPixelFormatType_32ARGB;
  }
  self.videoFrame = useI420 ? [frame newI420VideoFrame] : frame;
}

#pragma mark - Cross platform

#if TARGET_OS_IPHONE
- (void)layoutSubviews {
  [super layoutSubviews];
  [self performLayout];
}
#elif TARGET_OS_OSX
- (void)layout {
  [super layout];
  [self performLayout];
}

- (void)setNeedsLayout {
  self.needsLayout = YES;
}
#endif

@end

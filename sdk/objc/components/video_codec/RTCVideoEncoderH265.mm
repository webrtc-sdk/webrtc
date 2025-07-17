/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifdef RTC_ENABLE_H265

#import "RTCVideoEncoderH265.h"

#import <VideoToolbox/VideoToolbox.h>
#include <vector>

#if defined(WEBRTC_IOS)
#import "helpers/UIDevice+RTCDevice.h"
#endif
#import "api/peerconnection/RTCVideoCodecInfo+Private.h"
#import "base/RTCCodecSpecificInfo.h"
#import "base/RTCI420Buffer.h"
#import "base/RTCVideoEncoder.h"
#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"
#import "helpers.h"

#include "api/video_codecs/h265_profile_tier_level.h"
#include "common_video/include/bitrate_adjuster.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "sdk/objc/components/video_codec/nalu_rewriter.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"

@interface RTC_OBJC_TYPE (RTCVideoEncoderH265)
()

    - (void)frameWasEncoded : (OSStatus)status flags
    : (VTEncodeInfoFlags)infoFlags sampleBuffer
    : (CMSampleBufferRef)sampleBuffer codecSpecificInfo
    : (id<RTC_OBJC_TYPE(RTCCodecSpecificInfo)>)codecSpecificInfo width : (int32_t)width height
    : (int32_t)height renderTimeMs : (int64_t)renderTimeMs timestamp : (uint32_t)timestamp rotation
    : (RTC_OBJC_TYPE(RTCVideoRotation))rotation;

@end

namespace {  // anonymous namespace

// The ratio between kVTCompressionPropertyKey_DataRateLimits and
// kVTCompressionPropertyKey_AverageBitRate. The data rate limit is set higher
// than the average bit rate to avoid undershooting the target.
const float kLimitToAverageBitRateFactor = 10.0f;
// These thresholds are adapted for H.265 - typically higher quality than H.264
const int kLowH265QpThreshold = 26;
const int kHighH265QpThreshold = 36;
const int kBitsPerByte = 8;

const OSType kNV12PixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;

typedef NS_ENUM(NSInteger, RTC_OBJC_TYPE(RTCVideoEncodeMode)) {
  RTC_OBJC_TYPE(RTCVideoEncodeModeVariable) = 0,
  RTC_OBJC_TYPE(RTCVideoEncodeModeConstant) = 1,
};

NSArray *CreateRateLimitArray(uint32_t computedBitrateBps, RTC_OBJC_TYPE(RTCVideoEncodeMode) mode) {
  switch (mode) {
    case RTC_OBJC_TYPE(RTCVideoEncodeModeVariable): {
      // 5 seconds should be an okay interval for VBR to enforce the long-term
      // limit.
      float avgInterval = 5.0;
      uint32_t avgBytesPerSecond = computedBitrateBps / kBitsPerByte * avgInterval;
      // And the peak bitrate is measured per-second in a way similar to CBR.
      float peakInterval = 1.0;
      uint32_t peakBytesPerSecond =
          computedBitrateBps * kLimitToAverageBitRateFactor / kBitsPerByte;
      return @[ @(peakBytesPerSecond), @(peakInterval), @(avgBytesPerSecond), @(avgInterval) ];
    }
    case RTC_OBJC_TYPE(RTCVideoEncodeModeConstant): {
      // CBR should be enforces with granularity of a second.
      float targetInterval = 1.0;
      int32_t targetBitrate = computedBitrateBps / kBitsPerByte;
      return @[ @(targetBitrate), @(targetInterval) ];
    }
  }
}

// Struct that we pass to the encoder per frame to encode. We receive it again
// in the encoder callback.
struct RTCFrameEncodeParams {
  RTCFrameEncodeParams(RTC_OBJC_TYPE(RTCVideoEncoderH265) * e,
                       id<RTC_OBJC_TYPE(RTCCodecSpecificInfo)> csi,
                       int32_t w,
                       int32_t h,
                       int64_t rtms,
                       uint32_t ts,
                       RTC_OBJC_TYPE(RTCVideoRotation) r)
      : encoder(e), codecSpecificInfo(csi), width(w), height(h), render_time_ms(rtms), timestamp(ts), rotation(r) {}

  __weak RTC_OBJC_TYPE(RTCVideoEncoderH265) * encoder;
  id<RTC_OBJC_TYPE(RTCCodecSpecificInfo)> codecSpecificInfo;
  int32_t width;
  int32_t height;
  int64_t render_time_ms;
  uint32_t timestamp;
  RTC_OBJC_TYPE(RTCVideoRotation) rotation;
};

// We receive I420Frames as input, but we need to feed CVPixelBuffers into the
// encoder. This performs the copy and format conversion.
// TODO(tkchin): See if encoder will accept i420 frames and compare performance.
bool CopyVideoFrameToNV12PixelBuffer(id<RTC_OBJC_TYPE(RTCI420Buffer)> frameBuffer,
                                     CVPixelBufferRef pixelBuffer) {
  RTC_DCHECK(pixelBuffer);
  RTC_DCHECK_EQ(CVPixelBufferGetPixelFormatType(pixelBuffer), kNV12PixelFormat);
  RTC_DCHECK_EQ(CVPixelBufferGetHeightOfPlane(pixelBuffer, 0), frameBuffer.height);
  RTC_DCHECK_EQ(CVPixelBufferGetWidthOfPlane(pixelBuffer, 0), frameBuffer.width);

  CVReturn cvRet = CVPixelBufferLockBaseAddress(pixelBuffer, 0);
  if (cvRet != kCVReturnSuccess) {
    RTC_LOG(LS_ERROR) << "Failed to lock base address: " << cvRet;
    return false;
  }

  uint8_t* dstY = reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0));
  int dstStrideY = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
  uint8_t* dstUV = reinterpret_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1));
  int dstStrideUV = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);

  // Convert I420 to NV12.
  int ret = libyuv::I420ToNV12(frameBuffer.dataY, frameBuffer.strideY,
                               frameBuffer.dataU, frameBuffer.strideU,
                               frameBuffer.dataV, frameBuffer.strideV,
                               dstY, dstStrideY, dstUV, dstStrideUV,
                               frameBuffer.width, frameBuffer.height);
  CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
  if (ret) {
    RTC_LOG(LS_ERROR) << "Error converting I420 VideoFrame to NV12 :" << ret;
    return false;
  }
  return true;
}

CVPixelBufferRef CreatePixelBuffer(CVPixelBufferPoolRef pixel_buffer_pool,
                                   int32_t width,
                                   int32_t height) {
  if (!pixel_buffer_pool) {
    RTC_LOG(LS_ERROR) << "Failed to get pixel buffer pool.";
    return nullptr;
  }
  CVPixelBufferRef pixel_buffer;
  CVReturn ret = CVPixelBufferPoolCreatePixelBuffer(nullptr, pixel_buffer_pool, &pixel_buffer);
  if (ret != kCVReturnSuccess) {
    RTC_LOG(LS_ERROR) << "Failed to create pixel buffer: " << ret;
    // We probably want to drop frames here, since failure probably means
    // that the pool is empty.
    return nullptr;
  }
  return pixel_buffer;
}

// This is the callback function that VideoToolbox calls when encode is
// complete.
void compressionOutputCallback(void* encoder,
                               void* params,
                               OSStatus status,
                               VTEncodeInfoFlags infoFlags,
                               CMSampleBufferRef sampleBuffer) {
  std::unique_ptr<RTCFrameEncodeParams> encodeParams(
      reinterpret_cast<RTCFrameEncodeParams*>(params));
  [encodeParams->encoder frameWasEncoded:status
                                   flags:infoFlags
                            sampleBuffer:sampleBuffer
                       codecSpecificInfo:encodeParams->codecSpecificInfo
                                   width:encodeParams->width
                                  height:encodeParams->height
                            renderTimeMs:encodeParams->render_time_ms
                               timestamp:encodeParams->timestamp
                                rotation:encodeParams->rotation];
}

}  // namespace

@implementation RTC_OBJC_TYPE (RTCVideoEncoderH265) {
  RTC_OBJC_TYPE(RTCVideoCodecInfo) * _codecInfo;
  std::unique_ptr<webrtc::BitrateAdjuster> _bitrateAdjuster;
  uint32_t _targetBitrateBps;
  uint32_t _encoderBitrateBps;
  uint32_t _encoderFramerate;
  RTC_OBJC_TYPE(RTCVideoEncoderCallback) _callback;
  int32_t _width;
  int32_t _height;
  VTCompressionSessionRef _compressionSession;
  CVPixelBufferPoolRef _pixelBufferPool;
  RTCVideoEncoderQpThresholds* _qpThresholds;
}

// .m files support the new boxing syntax.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundeclared-selector"

- (instancetype)initWithCodecInfo:(RTC_OBJC_TYPE(RTCVideoCodecInfo)*)codecInfo {
  if ((self = [super init])) {
    _codecInfo = codecInfo;
    _bitrateAdjuster.reset(new webrtc::BitrateAdjuster(.5, .95));
    _encoderFramerate = 30;
    _compressionSession = nullptr;
    _pixelBufferPool = nullptr;
    RTC_LOG(LS_INFO) << "RTCVideoEncoderH265 initialized.";
  }
  return self;
}

- (void)dealloc {
  [self destroyCompressionSession];
  [self setPixelBufferPool:nullptr];
}

- (NSInteger)startEncodeWithSettings:(RTC_OBJC_TYPE(RTCVideoEncoderSettings)*)settings
                       numberOfCores:(int)numberOfCores {
  RTC_DCHECK(settings);
  RTC_DCHECK([settings isKindOfClass:[RTC_OBJC_TYPE(RTCVideoEncoderSettings) class]]);
  _width = settings.width;
  _height = settings.height;
  _targetBitrateBps = settings.startBitrate;
  _bitrateAdjuster->SetTargetBitrateBps(_targetBitrateBps);
  _qpThresholds = settings.qpMax > 0 ? [[RTCVideoEncoderQpThresholds alloc]
                                                   initWithThresholdsLow:kLowH265QpThreshold
                                                                    high:settings.qpMax]
                                              : [[RTCVideoEncoderQpThresholds alloc]
                                                    initWithThresholdsLow:kLowH265QpThreshold
                                                                     high:kHighH265QpThreshold];

  // We can only set average bitrate on the HW encoder.
  _encoderBitrateBps = _bitrateAdjuster->GetAdjustedBitrateBps();

  // TODO(tkchin): Try setting payload size via
  // kVTCompressionPropertyKey_MaxH264SliceBytes.

  return [self resetCompressionSession];
}

- (NSInteger)encode:(RTC_OBJC_TYPE(RTCVideoFrame)*)frame
    codecSpecificInfo:(nullable id<RTC_OBJC_TYPE(RTCCodecSpecificInfo)>)info
           frameTypes:(NSArray<NSNumber*>*)frameTypes {
  if (!_callback || !_compressionSession) {
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  BOOL isKeyframeRequired = NO;

  // Get a pixel buffer from the pool and copy frame data over.
  CVPixelBufferRef pixelBuffer = CreatePixelBuffer(_pixelBufferPool, _width, _height);
  if (!pixelBuffer) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  if (!CopyVideoFrameToNV12PixelBuffer([frame.buffer toI420], pixelBuffer)) {
    CFRelease(pixelBuffer);
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  // Check if we need a keyframe.
  if (frameTypes) {
    for (NSNumber* frameType in frameTypes) {
      if ((RTCFrameType)frameType.intValue == RTCFrameTypeVideoFrameKey) {
        isKeyframeRequired = YES;
        break;
      }
    }
  }

  CMTime presentationTimeStamp = CMTimeMake(frame.timeStampNs / 1000, 1000000);
  CFDictionaryRef frameProperties = nullptr;
  if (isKeyframeRequired) {
    CFTypeRef keys[] = {kVTEncodeFrameOptionKey_ForceKeyFrame};
    CFTypeRef values[] = {kCFBooleanTrue};
    frameProperties = CFDictionaryCreate(nullptr, keys, values, 1, &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);
  }

  std::unique_ptr<RTCFrameEncodeParams> encodeParams;
  encodeParams.reset(new RTCFrameEncodeParams(self, info, _width, _height, frame.timeStampNs / webrtc::kNumNanosecsPerMillisec,
                                              frame.timeStamp, frame.rotation));

  // Update the bitrate if needed.
  [self setBitrateBps:_bitrateAdjuster->GetAdjustedBitrateBps() framerate:_encoderFramerate];

  OSStatus status = VTCompressionSessionEncodeFrame(_compressionSession, pixelBuffer,
                                                    presentationTimeStamp, kCMTimeInvalid,
                                                    frameProperties, encodeParams.release(), nullptr);
  if (frameProperties) {
    CFRelease(frameProperties);
  }
  CFRelease(pixelBuffer);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to encode frame with code: " << status;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)setCallback:(RTC_OBJC_TYPE(RTCVideoEncoderCallback))callback {
  _callback = callback;
}

- (int)setBitrate:(uint32_t)bitrateKbit framerate:(uint32_t)framerate {
  _targetBitrateBps = 1000 * bitrateKbit;
  _bitrateAdjuster->SetTargetBitrateBps(_targetBitrateBps);
  [self setBitrateBps:_bitrateAdjuster->GetAdjustedBitrateBps() framerate:framerate];
  return WEBRTC_VIDEO_CODEC_OK;
}

- (NSInteger)releaseEncoder {
  [self destroyCompressionSession];
  [self setPixelBufferPool:nullptr];
  return WEBRTC_VIDEO_CODEC_OK;
}

- (NSString *)implementationName {
  return @"VideoToolbox";
}

- (BOOL)applyAlignmentToAllSimulcastLayers {
  return NO;
}

- (BOOL)supportsNativeHandle {
  return YES;
}

- (NSInteger)resolutionAlignment {
  return 1;
}

- (nullable RTC_OBJC_TYPE(RTCVideoEncoderQpThresholds) *)scalingSettings {
  return _qpThresholds;
}

#pragma mark - Private

- (NSInteger)resetCompressionSession {
  [self destroyCompressionSession];
  [self setPixelBufferPool:nullptr];

  // Set source image buffer attributes. These attributes will be present on
  // buffers that we receive from the encoder's pixel buffer pool.
  const size_t attributesSize = 3;
  CFTypeRef keys[attributesSize] = {
#if defined(WEBRTC_IOS)
    kCVPixelBufferOpenGLESCompatibilityKey,
#elif defined(WEBRTC_MAC)
    kCVPixelBufferOpenGLCompatibilityKey,
#endif
    kCVPixelBufferIOSurfacePropertiesKey,
    kCVPixelBufferPixelFormatTypeKey
  };
  CFDictionaryRef ioSurfaceValue = CFDictionaryCreate(
      nullptr, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  int64_t pixelFormatType = kNV12PixelFormat;
  CFNumberRef pixelFormat = CFNumberCreate(nullptr, kCFNumberLongType, &pixelFormatType);
  CFTypeRef values[attributesSize] = {kCFBooleanTrue, ioSurfaceValue, pixelFormat};
  CFDictionaryRef sourceAttributes = CFDictionaryCreate(
      nullptr, keys, values, attributesSize, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFRelease(ioSurfaceValue);
  CFRelease(pixelFormat);

  CFDictionaryRef encoderSpecs = nullptr;
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
  // Currently hw accl is supported above 360p on mac, below 360p
  // the compression session will be created with hw accl disabled.
  encoderSpecs = CFDictionaryCreate(nullptr, nullptr, nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
                                    &kCFTypeDictionaryValueCallBacks);
#endif

  OSStatus status = VTCompressionSessionCreate(
      nullptr,  // use default allocator
      _width,
      _height,
      kCMVideoCodecType_HEVC,  // Use HEVC/H.265
      encoderSpecs,  // use hardware accelerated encoder if available
      sourceAttributes,
      nullptr,  // use default compressed data allocator
      compressionOutputCallback,
      (__bridge void*)self,
      &_compressionSession);
  if (encoderSpecs) {
    CFRelease(encoderSpecs);
  }
  CFRelease(sourceAttributes);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "Failed to create compression session: " << status;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
  CFBooleanRef hwaccl_enabled = nullptr;
  status = VTSessionCopyProperty(_compressionSession,
                                 kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder,
                                 kCFAllocatorDefault, &hwaccl_enabled);
  if (status == noErr && (CFBooleanGetValue(hwaccl_enabled))) {
    RTC_LOG(LS_INFO) << "Compression session created with hw accl enabled";
  } else {
    RTC_LOG(LS_INFO) << "Compression session created with hw accl disabled";
  }
  if (hwaccl_enabled) {
    CFRelease(hwaccl_enabled);
  }
#endif

  [self configureCompressionSession];

  // The pixel buffer pool is dependent on the compression session so if the
  // compression session is reset, the pixel buffer pool should be reset as
  // well.
  CVPixelBufferPoolRef pixelBufferPool =
      VTCompressionSessionGetPixelBufferPool(_compressionSession);
  [self setPixelBufferPool:pixelBufferPool];

  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)configureCompressionSession {
  RTC_DCHECK(_compressionSession);
  SetVTSessionProperty(_compressionSession, kVTCompressionPropertyKey_RealTime, true);
  SetVTSessionProperty(_compressionSession, kVTCompressionPropertyKey_ProfileLevel,
                       kVTProfileLevel_HEVC_Main_AutoLevel);
  SetVTSessionProperty(_compressionSession, kVTCompressionPropertyKey_AllowFrameReordering, false);
  [self setBitrateBps:_encoderBitrateBps framerate:_encoderFramerate];
}

- (void)destroyCompressionSession {
  if (_compressionSession) {
    VTCompressionSessionInvalidate(_compressionSession);
    CFRelease(_compressionSession);
    _compressionSession = nullptr;
  }
}

- (void)setPixelBufferPool:(CVPixelBufferPoolRef)pixelBufferPool {
  if (_pixelBufferPool == pixelBufferPool) {
    return;
  }
  if (_pixelBufferPool) {
    CFRelease(_pixelBufferPool);
  }
  _pixelBufferPool = pixelBufferPool;
  if (_pixelBufferPool) {
    CFRetain(_pixelBufferPool);
  }
}

- (void)setBitrateBps:(uint32_t)bitrateBps framerate:(uint32_t)framerate {
  if (_encoderBitrateBps != bitrateBps || _encoderFramerate != framerate) {
    SetVTSessionProperty(_compressionSession, kVTCompressionPropertyKey_AverageBitRate, bitrateBps);
    SetVTSessionProperty(_compressionSession, kVTCompressionPropertyKey_ExpectedFrameRate, framerate);

    // TODO(tkchin): Add a helper method to set array value.
    NSArray* dataRateLimits = CreateRateLimitArray(bitrateBps, RTC_OBJC_TYPE(RTCVideoEncodeModeVariable));
    CFArrayRef cfArray = (__bridge CFArrayRef)dataRateLimits;
    SetVTSessionProperty(_compressionSession, kVTCompressionPropertyKey_DataRateLimits, cfArray);
    _encoderBitrateBps = bitrateBps;
    _encoderFramerate = framerate;
  }
}

- (void)frameWasEncoded:(OSStatus)status
                  flags:(VTEncodeInfoFlags)infoFlags
           sampleBuffer:(CMSampleBufferRef)sampleBuffer
      codecSpecificInfo:(id<RTC_OBJC_TYPE(RTCCodecSpecificInfo)>)codecSpecificInfo
                  width:(int32_t)width
                 height:(int32_t)height
           renderTimeMs:(int64_t)renderTimeMs
              timestamp:(uint32_t)timestamp
               rotation:(RTC_OBJC_TYPE(RTCVideoRotation))rotation {
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "H265 encode failed.";
    return;
  }
  if (infoFlags & kVTEncodeInfo_FrameDropped) {
    RTC_LOG(LS_INFO) << "H265 encode dropped frame.";
    return;
  }

  BOOL isKeyframe = NO;
  CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false);
  if (attachments != nullptr && CFArrayGetCount(attachments)) {
    CFDictionaryRef attachment =
        static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachments, 0));
    isKeyframe = !CFDictionaryContainsKey(attachment, kCMSampleAttachmentKey_NotSync);
  }

  if (isKeyframe) {
    RTC_LOG(LS_INFO) << "Generated keyframe";
  }

  __block std::unique_ptr<rtc::Buffer> buffer = std::make_unique<rtc::Buffer>();
  if (!webrtc::H265CMSampleBufferToAnnexBBuffer(sampleBuffer, isKeyframe, buffer.get())) {
    return;
  }
  RTC_OBJC_TYPE(RTCEncodedImage)* frame = [[RTC_OBJC_TYPE(RTCEncodedImage) alloc] init];
  frame.buffer = [NSData dataWithBytes:buffer->data() length:buffer->size()];
  frame.encodedWidth = width;
  frame.encodedHeight = height;
  frame.frameType = isKeyframe ? RTCFrameTypeVideoFrameKey : RTCFrameTypeVideoFrameDelta;
  frame.captureTimeMs = renderTimeMs;
  frame.timeStamp = timestamp;
  frame.rotation = rotation;
  frame.contentType = (rotation == RTC_OBJC_TYPE(RTCVideoRotation_0) ||
                       rotation == RTC_OBJC_TYPE(RTCVideoRotation_180))
                          ? RTCVideoContentTypeUnspecified
                          : RTCVideoContentTypeScreenshare;
  frame.flags = webrtc::VideoSendTiming::kInvalid;

  _bitrateAdjuster->Update(frame.buffer.length);

  BOOL sendEncodedFrame = YES;
  
  // Check if we need to drop for quality reasons.
  if (_qpThresholds) {
    // TODO: Parse H.265 QP from the encoded frame similar to H.264
    // For now, we'll skip QP-based dropping for H.265
  }

  if (sendEncodedFrame) {
    _callback(frame, codecSpecificInfo);
  }
}

#pragma clang diagnostic pop

@end

#endif  // RTC_ENABLE_H265 
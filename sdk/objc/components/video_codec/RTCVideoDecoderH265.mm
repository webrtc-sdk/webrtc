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

#import "RTCVideoDecoderH265.h"

#import <VideoToolbox/VideoToolbox.h>

#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"
#import "helpers.h"
#import "helpers/scoped_cftyperef.h"



#include "modules/video_coding/include/video_error_codes.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "sdk/objc/components/video_codec/nalu_rewriter.h"

// Struct that we pass to the decoder per frame to decode. We receive it again
// in the decoder callback.
struct RTCFrameDecodeParams {
  RTCFrameDecodeParams(RTCVideoDecoderCallback cb, int64_t ts)
      : callback(cb), timestamp(ts) {}
  RTCVideoDecoderCallback callback;
  int64_t timestamp;
};

@interface RTC_OBJC_TYPE (RTCVideoDecoderH265)
() - (void)setError : (OSStatus)error;
@end

// This is the callback function that VideoToolbox calls when decode is
// complete.
void h265DecompressionOutputCallback(void *decoderRef,
                                     void *params,
                                     OSStatus status,
                                     VTDecodeInfoFlags infoFlags,
                                     CVImageBufferRef imageBuffer,
                                     CMTime timestamp,
                                     CMTime duration) {
  std::unique_ptr<RTCFrameDecodeParams> decodeParams(
      reinterpret_cast<RTCFrameDecodeParams *>(params));
  if (status != noErr) {
    RTC_OBJC_TYPE(RTCVideoDecoderH265) *decoder =
        (__bridge RTC_OBJC_TYPE(RTCVideoDecoderH265) *)decoderRef;
    [decoder setError:status];
    RTC_LOG(LS_ERROR) << "Failed to decode frame. Status: " << status;
    return;
  }
  // TODO(tkchin): Handle CVO properly.
  RTC_OBJC_TYPE(RTCCVPixelBuffer) *frameBuffer =
      [[RTC_OBJC_TYPE(RTCCVPixelBuffer) alloc] initWithPixelBuffer:imageBuffer];
  RTC_OBJC_TYPE(
      RTCVideoFrame) *decodedFrame = [[RTC_OBJC_TYPE(RTCVideoFrame) alloc]
      initWithBuffer:frameBuffer
            rotation:RTC_OBJC_TYPE(RTCVideoRotation_0)
         timeStampNs:CMTimeGetSeconds(timestamp) * webrtc::kNumNanosecsPerSec];
  decodedFrame.timeStamp = decodeParams->timestamp;
  decodeParams->callback(decodedFrame);
}

// Decoder.
@implementation RTC_OBJC_TYPE (RTCVideoDecoderH265) {
  CMVideoFormatDescriptionRef _videoFormat;
  CMMemoryPoolRef _memoryPool;
  VTDecompressionSessionRef _decompressionSession;
  RTCVideoDecoderCallback _callback;
  OSStatus _error;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _memoryPool = CMMemoryPoolCreate(nil);
  }
  return self;
}

- (void)dealloc {
  CMMemoryPoolInvalidate(_memoryPool);
  CFRelease(_memoryPool);
  [self destroyDecompressionSession];
  [self setVideoFormat:nullptr];
}

- (NSInteger)startDecodeWithNumberOfCores:(int)numberOfCores {
  return WEBRTC_VIDEO_CODEC_OK;
}

- (NSInteger)decode:(RTC_OBJC_TYPE(RTCEncodedImage) *)encodedImage
        missingFrames:(BOOL)missingFrames
    codecSpecificInfo:(nullable id<RTC_OBJC_TYPE(RTCCodecSpecificInfo)>)info
         renderTimeMs:(int64_t)renderTimeMs {
  RTC_DCHECK(encodedImage.buffer);

  if (_error != noErr) {
    RTC_LOG(LS_WARNING) << "H265Decoder: Last frame decode failed with error: " << _error;
    _error = noErr;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  RTC_LOG(LS_INFO) << "H265Decoder: Decoding frame - size: " << encodedImage.buffer.length 
                   << ", timestamp: " << encodedImage.timeStamp
                   << ", frame type: " << (encodedImage.frameType == RTCFrameTypeVideoFrameKey ? "key" : "delta");

  // Create format description from input buffer first (similar to H.264 decoder)
  webrtc::ScopedCFTypeRef<CMVideoFormatDescriptionRef> inputFormat =
      webrtc::ScopedCF(webrtc::CreateH265VideoFormatDescription(
          (uint8_t *)encodedImage.buffer.bytes, encodedImage.buffer.length));
  if (inputFormat) {
    RTC_LOG(LS_INFO) << "H265Decoder: Successfully created format description";
    // Check if the video format has changed, and reinitialize decoder if needed.
    if (!CMFormatDescriptionEqual(inputFormat.get(), _videoFormat)) {
      RTC_LOG(LS_INFO) << "H265Decoder: Video format changed, resetting decompression session";
      [self setVideoFormat:inputFormat.get()];
      int resetDecompressionSessionError = [self resetDecompressionSession];
      if (resetDecompressionSessionError != WEBRTC_VIDEO_CODEC_OK) {
        RTC_LOG(LS_ERROR) << "H265Decoder: Failed to reset decompression session: " << resetDecompressionSessionError;
        return resetDecompressionSessionError;
      }
    }
  } else {
    RTC_LOG(LS_WARNING) << "H265Decoder: Failed to create format description from input buffer";
  }
  if (!_videoFormat) {
    // We received a frame but we don't have format information so we can't
    // decode it.
    // This can happen after backgrounding. We need to wait for the next
    // vps/sps/pps before we can resume so we request a keyframe by returning an
    // error.
    RTC_LOG(LS_WARNING) << "H265Decoder: Missing video format. Frame with vps/sps/pps required.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  CMSampleBufferRef sampleBuffer = nullptr;
  if (!webrtc::H265AnnexBBufferToCMSampleBuffer((uint8_t *)encodedImage.buffer.bytes,
                                               encodedImage.buffer.length,
                                               _videoFormat, &sampleBuffer,
                                               _memoryPool)) {
    RTC_LOG(LS_ERROR) << "H265Decoder: Failed to convert AnnexB buffer to CMSampleBuffer";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  RTC_DCHECK(sampleBuffer);
  CFAutorelease(sampleBuffer);
  
  RTC_LOG(LS_INFO) << "H265Decoder: Successfully created CMSampleBuffer";
  
  if (!_decompressionSession) {
    RTC_LOG(LS_ERROR) << "H265Decoder: Decompression session is null";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }

  std::unique_ptr<RTCFrameDecodeParams> decodeParams;
  decodeParams.reset(new RTCFrameDecodeParams(_callback, encodedImage.timeStamp));

  // Pass the decode params to the decode callback in the form of a void*.
  OSStatus status = VTDecompressionSessionDecodeFrame(
      _decompressionSession, sampleBuffer, 0, decodeParams.release(), nullptr);
  if (status != noErr) {
    RTC_LOG(LS_ERROR) << "H265Decoder: VTDecompressionSessionDecodeFrame failed with status: " << status;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  
  RTC_LOG(LS_INFO) << "H265Decoder: Successfully submitted frame for decoding";
  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)setCallback:(RTCVideoDecoderCallback)callback {
  _callback = callback;
}

- (NSInteger)releaseDecoder {
  // Need to invalidate the session so that callbacks no longer occur and it
  // is safe to null the callback.
  [self destroyDecompressionSession];
  [self setVideoFormat:nullptr];
  _callback = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

- (NSString *)implementationName {
  return @"VideoToolbox";
}

#pragma mark - Private

- (int)resetDecompressionSession {
  [self destroyDecompressionSession];

  // Set keys for OpenGL and IOSurface compatibilty, which makes the encoder
  // create pixel buffers with GPU backed memory. The intent here is to pass
  // the pixel buffers directly so we avoid a texture upload later during
  // rendering. This currently is moot because we are converting back to an
  // I420 frame after decode, but eventually we will be able to plumb
  // CVPixelBuffers directly to the renderer.
  NSDictionary *attributes = @{
#if defined(WEBRTC_IOS) && (TARGET_OS_MACCATALYST || TARGET_OS_SIMULATOR)
    (NSString *)kCVPixelBufferMetalCompatibilityKey : @(YES),
#elif defined(WEBRTC_IOS) && !defined(TARGET_OS_VISION)
    (NSString *)kCVPixelBufferOpenGLESCompatibilityKey : @(YES),
#elif defined(WEBRTC_MAC) && !defined(WEBRTC_ARCH_ARM64)
    (NSString *)kCVPixelBufferOpenGLCompatibilityKey : @(YES),
#endif
#if !(TARGET_OS_SIMULATOR)
    (NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{},
#endif
    (NSString *)kCVPixelBufferPixelFormatTypeKey :
        @(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange),
  };

  VTDecompressionOutputCallbackRecord record = {
      h265DecompressionOutputCallback, (__bridge void *)self,
  };
  OSStatus status = VTDecompressionSessionCreate(
      nullptr, _videoFormat, nullptr, (__bridge CFDictionaryRef)attributes, &record, &_decompressionSession);
  if (status != noErr) {
    [self destroyDecompressionSession];
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  [self configureDecompressionSession];

  return WEBRTC_VIDEO_CODEC_OK;
}

- (void)configureDecompressionSession {
  RTC_DCHECK(_decompressionSession);
#if defined(WEBRTC_IOS)
  VTSessionSetProperty(_decompressionSession,
                       kVTDecompressionPropertyKey_RealTime, kCFBooleanTrue);
#endif
}

- (void)destroyDecompressionSession {
  if (_decompressionSession) {
#if defined(WEBRTC_IOS)
    VTDecompressionSessionWaitForAsynchronousFrames(_decompressionSession);
#endif
    VTDecompressionSessionInvalidate(_decompressionSession);
    CFRelease(_decompressionSession);
    _decompressionSession = nullptr;
  }
}

- (void)setVideoFormat:(CMVideoFormatDescriptionRef)videoFormat {
  if (_videoFormat == videoFormat) {
    return;
  }
  if (_videoFormat) {
    CFRelease(_videoFormat);
  }
  _videoFormat = videoFormat;
  if (_videoFormat) {
    CFRetain(_videoFormat);
  }
}

- (void)setError:(OSStatus)error {
  _error = error;
}

@end

#endif  // RTC_ENABLE_H265 
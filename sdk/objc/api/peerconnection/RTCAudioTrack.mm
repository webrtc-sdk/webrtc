/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <AVFoundation/AVFoundation.h>
#import <os/lock.h>

#import "RTCAudioTrack+Private.h"

#import "RTCAudioRenderer.h"
#import "RTCAudioSource+Private.h"
#import "RTCMediaStreamTrack+Private.h"
#import "RTCPeerConnectionFactory+Private.h"
#import "helpers/NSString+StdString.h"

#include "rtc_base/checks.h"

namespace webrtc {
/**
 * Captures audio data and converts to CMSampleBuffers
 */
class AudioSinkConverter : public rtc::RefCountInterface, public webrtc::AudioTrackSinkInterface {
 private:
  os_unfair_lock *lock_;
  __weak RTCAudioTrack *audio_track_;
  int64_t total_frames_ = 0;
  bool attached_ = false;

 public:
  AudioSinkConverter(RTCAudioTrack *audioTrack, os_unfair_lock *lock) {
    RTC_LOG(LS_INFO) << "RTCAudioTrack.AudioSinkConverter init";
    audio_track_ = audioTrack;
    lock_ = lock;
  }

  ~AudioSinkConverter() {
    //
    RTC_LOG(LS_INFO) << "RTCAudioTrack.AudioSinkConverter dealloc";
  }

  // Must be called while locked
  void TryAttach() {
    if (attached_) {
      // Already attached
      return;
    }
    RTC_LOG(LS_INFO) << "RTCAudioTrack attaching sink...";
    // Reset for creating CMSampleTimingInfo correctly
    audio_track_.nativeAudioTrack->AddSink(this);
    total_frames_ = 0;
    attached_ = true;
  }

  // Must be called while locked
  void TryDetach() {
    if (!attached_) {
      // Already detached
      return;
    }
    RTC_LOG(LS_INFO) << "RTCAudioTrack detaching sink...";
    audio_track_.nativeAudioTrack->RemoveSink(this);
    attached_ = false;
  }

  void OnData(const void *audio_data,
              int bits_per_sample,
              int sample_rate,
              size_t number_of_channels,
              size_t number_of_frames,
              absl::optional<int64_t> absolute_capture_timestamp_ms) override {
    RTC_LOG(LS_INFO) << "RTCAudioTrack.AudioSinkConverter OnData bits_per_sample: "
                     << bits_per_sample << " sample_rate: " << sample_rate
                     << " number_of_channels: " << number_of_channels
                     << " number_of_frames: " << number_of_frames
                     << " absolute_capture_timestamp_ms: "
                     << (absolute_capture_timestamp_ms ? absolute_capture_timestamp_ms.value() : 0);

    bool is_locked = os_unfair_lock_trylock(lock_);
    if (!is_locked) {
      RTC_LOG(LS_INFO) << "RTCAudioTrack.AudioSinkConverter OnData already locked, skipping...";
      return;
    }
    bool is_attached = attached_;
    os_unfair_lock_unlock(lock_);

    if (!is_attached) {
      RTC_LOG(LS_INFO) << "RTCAudioTrack.AudioSinkConverter OnData already detached, skipping...";
      return;
    }

    /*
     * Convert to CMSampleBuffer
     */

    if (!(number_of_channels == 1 || number_of_channels == 2)) {
      NSLog(@"RTCAudioTrack: Only mono or stereo is supported currently. numberOfChannels: %zu",
            number_of_channels);
      return;
    }

    OSStatus status;

    AudioChannelLayout acl;
    bzero(&acl, sizeof(acl));
    acl.mChannelLayoutTag =
        number_of_channels == 2 ? kAudioChannelLayoutTag_Stereo : kAudioChannelLayoutTag_Mono;

    AudioStreamBasicDescription sd;
    sd.mSampleRate = sample_rate;
    sd.mFormatID = kAudioFormatLinearPCM;
    sd.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    sd.mFramesPerPacket = 1;
    sd.mChannelsPerFrame = number_of_channels;
    sd.mBitsPerChannel = bits_per_sample; /* 16 */
    sd.mBytesPerFrame = sd.mChannelsPerFrame * (sd.mBitsPerChannel / 8);
    sd.mBytesPerPacket = sd.mBytesPerFrame;

    CMSampleTimingInfo timing = {
        CMTimeMake(1, sample_rate),
        CMTimeMake(total_frames_, sample_rate),
        kCMTimeInvalid,
    };

    total_frames_ += number_of_frames;  // update the total

    CMFormatDescriptionRef format = NULL;
    status = CMAudioFormatDescriptionCreate(
        kCFAllocatorDefault, &sd, sizeof(acl), &acl, 0, NULL, NULL, &format);

    if (status != 0) {
      NSLog(@"RTCAudioTrack: Failed to create audio format description");
      return;
    }

    CMSampleBufferRef buffer;
    status = CMSampleBufferCreate(kCFAllocatorDefault,
                                  NULL,
                                  false,
                                  NULL,
                                  NULL,
                                  format,
                                  (CMItemCount)number_of_frames,
                                  1,
                                  &timing,
                                  0,
                                  NULL,
                                  &buffer);
    if (status != 0) {
      NSLog(@"RTCAudioTrack: Failed to allocate sample buffer");
      return;
    }

    AudioBufferList bufferList;
    bufferList.mNumberBuffers = 1;
    bufferList.mBuffers[0].mNumberChannels = sd.mChannelsPerFrame;
    bufferList.mBuffers[0].mDataByteSize = (UInt32)(number_of_frames * sd.mBytesPerFrame);
    bufferList.mBuffers[0].mData = (void *)audio_data;
    status = CMSampleBufferSetDataBufferFromAudioBufferList(
        buffer, kCFAllocatorDefault, kCFAllocatorDefault, 0, &bufferList);
    if (status != 0) {
      NSLog(@"RTCAudioTrack: Failed to convert audio buffer list into sample buffer");
      return;
    }

    // Report back to RTCAudioTrack
    [audio_track_ didCaptureSampleBuffer:buffer];

    CFRelease(buffer);
  }
};
}  // namespace webrtc

@implementation RTC_OBJC_TYPE (RTCAudioTrack) {
  rtc::scoped_refptr<webrtc::AudioSinkConverter> _audioConverter;
  // Stores weak references to renderers
  NSHashTable *_renderers;
  os_unfair_lock _lock;
}

@synthesize source = _source;

- (instancetype)initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
                         source:(RTC_OBJC_TYPE(RTCAudioSource) *)source
                        trackId:(NSString *)trackId {
  RTC_DCHECK(factory);
  RTC_DCHECK(source);
  RTC_DCHECK(trackId.length);

  std::string nativeId = [NSString stdStringForString:trackId];
  rtc::scoped_refptr<webrtc::AudioTrackInterface> track =
      factory.nativeFactory->CreateAudioTrack(nativeId, source.nativeAudioSource.get());
  if (self = [self initWithFactory:factory nativeTrack:track type:RTCMediaStreamTrackTypeAudio]) {
    _source = source;
  }
  return self;
}

- (instancetype)initWithFactory:(RTC_OBJC_TYPE(RTCPeerConnectionFactory) *)factory
                    nativeTrack:(rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>)nativeTrack
                           type:(RTCMediaStreamTrackType)type {
  NSParameterAssert(factory);
  NSParameterAssert(nativeTrack);
  NSParameterAssert(type == RTCMediaStreamTrackTypeAudio);
  if (self = [super initWithFactory:factory nativeTrack:nativeTrack type:type]) {
    RTC_LOG(LS_INFO) << "RTCAudioTrack init";
    _lock = OS_UNFAIR_LOCK_INIT;
    _renderers = [NSHashTable weakObjectsHashTable];
    _audioConverter = new rtc::RefCountedObject<webrtc::AudioSinkConverter>(self, &_lock);
  }

  return self;
}

- (void)dealloc {
  os_unfair_lock_lock(&_lock);
  _audioConverter->TryDetach();
  os_unfair_lock_unlock(&_lock);

  RTC_LOG(LS_INFO) << "RTCAudioTrack dealloc";
}

- (RTC_OBJC_TYPE(RTCAudioSource) *)source {
  if (!_source) {
    rtc::scoped_refptr<webrtc::AudioSourceInterface> source(self.nativeAudioTrack->GetSource());
    if (source) {
      _source = [[RTC_OBJC_TYPE(RTCAudioSource) alloc] initWithFactory:self.factory
                                                     nativeAudioSource:source];
    }
  }
  return _source;
}

- (void)addRenderer:(id<RTC_OBJC_TYPE(RTCAudioRenderer)>)renderer {
  os_unfair_lock_lock(&_lock);
  [_renderers addObject:renderer];
  _audioConverter->TryAttach();
  os_unfair_lock_unlock(&_lock);
}

- (void)removeRenderer:(id<RTC_OBJC_TYPE(RTCAudioRenderer)>)renderer {
  os_unfair_lock_lock(&_lock);
  [_renderers removeObject:renderer];
  NSUInteger renderersCount = _renderers.allObjects.count;

  if (renderersCount == 0) {
    // Detach if no more renderers...
    _audioConverter->TryDetach();
  }
  os_unfair_lock_unlock(&_lock);
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::AudioTrackInterface>)nativeAudioTrack {
  return rtc::scoped_refptr<webrtc::AudioTrackInterface>(
      static_cast<webrtc::AudioTrackInterface *>(self.nativeTrack.get()));
}

- (void)didCaptureSampleBuffer:(CMSampleBufferRef)sampleBuffer {
  bool is_locked = os_unfair_lock_trylock(&_lock);
  if (!is_locked) {
    RTC_LOG(LS_INFO) << "RTCAudioTrack didCaptureSampleBuffer already locked, skipping...";
    return;
  }
  NSArray *renderers = [_renderers allObjects];
  os_unfair_lock_unlock(&_lock);

  for (id<RTCAudioRenderer> renderer in renderers) {
    [renderer renderSampleBuffer:sampleBuffer];
  }
}

@end

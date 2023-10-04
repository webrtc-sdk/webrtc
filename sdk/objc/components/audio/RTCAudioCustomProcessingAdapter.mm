/*
 * Copyright 2023 LiveKit
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
#import <os/lock.h>

#import "RTCAudioBuffer+Private.h"
#import "RTCAudioCustomProcessingAdapter+Private.h"
#import "RTCAudioCustomProcessingAdapter.h"

#include "rtc_base/logging.h"

namespace webrtc {

class AudioCustomProcessingAdapter : public webrtc::CustomProcessing {
 public:
  bool is_initialized_;
  int sample_rate_hz_;
  int num_channels_;

  AudioCustomProcessingAdapter(RTC_OBJC_TYPE(RTCAudioCustomProcessingAdapter) *adapter, os_unfair_lock *lock) {
    RTC_LOG(LS_INFO) << "RTCAudioCustomProcessingAdapter.AudioCustomProcessingAdapter init";

    adapter_ = adapter;
    lock_ = lock;
    is_initialized_ = false;
    sample_rate_hz_ = 0;
    num_channels_ = 0;
  }

  ~AudioCustomProcessingAdapter() {
    RTC_LOG(LS_INFO) << "RTCAudioCustomProcessingAdapter.AudioCustomProcessingAdapter dealloc";

    os_unfair_lock_lock(lock_);
    id<RTCAudioCustomProcessingDelegate> delegate = adapter_.rawAudioCustomProcessingDelegate;
    [delegate audioProcessingRelease];
    os_unfair_lock_unlock(lock_);
  }

  void Initialize(int sample_rate_hz, int num_channels) override {
    os_unfair_lock_lock(lock_);
    id<RTCAudioCustomProcessingDelegate> delegate = adapter_.rawAudioCustomProcessingDelegate;
    [delegate audioProcessingInitializeWithSampleRate:sample_rate_hz channels:num_channels];
    is_initialized_ = true;
    sample_rate_hz_ = sample_rate_hz;
    num_channels_ = num_channels;
    os_unfair_lock_unlock(lock_);
  }

  void Process(AudioBuffer *audio_buffer) override {
    bool is_locked = os_unfair_lock_trylock(lock_);
    if (!is_locked) {
      RTC_LOG(LS_INFO) << "RTCAudioCustomProcessingAdapter.AudioCustomProcessingAdapter Process "
                          "already locked, skipping...";

      return;
    }
    id<RTCAudioCustomProcessingDelegate> delegate = adapter_.rawAudioCustomProcessingDelegate;
    if (delegate != nil) {
      RTCAudioBuffer *audioBuffer = [[RTCAudioBuffer alloc] initWithNativeType:audio_buffer];
      [delegate audioProcessingProcess:audioBuffer];
    }
    os_unfair_lock_unlock(lock_);
  }

  std::string ToString() const override { return "AudioCustomProcessingAdapter"; }

 private:
  __weak RTC_OBJC_TYPE(RTCAudioCustomProcessingAdapter) *adapter_;
  os_unfair_lock *lock_;
};
}  // namespace webrtc

@implementation RTC_OBJC_TYPE(RTCAudioCustomProcessingAdapter) {
  webrtc::AudioCustomProcessingAdapter *_adapter;
  os_unfair_lock _lock;
}

@synthesize rawAudioCustomProcessingDelegate = _rawAudioCustomProcessingDelegate;

- (instancetype)initWithDelegate:
    (nullable id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)>)audioCustomProcessingDelegate {
  if (self = [super init]) {
    _lock = OS_UNFAIR_LOCK_INIT;
    _rawAudioCustomProcessingDelegate = audioCustomProcessingDelegate;
    _adapter = new webrtc::AudioCustomProcessingAdapter(self, &_lock);
    RTC_LOG(LS_INFO) << "RTCAudioCustomProcessingAdapter init";
  }

  return self;
}

- (void)dealloc {
  RTC_LOG(LS_INFO) << "RTCAudioCustomProcessingAdapter dealloc";
}

#pragma mark - Getter & Setter for audioCustomProcessingDelegate

- (nullable id<RTCAudioCustomProcessingDelegate>)audioCustomProcessingDelegate {
  os_unfair_lock_lock(&_lock);
  id<RTCAudioCustomProcessingDelegate> delegate = _rawAudioCustomProcessingDelegate;
  os_unfair_lock_unlock(&_lock);
  return delegate;
}

- (void)setAudioCustomProcessingDelegate:(nullable id<RTCAudioCustomProcessingDelegate>)delegate {
  os_unfair_lock_lock(&_lock);
  if (_rawAudioCustomProcessingDelegate != nil && _adapter->is_initialized_) {
    [_rawAudioCustomProcessingDelegate audioProcessingRelease];
  }
  _rawAudioCustomProcessingDelegate = delegate;
  if (_adapter->is_initialized_) {
    [_rawAudioCustomProcessingDelegate
        audioProcessingInitializeWithSampleRate:_adapter->sample_rate_hz_
                                       channels:_adapter->num_channels_];
  }
  os_unfair_lock_unlock(&_lock);
}

#pragma mark - Private

- (std::unique_ptr<webrtc::CustomProcessing>)nativeAudioCustomProcessingModule {
  return std::unique_ptr<webrtc::CustomProcessing>(_adapter);
}

@end

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

namespace webrtc {

class AudioCustomProcessingAdapter : public webrtc::CustomProcessing {
 public:
  AudioCustomProcessingAdapter(RTCAudioCustomProcessingAdapter *adapter, os_unfair_lock *lock) {
    adapter_ = adapter;
    lock_ = lock;
  }
  ~AudioCustomProcessingAdapter() {
    os_unfair_lock_lock(lock_);
    id<RTCAudioCustomProcessingDelegate> delegate = adapter_.rawAudioCustomProcessingDelegate;
    [delegate destroy];
    os_unfair_lock_unlock(lock_);
  }

  void Initialize(int sample_rate_hz, int num_channels) override {
    os_unfair_lock_lock(lock_);
    id<RTCAudioCustomProcessingDelegate> delegate = adapter_.rawAudioCustomProcessingDelegate;
    [delegate initializeWithSampleRateHz:sample_rate_hz numChannels:num_channels];
    os_unfair_lock_unlock(lock_);
  }

  void Process(AudioBuffer *audio_buffer) override {
    os_unfair_lock_lock(lock_);
    id<RTCAudioCustomProcessingDelegate> delegate = adapter_.rawAudioCustomProcessingDelegate;
    if (delegate != nil) {
      RTCAudioBuffer *audioBuffer = [[RTCAudioBuffer alloc] initWithNativeType:audio_buffer];
      [delegate processAudioBuffer:audioBuffer];
    }
    os_unfair_lock_unlock(lock_);
  }

  std::string ToString() const override { return "AudioCustomProcessingAdapter"; }

 private:
  __weak RTCAudioCustomProcessingAdapter *adapter_;
  os_unfair_lock *lock_;
};
}  // namespace webrtc

@implementation RTCAudioCustomProcessingAdapter {
  id<RTCAudioCustomProcessingDelegate> _audioCustomProcessingDelegate;
  std::unique_ptr<webrtc::AudioCustomProcessingAdapter> _adapter;
  os_unfair_lock _lock;
}

- (instancetype)initWithDelegate:
    (nullable id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)>)audioCustomProcessingDelegate {
  if (self = [super init]) {
    _lock = OS_UNFAIR_LOCK_INIT;
    _audioCustomProcessingDelegate = audioCustomProcessingDelegate;
    _adapter = std::make_unique<webrtc::AudioCustomProcessingAdapter>(self, &_lock);
  }

  return self;
}

- (nullable id<RTCAudioCustomProcessingDelegate>)audioCustomProcessingDelegate {
  os_unfair_lock_lock(&_lock);
  id<RTCAudioCustomProcessingDelegate> delegate = _audioCustomProcessingDelegate;
  os_unfair_lock_unlock(&_lock);
  return delegate;
}

- (void)setAudioCustomProcessingDelegate:(nullable id<RTCAudioCustomProcessingDelegate>)delegate {
  os_unfair_lock_lock(&_lock);
  _audioCustomProcessingDelegate = delegate;
  os_unfair_lock_unlock(&_lock);
}

#pragma mark - Private

- (nullable id<RTCAudioCustomProcessingDelegate>)rawAudioCustomProcessingDelegate {
  return _audioCustomProcessingDelegate;
}

- (std::unique_ptr<webrtc::CustomProcessing>)nativeAudioCustomProcessingModule {
  return std::unique_ptr<webrtc::CustomProcessing>(_adapter.get());
}

@end

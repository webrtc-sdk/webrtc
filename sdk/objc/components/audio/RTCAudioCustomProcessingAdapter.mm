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

  std::string ToString() const override { return "AudioCustomProcessingAdapter"; }

  AudioCustomProcessingAdapter() {
    lock_ = OS_UNFAIR_LOCK_INIT;
    is_initialized_ = false;
    sample_rate_hz_ = 0;
    num_channels_ = 0;
  }

  ~AudioCustomProcessingAdapter() {
    os_unfair_lock_lock(&lock_);
    [delegate_ audioProcessingRelease];
    os_unfair_lock_unlock(&lock_);
  }

  void Initialize(int sample_rate_hz, int num_channels) override {
    os_unfair_lock_lock(&lock_);
    [delegate_ audioProcessingInitializeWithSampleRate:sample_rate_hz channels:num_channels];
    is_initialized_ = true;
    sample_rate_hz_ = sample_rate_hz;
    num_channels_ = num_channels;
    os_unfair_lock_unlock(&lock_);
  }

  void Process(AudioBuffer *audio_buffer) override {
    bool did_lock = os_unfair_lock_trylock(&lock_);
    if (!did_lock) {
      RTC_LOG(LS_INFO) << "RTCAudioCustomProcessingAdapter Process "
                          "already locked, skipping...";

      return;
    }

    if (delegate_ != nil) {
      RTC_OBJC_TYPE(RTCAudioBuffer) *audioBuffer =
          [[RTC_OBJC_TYPE(RTCAudioBuffer) alloc] initWithNativeType:audio_buffer];
      [delegate_ audioProcessingProcess:audioBuffer];
    }
    os_unfair_lock_unlock(&lock_);
  }

  id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)> GetDelegate() {
    __weak id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)> delegate = nil;
    os_unfair_lock_lock(&lock_);
    delegate = delegate_;
    os_unfair_lock_unlock(&lock_);
    return delegate;
  }

  void SetDelegate(__weak id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)> delegate) {
    RTC_LOG(LS_INFO) << "RTCAudioCustomProcessingAdapter SetDelegate: "
                     << (delegate != nullptr ? "YES" : "NO");

    os_unfair_lock_lock(&lock_);
    // Release previous.
    if (delegate_ != nil && is_initialized_) {
      [delegate_ audioProcessingRelease];
    }

    delegate_ = delegate;

    if (is_initialized_) {
      [delegate_ audioProcessingInitializeWithSampleRate:sample_rate_hz_ channels:num_channels_];
    }
    os_unfair_lock_unlock(&lock_);
  }

 private:
  __weak id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)> delegate_;
  os_unfair_lock lock_;
};
}  // namespace webrtc

@implementation RTC_OBJC_TYPE (RTCAudioCustomProcessingAdapter) {
  webrtc::AudioCustomProcessingAdapter *_adapter;
}

- (instancetype)initWithDelegate:
    (nullable id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)>)audioCustomProcessingDelegate {
  self = [super init];
  if (self) {
    _adapter = new webrtc::AudioCustomProcessingAdapter();
    RTC_LOG(LS_INFO) << "RTCAudioCustomProcessingAdapter init";
  }

  return self;
}

#pragma mark - Getter & Setter for audioCustomProcessingDelegate

- (nullable id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)>)audioCustomProcessingDelegate {
  return _adapter->GetDelegate();
}

- (void)setAudioCustomProcessingDelegate:
    (nullable id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)>)delegate {
  _adapter->SetDelegate(delegate);
}

#pragma mark - Private

- (webrtc::CustomProcessing *)nativeAudioCustomProcessingModule {
  return _adapter;
}

@end

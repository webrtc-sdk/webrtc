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

#import "RTCDefaultAudioProcessingModule.h"
#import "RTCAudioCustomProcessingAdapter+Private.h"
#import "RTCAudioProcessingConfig+Private.h"

#include "modules/audio_processing/include/audio_processing.h"

@implementation RTC_OBJC_TYPE (RTCDefaultAudioProcessingModule) {
  rtc::scoped_refptr<webrtc::AudioProcessing> _nativeAudioProcessingModule;
  // Custom processing adapters...
  RTCAudioCustomProcessingAdapter *_capturePostProcessingAdapter;
  RTCAudioCustomProcessingAdapter *_renderPreProcessingAdapter;
}

- (instancetype)init {
  return [self initWithConfig:nil
      capturePostProcessingDelegate:nil
        renderPreProcessingDelegate:nil];
}

- (instancetype)initWithConfig:(nullable RTCAudioProcessingConfig *)config
    capturePostProcessingDelegate:
        (nullable id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)>)capturePostProcessingDelegate
      renderPreProcessingDelegate:(nullable id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)>)
                                      renderPreProcessingDelegate {
  if (self = [super init]) {
    webrtc::AudioProcessingBuilder builder = webrtc::AudioProcessingBuilder();

    // TODO: Custom Config...

    if (config != nil) {
      builder.SetConfig(config.nativeAudioProcessingConfig);
    }

    _capturePostProcessingAdapter =
        [[RTCAudioCustomProcessingAdapter alloc] initWithDelegate:capturePostProcessingDelegate];
    builder.SetCapturePostProcessing(
        _capturePostProcessingAdapter.nativeAudioCustomProcessingModule);

    _renderPreProcessingAdapter =
        [[RTCAudioCustomProcessingAdapter alloc] initWithDelegate:renderPreProcessingDelegate];
    builder.SetRenderPreProcessing(_renderPreProcessingAdapter.nativeAudioCustomProcessingModule);

    _nativeAudioProcessingModule = builder.Create();
  }
  return self;
}

#pragma mark - Getter & Setters for delegates

- (nullable id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)>)capturePostProcessingDelegate {
  return _capturePostProcessingAdapter.audioCustomProcessingDelegate;
}

- (void)setCapturePostProcessingDelegate:
    (nullable id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)>)delegate {
  _capturePostProcessingAdapter.audioCustomProcessingDelegate = delegate;
}

- (nullable id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)>)renderPreProcessingDelegate {
  return _renderPreProcessingAdapter.audioCustomProcessingDelegate;
}

- (void)setRenderPreProcessingDelegate:
    (nullable id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)>)delegate {
  _renderPreProcessingAdapter.audioCustomProcessingDelegate = delegate;
}

#pragma mark - RTCAudioProcessingModule protocol

- (void)applyConfig:(RTCAudioProcessingConfig *)config {
  _nativeAudioProcessingModule->ApplyConfig(config.nativeAudioProcessingConfig);
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::AudioProcessing>)nativeAudioProcessingModule {
  return _nativeAudioProcessingModule;
}

@end

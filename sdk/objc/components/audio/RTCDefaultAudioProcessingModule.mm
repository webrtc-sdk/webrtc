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

#include "modules/audio_processing/include/audio_processing.h"

@implementation RTC_OBJC_TYPE (RTCDefaultAudioProcessingModule) {
  rtc::scoped_refptr<webrtc::AudioProcessing> _nativeAudioProcessingModule;
  // Custom processing adapters...
  RTCAudioCustomProcessingAdapter *_capturePostProcessingAdapter;
  RTCAudioCustomProcessingAdapter *_renderPreProcessingAdapter;
}

- (instancetype)init {
  if (self = [super init]) {
    _nativeAudioProcessingModule = webrtc::AudioProcessingBuilder().Create();
  }
  return self;
}

- (instancetype)
    initWithCapturePostProcessing:
        (nullable id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)>)capturePostProcessingDelegate
              renderPreProcessing:(nullable id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)>)
                                      renderPreProcessingDelegate {
  if (self = [super init]) {
    webrtc::AudioProcessingBuilder builder = webrtc::AudioProcessingBuilder();

    // TODO: Custom Config...

    if (capturePostProcessingDelegate != nil) {
      _capturePostProcessingAdapter =
          [[RTCAudioCustomProcessingAdapter alloc] initWithDelegate:capturePostProcessingDelegate];
      builder.SetCapturePostProcessing(
          _capturePostProcessingAdapter.nativeAudioCustomProcessingModule);
    }

    if (renderPreProcessingDelegate != nil) {
      _renderPreProcessingAdapter =
          [[RTCAudioCustomProcessingAdapter alloc] initWithDelegate:renderPreProcessingDelegate];
      builder.SetRenderPreProcessing(_renderPreProcessingAdapter.nativeAudioCustomProcessingModule);
    }

    _nativeAudioProcessingModule = builder.Create();
  }
  return self;
}

#pragma mark - Private

- (rtc::scoped_refptr<webrtc::AudioProcessing>)nativeAudioProcessingModule {
  return _nativeAudioProcessingModule;
}

@end

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

#include "api/environment/environment_factory.h"
#include "api/scoped_refptr.h"
#include "api/audio/builtin_audio_processing_builder.h"
#include "modules/audio_processing/include/audio_processing.h"

@implementation RTC_OBJC_TYPE (RTCDefaultAudioProcessingModule) {
  webrtc::scoped_refptr<webrtc::AudioProcessing> _nativeAudioProcessingModule;
  // Custom processing adapters...
  RTC_OBJC_TYPE(RTCAudioCustomProcessingAdapter) * _capturePostProcessingAdapter;
  RTC_OBJC_TYPE(RTCAudioCustomProcessingAdapter) * _renderPreProcessingAdapter;
}

- (instancetype)init {
  return [self initWithConfig:nil
      capturePostProcessingDelegate:nil
        renderPreProcessingDelegate:nil];
}

- (instancetype)initWithConfig:(nullable RTC_OBJC_TYPE(RTCAudioProcessingConfig) *)config
    capturePostProcessingDelegate:
        (nullable id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)>)capturePostProcessingDelegate
      renderPreProcessingDelegate:(nullable id<RTC_OBJC_TYPE(RTCAudioCustomProcessingDelegate)>)
                                      renderPreProcessingDelegate {
  self = [super init];
  if (self) {
    webrtc::BuiltinAudioProcessingBuilder builder = webrtc::BuiltinAudioProcessingBuilder();

    // TODO: Custom Config...

    if (config != nil) {
      builder.SetConfig(config.nativeAudioProcessingConfig);
    }

    _capturePostProcessingAdapter = [[RTC_OBJC_TYPE(RTCAudioCustomProcessingAdapter) alloc]
        initWithDelegate:capturePostProcessingDelegate];
    builder.SetCapturePostProcessing(std::unique_ptr<webrtc::CustomProcessing>(
        _capturePostProcessingAdapter.nativeAudioCustomProcessingModule));

    _renderPreProcessingAdapter = [[RTC_OBJC_TYPE(RTCAudioCustomProcessingAdapter) alloc]
        initWithDelegate:renderPreProcessingDelegate];
    builder.SetRenderPreProcessing(std::unique_ptr<webrtc::CustomProcessing>(
        _renderPreProcessingAdapter.nativeAudioCustomProcessingModule));

    _nativeAudioProcessingModule = builder.Build(webrtc::CreateEnvironment());
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

- (RTC_OBJC_TYPE(RTCAudioProcessingConfig) *)config {
  webrtc::AudioProcessing::Config nativeConfig = _nativeAudioProcessingModule->GetConfig();
  return [[RTC_OBJC_TYPE(RTCAudioProcessingConfig) alloc]
      initWithNativeAudioProcessingConfig:nativeConfig];
}

- (void)setConfig:(RTC_OBJC_TYPE(RTCAudioProcessingConfig) *)config {
  _nativeAudioProcessingModule->ApplyConfig(config.nativeAudioProcessingConfig);
}

- (BOOL)isMuted {
  return _nativeAudioProcessingModule->get_output_will_be_muted();
}

- (void)setMuted:(BOOL)isMuted {
  _nativeAudioProcessingModule->set_output_will_be_muted(isMuted);
}

#pragma mark - Private

- (webrtc::scoped_refptr<webrtc::AudioProcessing>)nativeAudioProcessingModule {
  return _nativeAudioProcessingModule;
}

@end

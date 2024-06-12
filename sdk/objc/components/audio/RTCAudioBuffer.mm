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

#import "RTCAudioBuffer.h"

#include "modules/audio_processing/audio_buffer.h"

@implementation RTC_OBJC_TYPE (RTCAudioBuffer) {
  // Raw
  webrtc::AudioBuffer *_audioBuffer;
}

- (size_t)channels {
  return _audioBuffer->num_channels();
}

- (size_t)frames {
  return _audioBuffer->num_frames();
}

- (size_t)framesPerBand {
  return _audioBuffer->num_frames_per_band();
}

- (size_t)bands {
  return _audioBuffer->num_bands();
}

- (float *)rawBufferForChannel:(size_t)channel {
  return _audioBuffer->channels()[channel];
}

#pragma mark - Private

- (instancetype)initWithNativeType:(webrtc::AudioBuffer *)audioBuffer {
  if (self = [super init]) {
    _audioBuffer = audioBuffer;
  }
  return self;
}

@end

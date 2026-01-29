/*
 * Copyright 2026 LiveKit
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

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#import "sdk/objc/base/RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

/**
 * Audio source that allows pushing PCM data directly, bypassing the AudioDeviceModule.
 * Used for stereo app audio during screen sharing.
 *
 * Unlike RTCAudioSource which relies on the AudioDeviceModule for capture,
 * RTCPushAudioSource accepts PCM data directly via pushPCMBuffer:,
 * preserving the original channel count (stereo).
 */
RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCPushAudioSource) : NSObject

- (instancetype)init NS_UNAVAILABLE;

/**
 * Initialize a push audio source with specified sample rate and channel count.
 * @param sampleRate Sample rate in Hz (e.g., 48000)
 * @param channels Number of channels (1 for mono, 2 for stereo)
 */
- (instancetype)initWithSampleRate:(int)sampleRate
                          channels:(int)channels NS_DESIGNATED_INITIALIZER;

/** The sample rate this source was configured with. */
@property(nonatomic, readonly) int sampleRate;

/** The number of channels this source was configured with. */
@property(nonatomic, readonly) int channels;

/**
 * Push raw PCM audio data to all registered sinks.
 * @param data Pointer to interleaved PCM samples (16-bit signed integers)
 * @param bitsPerSample Bits per sample (typically 16)
 * @param sampleRate Sample rate in Hz
 * @param channels Number of channels
 * @param frames Number of frames (samples per channel)
 */
- (void)pushData:(const void *)data
   bitsPerSample:(int)bitsPerSample
      sampleRate:(int)sampleRate
        channels:(size_t)channels
          frames:(size_t)frames;

/**
 * Push an AVAudioPCMBuffer to all registered sinks.
 * Automatically converts float32 to int16 format.
 * @param buffer The audio buffer to push (float32 format expected)
 */
- (void)pushPCMBuffer:(AVAudioPCMBuffer *)buffer;

@end

NS_ASSUME_NONNULL_END

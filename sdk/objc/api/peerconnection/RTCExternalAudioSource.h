/*
 *  Copyright 2026 LiveKit
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <Foundation/Foundation.h>

#import "RTCAudioSource.h"
#import "sdk/objc/base/RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

/**
 * An audio source the application pushes PCM frames into, bypassing the
 * AudioDeviceModule. Use it with -[RTCPeerConnectionFactory
 * audioTrackWithSource:trackId:] to publish audio that is independent of the
 * microphone capture path, e.g. app or screen-share audio. Multiple instances
 * operate independently, alongside regular microphone tracks.
 *
 * Note: capture-side audio processing (AEC/NS/AGC) is intentionally bypassed
 * for this source. Push audio that requires no such processing.
 */
RTC_OBJC_EXPORT
@interface RTC_OBJC_TYPE (RTCExternalAudioSource) : RTC_OBJC_TYPE(RTCAudioSource)

- (instancetype)init NS_UNAVAILABLE;

/** The sample rate pushed frames must have, in Hz. */
@property(nonatomic, readonly) int sampleRate;

/** The number of interleaved channels pushed frames must have. */
@property(nonatomic, readonly) NSUInteger channels;

/**
 * Size of the internal buffer in milliseconds. 0 means synchronous mode:
 * frames are delivered on the calling thread and must be exactly 10 ms.
 * Greater than 0 means buffered mode: pushed audio is queued and delivered
 * in 10 ms frames by an internal pacing thread, with silence on underrun.
 */
@property(nonatomic, readonly) int queueSizeMs;

/** Currently buffered audio in milliseconds (buffered mode; 0 otherwise). */
@property(nonatomic, readonly) int64_t bufferedDurationMs;

/**
 * Pushes interleaved int16 PCM. sampleRate and channels must match the
 * source's declared format, no resampling is performed. frames is the number
 * of samples per channel (pcmData.length must equal
 * frames * channels * sizeof(int16_t)).
 *
 * Returns NO if the frame was rejected: wrong format, a synchronous-mode
 * frame that is not exactly 10 ms, a full buffer, or a previous completion
 * handler still pending. When this method returns NO the completion handler
 * is discarded and will never be invoked, so callers driving back-pressure
 * from the handler must check the return value and retry.
 *
 * completionHandler is the back-pressure signal in buffered mode: it is
 * invoked once the buffer has drained enough to push again, either inline
 * when there is already room, or later on an internal audio thread. Only
 * one completion handler may be pending at a time. It runs outside the
 * source's lock, so pushing the next frame from inside it is allowed. It
 * must not block.
 */
- (BOOL)captureFrameWithData:(NSData *)pcmData
                  sampleRate:(int)sampleRate
                    channels:(NSUInteger)channels
                      frames:(NSUInteger)frames
           completionHandler:(nullable void (^)(void))completionHandler;

/**
 * Convenience for AVAudioPCMBuffer input (int16 or float32, interleaved or
 * deinterleaved). Converted to interleaved int16 internally. The buffer's
 * sample rate and channel count must match the source's declared format.
 */
- (BOOL)captureAVAudioPCMBuffer:(AVAudioPCMBuffer *)pcmBuffer
              completionHandler:(nullable void (^)(void))completionHandler;

/**
 * Convenience for CMSampleBuffer input carrying linear PCM (int16 or
 * float32), e.g. ReplayKit app audio. The buffer's sample rate and channel
 * count must match the source's declared format.
 */
- (BOOL)captureSampleBuffer:(CMSampleBufferRef)sampleBuffer
          completionHandler:(nullable void (^)(void))completionHandler;

/** Drops all buffered audio. A pending completion handler is invoked. */
- (void)clearBuffer;

@end

NS_ASSUME_NONNULL_END

/*
 * Copyright 2022 LiveKit
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

#import "RTCMacros.h"

NS_ASSUME_NONNULL_BEGIN

RTC_OBJC_EXPORT
@protocol RTC_OBJC_TYPE (RTCFrameEncryptorInterface)<NSObject>

  // Attempts to encrypt the provided frame. You may assume the encrypted_frame
  // will match the size returned by GetMaxCiphertextByteSize for a give frame.
  // You may assume that the frames will arrive in order if SRTP is enabled.
  // The ssrc will simply identify which stream the frame is travelling on. You
  // must set bytes_written to the number of bytes you wrote in the
  // encrypted_frame. 0 must be returned if successful all other numbers can be
  // selected by the implementer to represent error codes.
  // virtual int Encrypt(cricket::MediaType media_type,
  //                     uint32_t ssrc,
  //                     rtc::ArrayView<const uint8_t> additional_data,
  //                     rtc::ArrayView<const uint8_t> frame,
  //                     rtc::ArrayView<uint8_t> encrypted_frame,
  //                     size_t* bytes_written) = 0;

  - (int)encryptMediaType: (RTCMediaType)mediaType
                     ssrc: (NSNumber *)ssrc
           additionalData: (uint8_t *)additionalData
                    frame: (uint8_t *)frame
           encryptedFrame: (uint8_t *)encryptedFrame
             bytesWritten: (size_t *)bytesWritten;

  // Returns the total required length in bytes for the output of the
  // encryption. This can be larger than the actual number of bytes you need but
  // must never be smaller as it informs the size of the encrypted_frame buffer.
  // virtual size_t GetMaxCiphertextByteSize(cricket::MediaType media_type,
  //                                         size_t frame_size) = 0;

  - (size_t)getMaxCiphertextByteSize: (RTCMediaType)mediaType
                           frameSize: (size_t)frameSize;

@end

NS_ASSUME_NONNULL_END

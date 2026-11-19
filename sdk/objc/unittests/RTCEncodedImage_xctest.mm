/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "api/peerconnection/RTCEncodedImage+Private.h"

#import <XCTest/XCTest.h>

@interface RTCEncodedImageTests : XCTestCase
@end

@implementation RTCEncodedImageTests

- (void)testInitializedWithNativeEncodedImage {
  const auto encoded_data = webrtc::EncodedImageBuffer::Create(1);
  webrtc::EncodedImage encoded_image;
  encoded_image.SetEncodedData(encoded_data);

  RTC_OBJC_TYPE(RTCEncodedImage) *encodedImage =
      [[RTC_OBJC_TYPE(RTCEncodedImage) alloc]
          initWithNativeEncodedImage:encoded_image];

  XCTAssertEqual([encodedImage nativeEncodedImage].GetEncodedData(),
                 encoded_data);
}

- (void)testReceiveTimesRoundTripWithoutTimingFlags {
  webrtc::EncodedImage native;
  native.SetEncodedData(webrtc::EncodedImageBuffer::Create(1));
  native.timing_.receive_start_ms = 1234;
  native.timing_.receive_finish_ms = 1256;
  RTC_OBJC_TYPE(RTCEncodedImage) *image = [[RTC_OBJC_TYPE(RTCEncodedImage)
      alloc] initWithNativeEncodedImage:native];
  XCTAssertEqual(image.flags, webrtc::VideoSendTiming::kInvalid);
  XCTAssertEqual(image.receiveStartMs, 1234);
  XCTAssertEqual(image.receiveFinishMs, 1256);
  image.receiveStartMs = 2345;
  image.receiveFinishMs = 2378;
  auto roundTrip = [image nativeEncodedImage];
  XCTAssertEqual(roundTrip.timing_.receive_start_ms, 2345);
  XCTAssertEqual(roundTrip.timing_.receive_finish_ms, 2378);
}

- (void)testUnknownReceiveTimes {
  RTC_OBJC_TYPE(RTCEncodedImage) *image =
      [[RTC_OBJC_TYPE(RTCEncodedImage) alloc] init];
  XCTAssertEqual(image.receiveStartMs, -1);
  XCTAssertEqual(image.receiveFinishMs, -1);
  auto native = [image nativeEncodedImage];
  XCTAssertEqual(native.timing_.receive_start_ms, -1);
  XCTAssertEqual(native.timing_.receive_finish_ms, -1);
  native.SetEncodedData(webrtc::EncodedImageBuffer::Create());
  image = [[RTC_OBJC_TYPE(RTCEncodedImage) alloc]
      initWithNativeEncodedImage:native];
  XCTAssertEqual(image.receiveStartMs, -1);
  XCTAssertEqual(image.receiveFinishMs, -1);
}

- (void)testInitWithNSData {
  NSData *bufferData = [NSData data];
  RTC_OBJC_TYPE(RTCEncodedImage) *encodedImage =
      [[RTC_OBJC_TYPE(RTCEncodedImage) alloc] init];
  encodedImage.buffer = bufferData;

  webrtc::EncodedImage result_encoded_image = [encodedImage nativeEncodedImage];
  XCTAssertTrue(result_encoded_image.GetEncodedData() != nullptr);
  XCTAssertEqual(result_encoded_image.GetEncodedData()->data(),
                 bufferData.bytes);
}

- (void)testRetainsNativeEncodedImage {
  RTC_OBJC_TYPE(RTCEncodedImage) * encodedImage;
  {
    const auto encoded_data = webrtc::EncodedImageBuffer::Create(1);
    webrtc::EncodedImage encoded_image;
    encoded_image.SetEncodedData(encoded_data);
    encodedImage = [[RTC_OBJC_TYPE(RTCEncodedImage) alloc]
        initWithNativeEncodedImage:encoded_image];
  }
  webrtc::EncodedImage result_encoded_image = [encodedImage nativeEncodedImage];
  XCTAssertTrue(result_encoded_image.GetEncodedData() != nullptr);
  XCTAssertTrue(result_encoded_image.GetEncodedData()->data() != nullptr);
}

@end

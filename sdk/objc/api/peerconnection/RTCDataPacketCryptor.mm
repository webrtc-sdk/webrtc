/*
 * Copyright 2025 LiveKit
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

#import "RTCDataPacketCryptor.h"
#import "RTCFrameCryptorKeyProvider+Private.h"

#import <os/lock.h>
#include <memory>

#import "base/RTCLogging.h"
#import "helpers/NSString+StdString.h"

#include "api/crypto/frame_crypto_transformer.h"


@implementation RTC_OBJC_TYPE(RTCEncryptedPacket)

@synthesize data = _data;
@synthesize iv = _iv;
@synthesize keyIndex = _keyIndex;

- (instancetype)initWithData:(NSData *)data iv:(NSData *)iv keyIndex:(uint32_t)keyIndex {
  self = [super init];
  if (self) {
    _data = data;
    _iv = iv;
    _keyIndex = keyIndex;
  }
  return self;
}
@end

@implementation RTC_OBJC_TYPE (RTCDataPacketCryptor) {
  webrtc::scoped_refptr<webrtc::DataPacketCryptor> _data_packet_cryptor;
  os_unfair_lock _lock;
}

- (webrtc::FrameCryptorTransformer::Algorithm)algorithmFromEnum:(RTC_OBJC_TYPE(RTCCryptorAlgorithm))algorithm {
  switch (algorithm) {
    case RTC_OBJC_TYPE(RTCCryptorAlgorithmAesGcm):
      return webrtc::FrameCryptorTransformer::Algorithm::kAesGcm;
    default:
      return webrtc::FrameCryptorTransformer::Algorithm::kAesGcm;
  }
}

- (nullable instancetype)initWithAlgorithm:(RTC_OBJC_TYPE(RTCCryptorAlgorithm))algorithm
                               keyProvider:(RTC_OBJC_TYPE(RTCFrameCryptorKeyProvider) *)keyProvider {
  self = [super init];
  if (self) {
    _lock = OS_UNFAIR_LOCK_INIT;
    os_unfair_lock_lock(&_lock);
    _data_packet_cryptor =
        webrtc::make_ref_counted<webrtc::DataPacketCryptor>([self algorithmFromEnum:algorithm], keyProvider.nativeKeyProvider);
  
    os_unfair_lock_unlock(&_lock);
  }

  return self;
}

- (nullable RTC_OBJC_TYPE(RTCEncryptedPacket) *)encrypt:(NSString*)participantId keyIndex:(uint32_t)keyIndex data:(NSData *)data {
  // Convert NSData to std::vector<uint8_t>
  const uint8_t* bytes = (const uint8_t*)data.bytes;
  std::vector<uint8_t> payloadPacket(bytes, bytes + data.length);

  // Encrypt the packet
  os_unfair_lock_lock(&_lock);
  auto nativePacket = _data_packet_cryptor->Encrypt(participantId.UTF8String, keyIndex, payloadPacket);
  if (!nativePacket.ok()) {
    os_unfair_lock_unlock(&_lock);
    RTCLogError(@"Failed to encrypt data for %@: %s",
                participantId,
                nativePacket.error().message());
    return nil;
  }
  os_unfair_lock_unlock(&_lock);

  // Convert std::vector<uint8_t> to NSData
  NSData *packetData = [NSData dataWithBytes:nativePacket.value()->data.data() length:nativePacket.value()->data.size()];
  NSData *ivData = [NSData dataWithBytes:nativePacket.value()->iv.data() length:nativePacket.value()->iv.size()];

  // Convert to Objective-C RTCEncryptedPacket
  return [[RTC_OBJC_TYPE(RTCEncryptedPacket) alloc] initWithData:packetData iv:ivData keyIndex:nativePacket.value()->key_index];
}

- (nullable NSData *)decrypt:(NSString*)participantId encryptedPacket:(RTC_OBJC_TYPE(RTCEncryptedPacket) *)packet {
  // Convert NSData to std::vector<uint8_t>
  const uint8_t* dataBytes = (const uint8_t*)packet.data.bytes;
  std::vector<uint8_t> data(dataBytes, dataBytes + packet.data.length);
  const uint8_t* ivBytes = (const uint8_t*)packet.iv.bytes;
  std::vector<uint8_t> iv(ivBytes, ivBytes + packet.iv.length);
  
  auto encryptedPacket = webrtc::make_ref_counted<webrtc::EncryptedPacket>(
      std::vector<uint8_t>(data.begin(), data.end()),
      std::vector<uint8_t>(iv.begin(), iv.end()),
      packet.keyIndex);

  // Decrypt the packet
  os_unfair_lock_lock(&_lock);
  auto decryptedData = _data_packet_cryptor->Decrypt(participantId.UTF8String, encryptedPacket);
  if (!decryptedData.ok()) {
    os_unfair_lock_unlock(&_lock);
    RTCLogError(@"Failed to decrypt data for %@: %s",
                participantId,
                decryptedData.error().message());
    return nil;
  }
  os_unfair_lock_unlock(&_lock);
  return [NSData dataWithBytes:decryptedData.value().data() length:decryptedData.value().size()];
}

- (void)dealloc {
  os_unfair_lock_lock(&_lock);
  if (_data_packet_cryptor != nullptr) {
    _data_packet_cryptor = nullptr;
  }
  os_unfair_lock_unlock(&_lock);
}

@end

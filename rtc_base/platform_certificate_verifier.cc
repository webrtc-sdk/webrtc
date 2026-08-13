/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/platform_certificate_verifier.h"

#include <atomic>
#include <memory>

namespace webrtc {
namespace {

// Written once during library initialisation and read on network threads for
// every TLS handshake, hence the atomic rather than a plain pointer.
std::atomic<PlatformCertificateVerifierFactory> g_factory{nullptr};

}  // namespace

void SetPlatformCertificateVerifierFactory(
    PlatformCertificateVerifierFactory factory) {
  g_factory.store(factory, std::memory_order_release);
}

std::unique_ptr<SSLCertificateVerifier> CreatePlatformCertificateVerifier() {
  PlatformCertificateVerifierFactory factory =
      g_factory.load(std::memory_order_acquire);
  if (factory != nullptr) {
    return factory();
  }
  return CreateNativePlatformCertificateVerifier();
}

}  // namespace webrtc

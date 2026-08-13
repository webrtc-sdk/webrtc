/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "rtc_base/platform_certificate_verifier.h"

namespace webrtc {

// Used where no trust store is reachable from rtc_base. Linux, ChromeOS and the
// BSDs have no OS-level trust evaluation API at all; their system roots are a
// directory of PEM files whose location varies by distribution. Android does
// have one, but only through X509TrustManager, so sdk/android registers an
// implementation via SetPlatformCertificateVerifierFactory instead and this is
// never consulted there.
//
// Returning nullptr leaves the built-in anchors in rtc_base/ssl_roots.h as the
// only trust source, which is the pre-existing behaviour.
std::unique_ptr<SSLCertificateVerifier>
CreateNativePlatformCertificateVerifier() {
  return nullptr;
}

}  // namespace webrtc

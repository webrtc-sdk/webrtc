/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_PLATFORM_CERTIFICATE_VERIFIER_H_
#define RTC_BASE_PLATFORM_CERTIFICATE_VERIFIER_H_

#include <memory>

#include "rtc_base/ssl_certificate.h"

namespace webrtc {

// Creates a verifier that validates a peer certificate chain against the
// operating system's trust store, or nullptr on platforms that do not expose
// one (Linux, ChromeOS, BSD).
//
// This exists because the built-in trust anchors in rtc_base/ssl_roots.h are a
// small, infrequently regenerated snapshot. A chain that the host OS trusts may
// have no anchor there, which surfaces as a TURN/TLS handshake failure.
//
// The verifier is consulted only after the built-in anchors have already failed
// to produce a valid path, so it can widen what is accepted but never narrow
// it. Hostname matching is not performed here; OpenSSLAdapter checks it
// separately in SSLPostConnectionCheck.
std::unique_ptr<SSLCertificateVerifier> CreatePlatformCertificateVerifier();

}  //  namespace webrtc

#endif  // RTC_BASE_PLATFORM_CERTIFICATE_VERIFIER_H_

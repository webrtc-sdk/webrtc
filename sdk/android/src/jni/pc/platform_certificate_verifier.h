/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SDK_ANDROID_SRC_JNI_PC_PLATFORM_CERTIFICATE_VERIFIER_H_
#define SDK_ANDROID_SRC_JNI_PC_PLATFORM_CERTIFICATE_VERIFIER_H_

#include <memory>

#include "rtc_base/ssl_certificate.h"

namespace webrtc {
namespace jni {

// Defers to the platform trust store via X509TrustManager when the anchors in
// rtc_base/ssl_roots.h yield no path for a peer chain.
//
// rtc_base cannot host this: reaching the JVM requires the JNI layer, which
// lives here. JNI_OnLoad registers it through
// SetPlatformCertificateVerifierFactory, so OpenSSLAdapter picks it up for every
// consumer rather than only those that build their dependencies through this
// SDK. It is still only consulted after the built-in anchors have failed, and an
// application-supplied tls_cert_verifier continues to take precedence.
class PlatformCertificateVerifier : public SSLCertificateVerifier {
 public:
  PlatformCertificateVerifier();
  ~PlatformCertificateVerifier() override;

  bool Verify(const SSLCertificate& certificate) override;
  bool VerifyChain(const SSLCertChain& chain) override;
};

// Matches rtc_base's PlatformCertificateVerifierFactory signature so that
// JNI_OnLoad can hand it to SetPlatformCertificateVerifierFactory.
std::unique_ptr<SSLCertificateVerifier> CreateAndroidCertificateVerifier();

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_PLATFORM_CERTIFICATE_VERIFIER_H_

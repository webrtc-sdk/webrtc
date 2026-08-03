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

#include "rtc_base/ssl_certificate.h"

namespace webrtc {
namespace jni {

// Defers to the platform trust store via X509TrustManager when the anchors in
// rtc_base/ssl_roots.h yield no path for a peer chain. Installed as the default
// tls_cert_verifier where the application has not supplied one of its own; it
// is consulted only after built-in verification has already failed, so it can
// widen what is accepted but never narrow it.
//
// rtc_base cannot host this: reaching the JVM requires the JNI layer, which
// lives here. This mirrors how AndroidNetworkMonitorFactory supplies an
// rtc_base interface from the Android SDK.
class PlatformCertificateVerifier : public SSLCertificateVerifier {
 public:
  PlatformCertificateVerifier();
  ~PlatformCertificateVerifier() override;

  bool Verify(const SSLCertificate& certificate) override;
  bool VerifyChain(const SSLCertChain& chain) override;
};

}  // namespace jni
}  // namespace webrtc

#endif  // SDK_ANDROID_SRC_JNI_PC_PLATFORM_CERTIFICATE_VERIFIER_H_

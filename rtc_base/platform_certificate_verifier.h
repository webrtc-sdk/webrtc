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

// Returns a verifier that validates a peer certificate chain against the
// operating system's trust store, or nullptr where none is reachable.
//
// This exists because the built-in trust anchors in rtc_base/ssl_roots.h are a
// small, infrequently regenerated snapshot. A chain that the host OS trusts may
// have no anchor there, which surfaces as a TURN/TLS handshake failure.
//
// With the built-in anchors compiled in, which is the default, the verifier is
// consulted only after ssl_roots.h has already failed to produce a valid path,
// so it can widen what is accepted but never narrow it. Under
// WEBRTC_EXCLUDE_BUILT_IN_SSL_ROOT_CERTS there are no built-in anchors and it
// becomes the only trust decision made.
//
// This is a default, not an override. OpenSSLAdapter installs it only when the
// embedder supplied no verifier of its own, so anything set through
// PeerConnectionDependencies::tls_cert_verifier — the ObjC
// certificateVerifier: initialiser, Android's
// PeerConnectionDependencies.setSSLCertificateVerifier — takes precedence and
// replaces this outright. Note the consequence: an embedder that supplies a
// verifier does not also get the OS trust store as a fallback behind it, which
// matters for anyone who adopted that API as a workaround for the very gap this
// closes.
//
// Hostname matching is not performed here; OpenSSLAdapter checks it separately
// in SSLPostConnectionCheck.
std::unique_ptr<SSLCertificateVerifier> CreatePlatformCertificateVerifier();

using PlatformCertificateVerifierFactory =
    std::unique_ptr<SSLCertificateVerifier> (*)();

// Registers a factory for platforms whose trust store cannot be reached from
// rtc_base itself. Android is the case in point: its anchors are only available
// through X509TrustManager, which needs the JVM, and the JNI layer lives in
// sdk/android. That layer registers here from JNI_OnLoad.
//
// Registering process-wide rather than injecting through
// PeerConnectionDependencies is deliberate. Injection only reaches consumers
// that build their dependencies through the Java or ObjC SDK; anything binding
// the C++ API directly — webrtc-sys, and so the Rust and Python SDKs — supplies
// its own PeerConnectionFactoryDependencies and would silently miss it.
//
// A registered factory takes precedence over the implementation compiled in for
// the platform. Must be called before the first TLS handshake; passing nullptr
// unregisters. Not thread-safe against concurrent handshakes, which is why the
// only intended caller is library initialisation.
void SetPlatformCertificateVerifierFactory(
    PlatformCertificateVerifierFactory factory);

// The implementation compiled in for this platform, or nullptr on platforms
// that expose no trust evaluation API. Called by
// CreatePlatformCertificateVerifier when nothing has been registered; not
// intended for use elsewhere.
std::unique_ptr<SSLCertificateVerifier>
CreateNativePlatformCertificateVerifier();

}  //  namespace webrtc

#endif  // RTC_BASE_PLATFORM_CERTIFICATE_VERIFIER_H_

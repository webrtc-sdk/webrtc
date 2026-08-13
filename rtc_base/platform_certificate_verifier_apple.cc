/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <memory>

#include "rtc_base/buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_certificate_verifier.h"
#include "rtc_base/ssl_certificate.h"

namespace webrtc {
namespace {

template <typename T>
class ScopedCF {
 public:
  ScopedCF() = default;
  explicit ScopedCF(T ref) : ref_(ref) {}
  ScopedCF(const ScopedCF&) = delete;
  ScopedCF& operator=(const ScopedCF&) = delete;
  ~ScopedCF() {
    if (ref_) {
      CFRelease(ref_);
    }
  }

  T get() const { return ref_; }
  T* receive() { return &ref_; }
  explicit operator bool() const { return ref_ != nullptr; }

 private:
  T ref_ = nullptr;
};

class AppleCertificateVerifier final : public SSLCertificateVerifier {
 public:
  bool Verify(const SSLCertificate& certificate) override {
    return VerifyChain(SSLCertChain(certificate.Clone()));
  }

  bool VerifyChain(const SSLCertChain& chain) override {
    if (chain.GetSize() == 0) {
      return false;
    }

    ScopedCF<CFMutableArrayRef> certs(
        CFArrayCreateMutable(kCFAllocatorDefault, chain.GetSize(),
                             &kCFTypeArrayCallBacks));
    if (!certs) {
      return false;
    }

    // Appended in the order received. SSLCertificateVerifier::VerifyChain is
    // documented to deliver the chain leaf first, then intermediates, which is
    // also what SecTrustCreateWithCertificates expects: it evaluates element 0
    // and treats the remainder as candidate intermediates.
    for (size_t i = 0; i < chain.GetSize(); ++i) {
      Buffer der;
      chain.Get(i).ToDER(&der);
      ScopedCF<CFDataRef> data(CFDataCreate(
          kCFAllocatorDefault, der.data(), static_cast<CFIndex>(der.size())));
      if (!data) {
        return false;
      }
      ScopedCF<SecCertificateRef> cert(
          SecCertificateCreateWithData(kCFAllocatorDefault, data.get()));
      if (!cert) {
        RTC_LOG(LS_WARNING) << "Peer certificate at depth " << i
                            << " could not be parsed by Security.framework.";
        return false;
      }
      CFArrayAppendValue(certs.get(), cert.get());
    }

    // Hostname matching is done by OpenSSLAdapter::SSLPostConnectionCheck, so
    // no name is supplied here. The SSL policy is still used rather than a
    // basic X.509 one so that TLS-specific rules (server extended key usage,
    // and so on) continue to be enforced.
    ScopedCF<SecPolicyRef> policy(SecPolicyCreateSSL(/*server=*/true, nullptr));
    if (!policy) {
      return false;
    }

    ScopedCF<SecTrustRef> trust;
    if (SecTrustCreateWithCertificates(certs.get(), policy.get(),
                                       trust.receive()) != errSecSuccess ||
        !trust) {
      return false;
    }

    // The peer sent the whole chain, so nothing needs to be fetched. Leaving
    // this enabled would allow an AIA lookup to block the network thread for
    // the duration of a plaintext HTTP round trip mid-handshake. Note that this
    // also stops OCSP and CRL fetching, so revocation is decided on whatever
    // the system has already cached; a blocking revocation fetch on this thread
    // is the worse of the two.
    SecTrustSetNetworkFetchAllowed(trust.get(), false);

    CFErrorRef error = nullptr;
    const bool trusted = SecTrustEvaluateWithError(trust.get(), &error);
    ScopedCF<CFErrorRef> scoped_error(error);
    if (!trusted) {
      ScopedCF<CFStringRef> reason(
          error != nullptr ? CFErrorCopyDescription(error) : nullptr);
      char buffer[256] = {};
      if (reason && CFStringGetCString(reason.get(), buffer, sizeof(buffer),
                                       kCFStringEncodingUTF8)) {
        RTC_LOG(LS_INFO) << "Peer certificate chain was rejected by the system "
                            "trust store: "
                         << buffer;
      } else {
        RTC_LOG(LS_INFO) << "Peer certificate chain was rejected by the system "
                            "trust store.";
      }
    }
    return trusted;
  }
};

}  // namespace

std::unique_ptr<SSLCertificateVerifier>
CreateNativePlatformCertificateVerifier() {
  return std::make_unique<AppleCertificateVerifier>();
}

}  // namespace webrtc

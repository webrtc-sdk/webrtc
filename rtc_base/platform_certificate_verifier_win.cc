/*
 *  Copyright 2026 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <windows.h>
// clang-format off
// wincrypt.h must follow windows.h.
#include <wincrypt.h>
// clang-format on

#include <memory>

#include "rtc_base/buffer.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_certificate_verifier.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/string_utils.h"

namespace webrtc {
namespace {

// From wininet.h, which cannot be included alongside wincrypt.h here.
constexpr DWORD kSecurityFlagIgnoreCertCnInvalid = 0x00001000;

class ScopedCertContext {
 public:
  explicit ScopedCertContext(PCCERT_CONTEXT ctx) : ctx_(ctx) {}
  ScopedCertContext(const ScopedCertContext&) = delete;
  ScopedCertContext& operator=(const ScopedCertContext&) = delete;
  ~ScopedCertContext() {
    if (ctx_) {
      CertFreeCertificateContext(ctx_);
    }
  }
  PCCERT_CONTEXT get() const { return ctx_; }

 private:
  PCCERT_CONTEXT ctx_;
};

PCCERT_CONTEXT CreateContext(const SSLCertificate& cert) {
  Buffer der;
  cert.ToDER(&der);
  return CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                      der.data(),
                                      static_cast<DWORD>(der.size()));
}

class WinCertificateVerifier final : public SSLCertificateVerifier {
 public:
  bool Verify(const SSLCertificate& certificate) override {
    return VerifyChain(SSLCertChain(certificate.Clone()));
  }

  bool VerifyChain(const SSLCertChain& chain) override {
    if (chain.GetSize() == 0) {
      return false;
    }

    // Intermediates the peer sent go into a temporary in-memory store so the
    // chain engine can use them without touching any persistent store.
    HCERTSTORE intermediates =
        CertOpenStore(CERT_STORE_PROV_MEMORY, 0, NULL,
                      CERT_STORE_CREATE_NEW_FLAG | CERT_STORE_DEFER_CLOSE_UNTIL_LAST_FREE_FLAG,
                      NULL);
    if (!intermediates) {
      return false;
    }

    for (size_t i = 1; i < chain.GetSize(); ++i) {
      Buffer der;
      chain.Get(i).ToDER(&der);
      if (!CertAddEncodedCertificateToStore(
              intermediates, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
              der.data(), static_cast<DWORD>(der.size()),
              CERT_STORE_ADD_ALWAYS, NULL)) {
        const DWORD error = GetLastError();
        // Not fatal by itself, since the engine may still find this issuer in a
        // system store. Logged so that a path failure below is attributable
        // rather than unexplained.
        RTC_LOG(LS_WARNING) << "Could not add peer certificate at depth " << i
                            << " to the temporary store, error "
                            << ToHex(static_cast<int>(error));
      }
    }

    // SSLCertificateVerifier::VerifyChain is documented to deliver the chain
    // leaf first, then intermediates, so element 0 is the server certificate
    // and the loop above covers the rest. CertGetCertificateChain wants the
    // leaf as its subject, with the others merely available to it.
    ScopedCertContext leaf(CreateContext(chain.Get(0)));
    if (!leaf.get()) {
      CertCloseStore(intermediates, 0);
      return false;
    }

    CERT_CHAIN_PARA chain_para = {};
    chain_para.cbSize = sizeof(chain_para);
    LPSTR usage[] = {const_cast<LPSTR>(szOID_PKIX_KP_SERVER_AUTH)};
    chain_para.RequestedUsage.dwType = USAGE_MATCH_TYPE_AND;
    chain_para.RequestedUsage.Usage.cUsageIdentifier = 1;
    chain_para.RequestedUsage.Usage.rgpszUsageIdentifier = usage;

    PCCERT_CHAIN_CONTEXT chain_context = nullptr;
    // CERT_CHAIN_CACHE_END_CERT keeps this off the wire; revocation is left to
    // whatever the engine has cached rather than blocking the network thread.
    const BOOL built = CertGetCertificateChain(
        NULL, leaf.get(), NULL, intermediates, &chain_para,
        CERT_CHAIN_CACHE_END_CERT | CERT_CHAIN_REVOCATION_CHECK_CACHE_ONLY,
        NULL, &chain_context);
    CertCloseStore(intermediates, 0);

    if (!built || !chain_context) {
      return false;
    }

    // Hostname matching is done by OpenSSLAdapter::SSLPostConnectionCheck, so
    // the name check is suppressed here.
    SSL_EXTRA_CERT_CHAIN_POLICY_PARA ssl_para = {};
    ssl_para.cbSize = sizeof(ssl_para);
    ssl_para.dwAuthType = AUTHTYPE_SERVER;
    ssl_para.fdwChecks = kSecurityFlagIgnoreCertCnInvalid;

    CERT_CHAIN_POLICY_PARA policy_para = {};
    policy_para.cbSize = sizeof(policy_para);
    policy_para.pvExtraPolicyPara = &ssl_para;

    CERT_CHAIN_POLICY_STATUS policy_status = {};
    policy_status.cbSize = sizeof(policy_status);

    const BOOL checked = CertVerifyCertificateChainPolicy(
        CERT_CHAIN_POLICY_SSL, chain_context, &policy_para, &policy_status);
    CertFreeCertificateChain(chain_context);

    if (!checked || policy_status.dwError != 0) {
      RTC_LOG(LS_INFO) << "Peer certificate chain was rejected by the system "
                          "trust store, error 0x"
                       << ToHex(static_cast<int>(policy_status.dwError));
      return false;
    }
    return true;
  }
};

}  // namespace

std::unique_ptr<SSLCertificateVerifier>
CreateNativePlatformCertificateVerifier() {
  return std::make_unique<WinCertificateVerifier>();
}

}  // namespace webrtc

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

#include <memory>
#include <utility>
#include <vector>

#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_identity.h"
#include "test/gtest.h"

// A chain the host OS *accepts* cannot be asserted here without a certificate
// issued by a real CA, which would expire and turn this into a time bomb, or
// without writing to the machine's trust store. What is covered instead is
// everything that must hold regardless of local trust configuration: the
// factory contract, and that untrusted input is refused rather than accepted
// or crashed on.

namespace webrtc {
namespace {

#if defined(WEBRTC_MAC) || defined(WEBRTC_WIN)
constexpr bool kPlatformVerifierExpected = true;
#else
constexpr bool kPlatformVerifierExpected = false;
#endif

std::unique_ptr<SSLCertificate> SelfSignedCert(absl::string_view name) {
  std::unique_ptr<SSLIdentity> identity =
      SSLIdentity::Create(name, KeyParams::ECDSA());
  return identity == nullptr ? nullptr : identity->certificate().Clone();
}

// Accepts everything, which no real platform implementation would do for a
// self-signed certificate. That difference is what lets the registry tests tell
// a registered factory apart from the one compiled in.
class AcceptAllVerifier final : public SSLCertificateVerifier {
 public:
  bool Verify(const SSLCertificate& /*certificate*/) override { return true; }
};

std::unique_ptr<SSLCertificateVerifier> MakeAcceptAllVerifier() {
  return std::make_unique<AcceptAllVerifier>();
}

// SetPlatformCertificateVerifierFactory is process-wide, so a test that
// registers must unregister however it leaves.
class ScopedRegisteredFactory {
 public:
  explicit ScopedRegisteredFactory(PlatformCertificateVerifierFactory factory) {
    SetPlatformCertificateVerifierFactory(factory);
  }
  ScopedRegisteredFactory(const ScopedRegisteredFactory&) = delete;
  ScopedRegisteredFactory& operator=(const ScopedRegisteredFactory&) = delete;
  ~ScopedRegisteredFactory() {
    SetPlatformCertificateVerifierFactory(nullptr);
  }
};

TEST(PlatformCertificateVerifierTest, RegisteredFactoryTakesPrecedence) {
  std::unique_ptr<SSLCertificate> cert = SelfSignedCert("untrusted.invalid");
  ASSERT_TRUE(cert != nullptr);

  {
    ScopedRegisteredFactory registered(&MakeAcceptAllVerifier);
    std::unique_ptr<SSLCertificateVerifier> verifier =
        CreatePlatformCertificateVerifier();
    ASSERT_TRUE(verifier != nullptr);
    // A platform implementation would refuse this; the registered one does not,
    // so accepting it shows the registered factory was preferred. This is what
    // lets Android reach its trust store on a build where rtc_base itself has no
    // implementation to offer.
    EXPECT_TRUE(verifier->VerifyChain(SSLCertChain(cert->Clone())));
  }

  // Leaving the scope must restore whatever the platform provides.
  std::unique_ptr<SSLCertificateVerifier> restored =
      CreatePlatformCertificateVerifier();
  EXPECT_EQ(restored != nullptr, kPlatformVerifierExpected);
  if (restored != nullptr) {
    EXPECT_FALSE(restored->VerifyChain(SSLCertChain(cert->Clone())));
  }
}

TEST(PlatformCertificateVerifierTest, FactoryMatchesPlatformSupport) {
  std::unique_ptr<SSLCertificateVerifier> verifier =
      CreatePlatformCertificateVerifier();
  EXPECT_EQ(verifier != nullptr, kPlatformVerifierExpected);
}

TEST(PlatformCertificateVerifierTest, RejectsEmptyChain) {
  std::unique_ptr<SSLCertificateVerifier> verifier =
      CreatePlatformCertificateVerifier();
  if (verifier == nullptr) {
    GTEST_SKIP() << "No platform trust store on this target.";
  }

  SSLCertChain empty((std::vector<std::unique_ptr<SSLCertificate>>()));
  EXPECT_FALSE(verifier->VerifyChain(empty));
}

TEST(PlatformCertificateVerifierTest, RejectsSelfSignedCertificate) {
  std::unique_ptr<SSLCertificateVerifier> verifier =
      CreatePlatformCertificateVerifier();
  if (verifier == nullptr) {
    GTEST_SKIP() << "No platform trust store on this target.";
  }

  std::unique_ptr<SSLCertificate> cert = SelfSignedCert("not-a-real-ca.invalid");
  ASSERT_TRUE(cert != nullptr);

  // Both entry points must refuse it: Verify() is what the base class routes a
  // single certificate through, VerifyChain() is what OpenSSLAdapter calls.
  EXPECT_FALSE(verifier->Verify(*cert));

  SSLCertChain chain(cert->Clone());
  EXPECT_FALSE(verifier->VerifyChain(chain));
}

TEST(PlatformCertificateVerifierTest, RejectsMultiCertificateUntrustedChain) {
  std::unique_ptr<SSLCertificateVerifier> verifier =
      CreatePlatformCertificateVerifier();
  if (verifier == nullptr) {
    GTEST_SKIP() << "No platform trust store on this target.";
  }

  // Exercises the loop that walks every element of the chain, not just the
  // leaf. The certificates do not chain to each other, which is precisely the
  // sort of input that must not be mistaken for a valid path.
  std::vector<std::unique_ptr<SSLCertificate>> certs;
  for (int i = 0; i < 3; ++i) {
    std::unique_ptr<SSLCertificate> cert = SelfSignedCert("untrusted.invalid");
    ASSERT_TRUE(cert != nullptr);
    certs.push_back(std::move(cert));
  }

  SSLCertChain chain(std::move(certs));
  ASSERT_EQ(chain.GetSize(), 3u);
  EXPECT_FALSE(verifier->VerifyChain(chain));
}

TEST(PlatformCertificateVerifierTest, VerifierIsReusableAcrossChains) {
  std::unique_ptr<SSLCertificateVerifier> verifier =
      CreatePlatformCertificateVerifier();
  if (verifier == nullptr) {
    GTEST_SKIP() << "No platform trust store on this target.";
  }

  // One verifier is installed per adapter but evaluates every peer certificate
  // that adapter sees, so it must hold no per-evaluation state.
  for (int i = 0; i < 3; ++i) {
    std::unique_ptr<SSLCertificate> cert = SelfSignedCert("untrusted.invalid");
    ASSERT_TRUE(cert != nullptr);
    SSLCertChain chain(std::move(cert));
    EXPECT_FALSE(verifier->VerifyChain(chain));
  }
}

}  // namespace
}  // namespace webrtc

/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;
import java.io.InputStream;
import java.security.KeyStore;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import javax.net.ssl.TrustManager;
import javax.net.ssl.TrustManagerFactory;
import javax.net.ssl.X509TrustManager;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.webrtc.certificateverifiertest.R;

/** Runs with the dedicated APK's real Network Security Configuration and platform PKI. */
@RunWith(AndroidJUnit4.class)
@SmallTest
public class PlatformCertificateVerifierNscTest {
  private X509Certificate[] chain;
  private byte[][] derChain;

  private static X509Certificate readCertificate(int resource) throws Exception {
    try (InputStream input =
             InstrumentationRegistry.getTargetContext().getResources().openRawResource(resource)) {
      return (X509Certificate) CertificateFactory.getInstance("X.509").generateCertificate(input);
    }
  }

  @Before
  public void setUp() throws Exception {
    // Public test fixtures only, valid from 2020 to 2120. The APK trusts its
    // own CA resource; neither the system nor user trust store is modified.
    chain =
        new X509Certificate[] {readCertificate(R.raw.test_server), readCertificate(R.raw.test_ca)};
    derChain = new byte[][] {chain[0].getEncoded(), chain[1].getEncoded()};
  }

  @Test
  public void unrelatedDomainConfigDoesNotRejectTrustedServer() throws Exception {
    TrustManagerFactory factory =
        TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm());
    factory.init((KeyStore) null);
    X509TrustManager manager = null;
    for (TrustManager candidate : factory.getTrustManagers()) {
      if (candidate instanceof X509TrustManager) {
        manager = (X509TrustManager) candidate;
        break;
      }
    }
    assertTrue(manager != null);
    assertEquals("android.security.net.config.RootTrustManager", manager.getClass().getName());
    try {
      // Reproduce the hostname-free platform fallback used before the fix.
      manager.checkServerTrusted(chain, chain[0].getPublicKey().getAlgorithm());
      fail("The hostname-free check must reject a per-domain NSC");
    } catch (CertificateException expected) {
      assertTrue(expected.getMessage().contains("Domain specific configurations"));
    }
    assertTrue(PlatformCertificateVerifier.verifyServerChain(derChain, "turn.test"));
  }

  @Test
  public void domainConfigCanRestrictBaseTrust() {
    assertTrue(PlatformCertificateVerifier.verifyServerChain(derChain, "turn.test"));
    assertFalse(PlatformCertificateVerifier.verifyServerChain(derChain, "strict.test"));
    assertTrue(PlatformCertificateVerifier.verifyServerChain(derChain, "turn.test"));
  }

  @Test
  public void domainPinsCanRejectAnOtherwiseTrustedChain() {
    assertTrue(PlatformCertificateVerifier.verifyServerChain(derChain, "turn.test"));
    assertFalse(PlatformCertificateVerifier.verifyServerChain(derChain, "pinned.test"));
  }

  @Test
  public void missingHostKeepsTheHostnameFreeCheck() {
    // Without a hostname the platform gets the hostname-free call, as before the fix, which a
    // per-domain configuration refuses outright; no domain policy is consulted.
    assertFalse(PlatformCertificateVerifier.verifyServerChain(derChain, ""));
  }
}

/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.net.http.X509TrustManagerExtensions;
import android.util.Base64;
import androidx.test.runner.AndroidJUnit4;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.Arrays;
import java.util.List;
import javax.net.ssl.X509TrustManager;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

@RunWith(AndroidJUnit4.class)
@Config(manifest = Config.NONE, sdk = {24, 28, 35})
public class PlatformCertificateVerifierTest {
  // Public parsing fixtures only. The recording policy does not validate their
  // signatures or dates; these tests cover transport of chain and host to Android.
  private static final byte[] LEAF = Base64.decode(
      "MIIBgzCCASmgAwIBAgIUBeEtskZOC8kFK7bIUCNEG2yB49swCgYIKoZIzj0EAwIwFzEVMBMGA1UEAwwMbGVh"
          + "Zi5pbnZhbGlkMB4XDTI2MDkyNzA2MzMxMFoXDTI2MDkyODA2MzMxMFowFzEVMBMGA1UEAwwMbGVhZi5pbnZh"
          + "bGlkMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEvWcwJpZr9dPcBk5Lyp9sfRm5wRPMZFbbjTcaBf47IhNa"
          + "wQgL/0Dda1tCKWMUkUj8XlEd/P37G2T/UjSxS3odyaNTMFEwHQYDVR0OBBYEFI49FR7n4ABkM8nR3kC5W6dk"
          + "I29dMB8GA1UdIwQYMBaAFI49FR7n4ABkM8nR3kC5W6dkI29dMA8GA1UdEwEB/wQFMAMBAf8wCgYIKoZIzj0E"
          + "AwIDSAAwRQIgKxtZ3p/qpCXmpZyO7ThdIb7inAOWgcWCLt9QDTPq+u4CIQDJm84iM1nOhPjUBcTactzQE32Y"
          + "3YNLIiwejdra3eVZIw==",
      Base64.DEFAULT);
  private static final byte[] ISSUER = Base64.decode(
      "MIIBiDCCAS2gAwIBAgIUXh1r4xLoQGjXbhOt4iAaoC0l/GAwCgYIKoZIzj0EAwIwGTEXMBUGA1UEAwwOaXNz"
          + "dWVyLmludmFsaWQwHhcNMjYwOTI3MDYzMzEwWhcNMjYwOTI4MDYzMzEwWjAZMRcwFQYDVQQDDA5pc3N1ZXIu"
          + "aW52YWxpZDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABC/9TfeoAivC8sfOJt371TJVD6sZfQxeuoyC0F3f"
          + "8K1RRbnLf7v9uxdX6mK1B9L+XMnS1bh9/0VZnhm2TOWCpOKjUzBRMB0GA1UdDgQWBBQ9GtDw9IKrkEsORY37"
          + "S3LE+zja7zAfBgNVHSMEGDAWgBQ9GtDw9IKrkEsORY37S3LE+zja7zAPBgNVHRMBAf8EBTADAQH/MAoGCCqG"
          + "SM49BAMCA0kAMEYCIQDhJv8HgKXGgLJlGLu+ptpRlYejdUUmMLnM9zdqzzw1lQIhAPounN0KLusK/991LXmk"
          + "hsoy3+UUclVZQiY6jdn/dhZM",
      Base64.DEFAULT);

  // Public because Android's real X509TrustManagerExtensions invokes this policy by reflection.
  public static final class HostPolicy implements X509TrustManager {
    int checks;
    X509Certificate[] chain;
    String host;
    String authType;

    public List<X509Certificate> checkServerTrusted(
        X509Certificate[] chain, String authType, String host) throws CertificateException {
      ++checks;
      this.chain = chain;
      this.host = host;
      this.authType = authType;
      if (!"allowed.test".equals(host)) {
        throw new CertificateException("The policy for this host refuses the chain");
      }
      return Arrays.asList(chain);
    }

    @Override
    public void checkServerTrusted(X509Certificate[] chain, String authType)
        throws CertificateException {
      throw new AssertionError("A hostname-aware trust check is required");
    }

    @Override
    public void checkClientTrusted(X509Certificate[] chain, String authType)
        throws CertificateException {
      throw new CertificateException("Not a client trust policy");
    }

    @Override
    public X509Certificate[] getAcceptedIssuers() {
      return new X509Certificate[0];
    }

    public boolean isUserAddedCertificate(X509Certificate certificate) {
      return false;
    }
  }

  private static boolean verify(byte[][] chain, String host, X509TrustManagerExtensions manager) {
    try {
      return PlatformCertificateVerifier.verifyServerChain(
          chain, host, manager, CertificateFactory.getInstance("X.509"));
    } catch (CertificateException e) {
      throw new AssertionError(e);
    }
  }

  @Test
  public void forwardsWholeChainInOrderAndActualHost() throws Exception {
    HostPolicy policy = new HostPolicy();
    assertTrue(verify(
        new byte[][] {LEAF, ISSUER}, "allowed.test", new X509TrustManagerExtensions(policy)));
    assertEquals(1, policy.checks);
    assertEquals("allowed.test", policy.host);
    assertEquals("EC", policy.authType);
    assertEquals(2, policy.chain.length);
    assertArrayEquals(LEAF, policy.chain[0].getEncoded());
    assertArrayEquals(ISSUER, policy.chain[1].getEncoded());
  }

  @Test
  public void previousAcceptanceDoesNotAuthorizeAnotherHost() {
    HostPolicy policy = new HostPolicy();
    X509TrustManagerExtensions manager = new X509TrustManagerExtensions(policy);
    byte[][] chain = new byte[][] {LEAF, ISSUER};
    assertTrue(verify(chain, "allowed.test", manager));
    assertFalse(verify(chain, "strict.test", manager));
    assertEquals(2, policy.checks);
    assertEquals("strict.test", policy.host);
  }

  @Test
  public void missingHostKeepsTheHostnameFreePolicy() {
    HostPolicy policy = new HostPolicy();
    X509TrustManagerExtensions manager = new X509TrustManagerExtensions(policy);
    assertFalse(verify(new byte[][] {LEAF}, "", manager));
    assertNull(policy.host);
    assertFalse(verify(new byte[][] {LEAF}, null, manager));
    assertNull(policy.host);
    assertEquals(2, policy.checks);
  }

  @Test
  public void malformedOrEmptyChainDoesNotConsultTrustPolicy() {
    HostPolicy policy = new HostPolicy();
    X509TrustManagerExtensions manager = new X509TrustManagerExtensions(policy);
    assertFalse(verify(null, "allowed.test", manager));
    assertFalse(verify(new byte[0][], "allowed.test", manager));
    assertFalse(verify(new byte[][] {{1, 2, 3}}, "allowed.test", manager));
    assertFalse(verify(new byte[][] {LEAF, null}, "allowed.test", manager));
    assertEquals(0, policy.checks);
  }
}

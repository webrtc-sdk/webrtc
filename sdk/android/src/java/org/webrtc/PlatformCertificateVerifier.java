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

import androidx.annotation.Nullable;
import java.io.ByteArrayInputStream;
import java.security.KeyStore;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.List;
import javax.net.ssl.TrustManager;
import javax.net.ssl.TrustManagerFactory;
import javax.net.ssl.X509TrustManager;

/**
 * Validates a peer certificate chain against the platform trust store, for use when the trust
 * anchors compiled into rtc_base/ssl_roots.h contain no path for it.
 *
 * <p>Only reachable from native code; the chain arrives whole, so no certificate has to be fetched
 * to complete it. Hostname matching is not done here — OpenSSLAdapter checks it separately.
 */
final class PlatformCertificateVerifier {
  private static final String TAG = "PlatformCertificateVerifier";

  @Nullable private static X509TrustManager trustManager;
  @Nullable private static CertificateFactory certificateFactory;
  private static boolean initialized;

  private PlatformCertificateVerifier() {}

  private static synchronized boolean initialize() {
    if (initialized) {
      return trustManager != null && certificateFactory != null;
    }
    initialized = true;
    try {
      TrustManagerFactory factory =
          TrustManagerFactory.getInstance(TrustManagerFactory.getDefaultAlgorithm());
      // A null KeyStore selects the platform's own trust anchors, which includes any the user or
      // a device administrator has installed.
      factory.init((KeyStore) null);
      for (TrustManager candidate : factory.getTrustManagers()) {
        if (candidate instanceof X509TrustManager) {
          trustManager = (X509TrustManager) candidate;
          break;
        }
      }
      certificateFactory = CertificateFactory.getInstance("X.509");
    } catch (Exception e) {
      Logging.e(TAG, "Could not reach the platform trust store", e);
      trustManager = null;
      certificateFactory = null;
    }
    return trustManager != null && certificateFactory != null;
  }

  /**
   * @param derChain peer certificates in DER form, leaf first.
   * @return whether the chain terminates in an anchor the platform trusts.
   */
  @CalledByNative
  static boolean verifyServerChain(byte[][] derChain) {
    if (derChain == null || derChain.length == 0 || !initialize()) {
      return false;
    }
    final X509TrustManager manager = trustManager;
    final CertificateFactory factory = certificateFactory;
    if (manager == null || factory == null) {
      return false;
    }

    try {
      List<X509Certificate> parsed = new ArrayList<>(derChain.length);
      for (byte[] der : derChain) {
        parsed.add(
            (X509Certificate) factory.generateCertificate(new ByteArrayInputStream(der)));
      }
      X509Certificate[] chain = parsed.toArray(new X509Certificate[0]);

      // The key algorithm of the leaf stands in for the TLS key-exchange authType, which is not
      // available at this layer. Conscrypt uses it only to pick a validation profile.
      String authType = chain[0].getPublicKey().getAlgorithm();
      manager.checkServerTrusted(chain, authType == null ? "RSA" : authType);
      return true;
    } catch (Exception e) {
      Logging.d(TAG, "Peer certificate chain was rejected by the platform trust store: " + e);
      return false;
    }
  }
}

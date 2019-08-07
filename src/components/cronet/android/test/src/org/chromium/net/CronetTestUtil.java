// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.net.impl.CronetEngineBuilderImpl;
import org.chromium.net.impl.CronetUrlRequest;
import org.chromium.net.impl.CronetUrlRequestContext;

/**
 * Utilities for Cronet testing
 */
@JNINamespace("cronet")
public class CronetTestUtil {
    // QUIC test domain must match the certificate used
    // (quic-chain.pem and quic-leaf-cert.key), and the file served (
    // components/cronet/android/test/assets/test/quic_data/simple.txt).
    static final String QUIC_FAKE_HOST = "test.example.com";
    private static final String[] TEST_DOMAINS = {QUIC_FAKE_HOST};
    private static final String LOOPBACK_ADDRESS = "127.0.0.1";

    /**
     * Generates rules for customized DNS mapping for testing hostnames used by test servers,
     * namely:
     * <ul>
     * <li>{@link QuicTestServer#getServerHost}</li>
     * </ul>
     * Maps the test hostnames to 127.0.0.1.
     */
    public static JSONObject generateHostResolverRules() throws JSONException {
        return generateHostResolverRules(LOOPBACK_ADDRESS);
    }

    /**
     * Generates rules for customized DNS mapping for testing hostnames used by test servers,
     * namely:
     * <ul>
     * <li>{@link QuicTestServer#getServerHost}</li>
     * </ul>
     * @param destination host to map to
     */
    public static JSONObject generateHostResolverRules(String destination) throws JSONException {
        StringBuilder rules = new StringBuilder();
        for (String domain : TEST_DOMAINS) {
            rules.append("MAP " + domain + " " + destination + ",");
        }
        return new JSONObject().put("host_resolver_rules", rules);
    }

    /**
     * Prepare {@code cronetEngine}'s network thread so libcronet_test code can run on it.
     */
    public static class NetworkThreadTestConnector {
        private final CronetUrlRequestContext mRequestContext;

        public NetworkThreadTestConnector(CronetEngine cronetEngine) {
            mRequestContext = (CronetUrlRequestContext) cronetEngine;
            nativePrepareNetworkThread(mRequestContext.getUrlRequestContextAdapter());
        }

        public void shutdown() {
            nativeCleanupNetworkThread(mRequestContext.getUrlRequestContextAdapter());
        }
    }

    /**
     * Returns the value of load flags in |urlRequest|.
     * @param urlRequest is the UrlRequest object of interest.
     */
    public static int getLoadFlags(UrlRequest urlRequest) {
        return nativeGetLoadFlags(((CronetUrlRequest) urlRequest).getUrlRequestAdapterForTesting());
    }

    public static void setMockCertVerifierForTesting(
            ExperimentalCronetEngine.Builder builder, long mockCertVerifier) {
        getCronetEngineBuilderImpl(builder).setMockCertVerifierForTesting(mockCertVerifier);
    }

    public static CronetEngineBuilderImpl getCronetEngineBuilderImpl(
            ExperimentalCronetEngine.Builder builder) {
        return (CronetEngineBuilderImpl) builder.getBuilderDelegate();
    }

    /**
     * Returns whether the device supports calling nativeGetTaggedBytes().
     */
    public static native boolean nativeCanGetTaggedBytes();

    /**
     * Query the system to find out how many bytes were received with tag
     * {@code expectedTag} for our UID.
     * @param expectedTag the tag to query for.
     * @return the count of recieved bytes.
     */
    public static native long nativeGetTaggedBytes(int expectedTag);

    private static native int nativeGetLoadFlags(long urlRequestAdapter);

    private static native void nativePrepareNetworkThread(long contextAdapter);
    private static native void nativeCleanupNetworkThread(long contextAdapter);
}

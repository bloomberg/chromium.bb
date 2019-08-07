// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.annotations.CalledByNativeUnchecked;
import org.chromium.base.annotations.JNINamespace;

import java.util.concurrent.Executor;

/**
 * Manages proxy override functionality in WebView.
 */
@JNINamespace("android_webview")
public class AwProxyController {
    public AwProxyController() {}

    public String setProxyOverride(
            String[][] proxyRules, String[] bypassRules, Runnable listener, Executor executor) {
        int length = (proxyRules == null ? 0 : proxyRules.length);
        String[] urlSchemes = new String[length];
        String[] proxyUrls = new String[length];
        for (int i = 0; i < length; i++) {
            // URL schemes
            if (proxyRules[i][0] == null) {
                urlSchemes[i] = "*";
            } else {
                urlSchemes[i] = proxyRules[i][0];
            }
            // proxy URLs
            proxyUrls[i] = proxyRules[i][1];
            if (proxyUrls[i] == null) {
                return "Proxy rule " + i + " has a null url";
            }
        }
        length = (bypassRules == null ? 0 : bypassRules.length);
        for (int i = 0; i < length; i++) {
            if (bypassRules[i] == null) {
                return "Bypass rule " + i + " is null";
            }
        }
        if (executor == null) {
            return "Executor must not be null";
        }

        return nativeSetProxyOverride(urlSchemes, proxyUrls, bypassRules, listener, executor);
    }

    public String clearProxyOverride(Runnable listener, Executor executor) {
        if (executor == null) {
            return "Executor must not be null";
        }

        nativeClearProxyOverride(listener, executor);
        return "";
    }

    @CalledByNativeUnchecked
    private void proxyOverrideChanged(Runnable listener, Executor executor) {
        if (listener == null) return;
        executor.execute(listener);
    }

    private native String nativeSetProxyOverride(String[] urlSchemes, String[] proxyUrls,
            String[] bypassRules, Runnable listener, Executor executor);
    private native void nativeClearProxyOverride(Runnable listener, Executor executor);
}

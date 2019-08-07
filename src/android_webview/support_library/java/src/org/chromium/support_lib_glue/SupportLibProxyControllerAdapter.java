// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import org.chromium.android_webview.AwProxyController;
import org.chromium.android_webview.WebViewChromiumRunQueue;
import org.chromium.base.ThreadUtils;
import org.chromium.support_lib_boundary.ProxyControllerBoundaryInterface;

import java.util.concurrent.Executor;

/**
 * Adapter between AwProxyController and ProxyControllerBoundaryInterface.
 */
public class SupportLibProxyControllerAdapter implements ProxyControllerBoundaryInterface {
    private final WebViewChromiumRunQueue mRunQueue;
    private final AwProxyController mProxyController;

    public SupportLibProxyControllerAdapter(
            WebViewChromiumRunQueue runQueue, AwProxyController proxyController) {
        mRunQueue = runQueue;
        mProxyController = proxyController;
    }

    @Override
    public void setProxyOverride(
            String[][] proxyRules, String[] bypassRules, Runnable listener, Executor executor) {
        String result;
        if (checkNeedsPost()) {
            result = mRunQueue.runOnUiThreadBlocking(() -> {
                return mProxyController.setProxyOverride(
                        proxyRules, bypassRules, listener, executor);
            });
        } else {
            result = mProxyController.setProxyOverride(proxyRules, bypassRules, listener, executor);
        }
        if (!result.isEmpty()) {
            throw new IllegalArgumentException(result);
        }
    }

    @Override
    public void clearProxyOverride(Runnable listener, Executor executor) {
        String result;
        if (checkNeedsPost()) {
            result = mRunQueue.runOnUiThreadBlocking(
                    () -> { return mProxyController.clearProxyOverride(listener, executor); });
        } else {
            result = mProxyController.clearProxyOverride(listener, executor);
        }
        if (!result.isEmpty()) {
            throw new IllegalArgumentException(result);
        }
    }

    private static boolean checkNeedsPost() {
        return !ThreadUtils.runningOnUiThread();
    }
}

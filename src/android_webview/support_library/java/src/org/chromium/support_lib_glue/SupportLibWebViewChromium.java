// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.net.Uri;
import android.webkit.WebChromeClient;
import android.webkit.WebViewClient;

import com.android.webview.chromium.SharedWebViewChromium;
import com.android.webview.chromium.SharedWebViewRendererClientAdapter;

import org.chromium.android_webview.AwContents;
import org.chromium.support_lib_boundary.VisualStateCallbackBoundaryInterface;
import org.chromium.support_lib_boundary.WebMessageBoundaryInterface;
import org.chromium.support_lib_boundary.WebViewProviderBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;

import java.lang.reflect.InvocationHandler;

/**
 * Support library glue version of WebViewChromium.
 *
 * A new instance of this class is created transiently for every shared library
 * WebViewCompat call. Do not store state here.
 */
class SupportLibWebViewChromium implements WebViewProviderBoundaryInterface {
    private final SharedWebViewChromium mSharedWebViewChromium;

    public SupportLibWebViewChromium(SharedWebViewChromium sharedWebViewChromium) {
        mSharedWebViewChromium = sharedWebViewChromium;
    }

    @Override
    public void insertVisualStateCallback(long requestId, InvocationHandler callbackInvoHandler) {
        final VisualStateCallbackBoundaryInterface visualStateCallback =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        VisualStateCallbackBoundaryInterface.class, callbackInvoHandler);

        mSharedWebViewChromium.insertVisualStateCallback(
                requestId, new AwContents.VisualStateCallback() {
                    @Override
                    public void onComplete(long requestId) {
                        visualStateCallback.onComplete(requestId);
                    }
                });
    }

    @Override
    public /* WebMessagePort */ InvocationHandler[] createWebMessageChannel() {
        return SupportLibWebMessagePortAdapter.fromMessagePorts(
                mSharedWebViewChromium.createWebMessageChannel());
    }

    @Override
    public void postMessageToMainFrame(
            /* WebMessage */ InvocationHandler message, Uri targetOrigin) {
        WebMessageBoundaryInterface messageBoundaryInterface =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        WebMessageBoundaryInterface.class, message);
        mSharedWebViewChromium.postMessageToMainFrame(messageBoundaryInterface.getData(),
                targetOrigin.toString(),
                SupportLibWebMessagePortAdapter.toMessagePorts(
                        messageBoundaryInterface.getPorts()));
    }

    @Override
    public WebViewClient getWebViewClient() {
        return mSharedWebViewChromium.getWebViewClient();
    }

    @Override
    public WebChromeClient getWebChromeClient() {
        return mSharedWebViewChromium.getWebChromeClient();
    }

    @Override
    public /* WebViewRenderer */ InvocationHandler getWebViewRenderer() {
        return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                new SupportLibWebViewRendererAdapter(mSharedWebViewChromium.getRenderProcess()));
    }

    @Override
    public /* WebViewRendererClient */ InvocationHandler getWebViewRendererClient() {
        SharedWebViewRendererClientAdapter webViewRendererClientAdapter =
                mSharedWebViewChromium.getWebViewRendererClientAdapter();
        return webViewRendererClientAdapter != null
                ? webViewRendererClientAdapter.getSupportLibInvocationHandler()
                : null;
    }

    @Override
    public void setWebViewRendererClient(
            /* WebViewRendererClient */ InvocationHandler webViewRendererClient) {
        mSharedWebViewChromium.setWebViewRendererClientAdapter(webViewRendererClient != null
                        ? new SupportLibWebViewRendererClientAdapter(webViewRendererClient)
                        : null);
    }
}

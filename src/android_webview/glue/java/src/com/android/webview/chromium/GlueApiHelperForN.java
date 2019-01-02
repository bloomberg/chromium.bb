// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.TargetApi;
import android.os.Build;
import android.webkit.ServiceWorkerController;
import android.webkit.TokenBindingService;

import org.chromium.base.annotations.DoNotInline;

/**
 * Utility class to use new APIs that were added in N (API level 24). These need to exist in a
 * separate class so that Android framework can successfully verify glue layer classes without
 * encountering the new APIs. Note that GlueApiHelper is only for APIs that cannot go to ApiHelper
 * in base/, for reasons such as using system APIs or instantiating an adapter class that is
 * specific to glue layer.
 */
@DoNotInline
@TargetApi(Build.VERSION_CODES.N)
public final class GlueApiHelperForN {
    private GlueApiHelperForN() {}

    /**
     * See {@link
     * ServiceWorkerControllerAdapter#ServiceWorkerControllerAdapter(AwServiceWorkerController)},
     * which was added in N.
     */
    public static ServiceWorkerController createServiceWorkerControllerAdapter(
            WebViewChromiumAwInit awInit) {
        return new ServiceWorkerControllerAdapter(awInit.getServiceWorkerController());
    }

    /**
     * See {@link
     * TokenBindingManagerAdapter#TokenBindingManagerAdapter(WebViewChromiumFactoryProvider)}, which
     * was added in N.
     */
    public static TokenBindingService createTokenBindingManagerAdapter(
            WebViewChromiumFactoryProvider factory) {
        return new TokenBindingManagerAdapter(factory);
    }
}

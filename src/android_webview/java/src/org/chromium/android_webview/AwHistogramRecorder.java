// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.support.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Collect information about callbacks in Android WebView.
 */
public class AwHistogramRecorder {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({WebViewCallbackType.ON_RECEIVED_LOGIN_REQUEST,
            WebViewCallbackType.ON_RECEIVED_CLIENT_CERT_REQUEST,
            WebViewCallbackType.ON_RECEIVED_HTTP_AUTH_REQUEST,
            WebViewCallbackType.ON_DOWNLOAD_START})
    @interface WebViewCallbackType {
        // These values are used for UMA. Don't reuse or reorder values.
        // If you add something, update NUM_ENTRIES.
        int ON_RECEIVED_LOGIN_REQUEST = 0;
        int ON_RECEIVED_CLIENT_CERT_REQUEST = 1;
        int ON_RECEIVED_HTTP_AUTH_REQUEST = 2;
        int ON_DOWNLOAD_START = 3;
        int NUM_ENTRIES = 4;
    }

    // not meant to be instantiated
    private AwHistogramRecorder() {}

    public static void recordCallbackInvocation(@WebViewCallbackType int result) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.Callback.Counts", result, WebViewCallbackType.NUM_ENTRIES);
    }
}

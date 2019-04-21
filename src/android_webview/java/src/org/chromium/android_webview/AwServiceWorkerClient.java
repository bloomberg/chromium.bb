// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;

/**
 * Abstract base class that implementors of service worker related callbacks
 * derive from.
 */
public abstract class AwServiceWorkerClient {

    public abstract AwWebResourceResponse shouldInterceptRequest(AwWebResourceRequest request);

    // TODO: add support for onReceivedError and onReceivedHttpError callbacks.
}
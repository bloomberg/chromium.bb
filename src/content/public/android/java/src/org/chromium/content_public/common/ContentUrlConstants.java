// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.common;

/**
 * URL constants used by both Chrome, WebLayer and WebView.
 */
public final class ContentUrlConstants {
    public static final String ABOUT_SCHEME = "about";

    public static final String ABOUT_URL_SHORT_PREFIX = "about:";

    public static final String ABOUT_BLANK_DISPLAY_URL = "about:blank";
    public static final String ABOUT_BLANK_URL = "about://blank";

    public static final String FILE_URL_PREFIX = "file://";

    public static final String DATA_SCHEME = "data";
    public static final String HTTP_SCHEME = "http";
    public static final String HTTPS_SCHEME = "https";

    private ContentUrlConstants() {}
}

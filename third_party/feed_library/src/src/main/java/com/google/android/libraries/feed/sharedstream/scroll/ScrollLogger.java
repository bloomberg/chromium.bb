// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package com.google.android.libraries.feed.sharedstream.scroll;

import com.google.android.libraries.feed.api.host.logging.BasicLoggingApi;
import com.google.android.libraries.feed.api.host.logging.ScrollType;

/** */
public class ScrollLogger {
    private final BasicLoggingApi api;
    // We don't want to log scrolls that are tiny since the user probably didn't mean to actually
    // scroll.
    private static final int SCROLL_TOLERANCE = 10;

    public ScrollLogger(BasicLoggingApi api) {
        this.api = api;
    }
    /** Handles logging of scrolling. */
    public void handleScroll(@ScrollType int scrollType, int scrollAmount) {
        if (Math.abs(scrollAmount) <= SCROLL_TOLERANCE) {
            return;
        }
        api.onScroll(scrollType, scrollAmount);
    }
}

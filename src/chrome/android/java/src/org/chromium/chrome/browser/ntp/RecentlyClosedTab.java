// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

/**
 * Represents a recently closed tab.
 */
public class RecentlyClosedTab {
    public final int id;
    public final String title;
    public final String url;

    public RecentlyClosedTab(int id, String title, String url) {
        this.id = id;
        this.title = title;
        this.url = url;
    }
}

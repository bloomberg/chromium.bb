// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.native_page.BasicNativePage;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.touchless.R;

/**
 * Condensed new tab page for touchless devices.
 */
public class TouchlessNewTabPage extends BasicNativePage {
    private ViewGroup mView;
    private String mTitle;

    public TouchlessNewTabPage(ChromeActivity activity, NativePageHost host) {
        super(activity, host);
    }

    @Override
    protected void initialize(ChromeActivity activity, NativePageHost host) {
        mView = (ViewGroup) activity.getLayoutInflater().inflate(
                R.layout.new_tab_page_touchless, null);
        mTitle = activity.getResources().getString(R.string.button_new_tab);
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public String getHost() {
        return UrlConstants.NTP_HOST;
    }

    @Override
    public String getTitle() {
        return mTitle;
    }
}
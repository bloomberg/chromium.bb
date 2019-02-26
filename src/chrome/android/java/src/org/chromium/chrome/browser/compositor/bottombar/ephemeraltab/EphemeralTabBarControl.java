// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.ephemeraltab;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelInflater;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Top control used for ephemeral tab.
 */
public class EphemeralTabBarControl extends OverlayPanelInflater {
    /**
     * The tab title View.
     */
    private TextView mBarText;

    /**
     * @param panel The panel.
     * @param context The Android Context used to inflate the View.
     * @param container The container View used to inflate the View.
     * @param resourceLoader The resource loader that will handle the snapshot capturing.
     */
    public EphemeralTabBarControl(OverlayPanel panel, Context context, ViewGroup container,
            DynamicResourceLoader resourceLoader) {
        super(panel, R.layout.ephemeral_tab_text_view, R.id.ephemeral_tab_text_view, context,
                container, resourceLoader);
        invalidate();
    }

    /**
     * Set the text in the panel.
     * @param text The string to set the text to.
     */
    public void setBarText(String text) {
        inflate();
        mBarText.setText(text);
        invalidate();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        View view = getView();
        mBarText = (TextView) view.findViewById(R.id.ephemeral_tab_text);
    }
}

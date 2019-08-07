// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless.ui.iph;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.FrameLayout;
import android.widget.ImageView;

import org.chromium.chrome.browser.touchless.ui.tooltip.TooltipView;
import org.chromium.chrome.touchless.R;

/**
 * View responsible for displaying key functions IPH.
 */
public class KeyFunctionsIPHView extends FrameLayout {
    private TooltipView mTooltipView;
    private ImageView mNavigationModeImageView;

    public KeyFunctionsIPHView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public KeyFunctionsIPHView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mNavigationModeImageView = findViewById(R.id.navigation_mode_image);
    }

    void setTooltipView(TooltipView hostView) {
        mTooltipView = hostView;
    }

    void show() {
        mTooltipView.show(this);
    }

    void setCursorVisibility(boolean isCursorVisible) {
        mNavigationModeImageView.setImageResource(isCursorVisible
                        ? R.drawable.ic_spatial_navigation
                        : R.drawable.ic_cursor_navigation);
    }

    void hide() {
        mTooltipView.hide();
    }
}

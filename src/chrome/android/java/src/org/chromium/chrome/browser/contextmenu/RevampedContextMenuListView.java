// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.ListView;

import org.chromium.chrome.R;

/**
 * A custom ListView to be able to set width and height using the contents. Width and height are
 * constrained to make sure the view fits the screen size with margins.
 */
public class RevampedContextMenuListView extends ListView {
    public RevampedContextMenuListView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        widthMeasureSpec = MeasureSpec.makeMeasureSpec(calculateWidth(), MeasureSpec.EXACTLY);
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * The value returned by this method is used to set the width of the context menu in {@link
     * onMeasure()}
     *
     * @return The width of the context menu in pixels
     */
    private int calculateWidth() {
        int windowWidthPx = getResources().getDisplayMetrics().widthPixels;
        int maxWidth = getResources().getDimensionPixelSize(R.dimen.context_menu_max_width);
        int lateralMargin =
                getResources().getDimensionPixelSize(R.dimen.revamped_context_menu_lateral_margin);

        return Math.min(maxWidth, windowWidthPx - 2 * lateralMargin);
    }
}

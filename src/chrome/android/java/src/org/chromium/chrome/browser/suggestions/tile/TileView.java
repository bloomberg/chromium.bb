// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.FrameLayout;
import android.widget.ImageView;

import org.chromium.chrome.R;

/**
 * The view for a tile with icon and text.
 *
 * Displays the title of the site beneath a large icon.
 */
public class TileView extends FrameLayout {
    private ImageView mBadgeView;
    protected ImageView mIconView;

    /**
     * Constructor for inflating from XML.
     */
    public TileView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mIconView = findViewById(R.id.tile_view_icon);
        mBadgeView = findViewById(R.id.offline_badge);
    }

    /**
     * Initializes the view. This should be called immediately after inflation.
     *
     * @param showOfflineBadge Whether to show the offline badge.
     * @param icon The icon to display on the tile.
     */
    public void initialize(boolean showOfflineBadge, Drawable icon) {
        setOfflineBadgeVisibility(showOfflineBadge);
        setIconDrawable(icon);
    }

    /**
     * Renders the icon or clears it from the view if the icon is null.
     */
    public void setIconDrawable(Drawable icon) {
        mIconView.setImageDrawable(icon);
    }

    /** Shows or hides the offline badge to reflect the offline availability. */
    public void setOfflineBadgeVisibility(boolean showOfflineBadge) {
        mBadgeView.setVisibility(showOfflineBadge ? VISIBLE : GONE);
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.content.Context;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.View;

/**
 * Custom layout manager for site suggestions carousel. Handles custom effects such as
 * layout such that items at the edges are smaller than the center item and keeping focus
 * to the middle of the view.
 */
class SiteSuggestionsLayoutManager extends LinearLayoutManager {
    SiteSuggestionsLayoutManager(Context context) {
        super(context, LinearLayoutManager.HORIZONTAL, /* reverseLayout= */ false);
    }

    @Override
    public boolean onRequestChildFocus(
            RecyclerView parent, RecyclerView.State state, View child, View focused) {
        if (focused != null) {
            // Gets the adapter position of the child.
            int position = getPosition(child);
            // Calculates the offset for the child's left edge such that the child would be
            // perfectly centered in the view.
            int childHalfWidth = child.getWidth() / 2 + getLeftDecorationWidth(child);
            int offset = parent.getWidth() / 2 - childHalfWidth;
            // Scroll to child with calculated offset.
            scrollToPositionWithOffset(position, offset);
        }
        return true;
    }
}

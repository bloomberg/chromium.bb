// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.content.Context;
import android.util.AttributeSet;

import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;

/**
 * Provides a TouchlessLayoutManager to the SuggestionsRecyclerView. This facilitates restoring
 * focus.
 */
public class TouchlessRecyclerView extends SuggestionsRecyclerView {
    public TouchlessRecyclerView(Context context, AttributeSet attrs) {
        super(context, attrs, new TouchlessLayoutManager(context));
    }

    public TouchlessLayoutManager getTouchlessLayoutManager() {
        return (TouchlessLayoutManager) getLinearLayoutManager();
    }
}
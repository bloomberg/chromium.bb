// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import org.chromium.chrome.browser.metrics.ImpressionTracker.Listener;

/**
 * Filters {@link ImpressionTracker} impressions to only forward the first impression.
 * This can be useful to record whether a given view has ever been seen.
 */
public class OneShotImpressionListener implements Listener {
    private final Listener mListener;
    private boolean mTriggered;

    public OneShotImpressionListener(Listener listener) {
        mListener = listener;
    }

    @Override
    public void onImpression() {
        if (mTriggered) return;
        mTriggered = true;
        mListener.onImpression();
    }

    /**
     * Resets this object, so subsequent impressions will be forwarded to its listener again.
     */
    public void reset() {
        mTriggered = false;
    }
}

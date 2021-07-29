// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;

/**
 * A special linear layout that limits its maximum size to always stay below the Chrome navigation
 * bar.
 */
public class AssistantRootViewContainer
        extends LinearLayout implements BrowserControlsStateProvider.Observer {
    private final Activity mActivity;
    private BrowserControlsStateProvider mBrowserControlsStateProvider;
    private Rect mVisibleViewportRect = new Rect();
    private float mTalkbackSheetSizeFraction;
    private boolean mTalkbackResizingDisabled;

    public AssistantRootViewContainer(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        mActivity = ContextUtils.activityFromContext(context);
    }

    /** Initializes the object with the given {@link BrowserControlsStateProvider}. */
    public void initialize(@NonNull BrowserControlsStateProvider browserControlsStateProvider) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mBrowserControlsStateProvider.addObserver(this);
    }

    public void setTalkbackViewSizeFraction(float fraction) {
        mTalkbackSheetSizeFraction = fraction;
    }

    @Override
    public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
            int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
        invalidate();
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        invalidate();
    }

    @Override
    public void onAndroidVisibilityChanged(int visibility) {
        // TODO(crbug/1223069): Remove this workaround for default method desugaring in D8 causing
        // AbstractMethodErrors in some cases once fixed upstream.
    }

    public void disableTalkbackViewResizing() {
        mTalkbackResizingDisabled = true;
    }

    void destroy() {
        if (mBrowserControlsStateProvider != null) {
            mBrowserControlsStateProvider.removeObserver(this);
        }
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        mActivity.getWindow().getDecorView().getWindowVisibleDisplayFrame(mVisibleViewportRect);
        int browserControlsOffset = mBrowserControlsStateProvider == null
                ? 0
                : -mBrowserControlsStateProvider.getContentOffset()
                        - mBrowserControlsStateProvider.getBottomControlsHeight()
                        - mBrowserControlsStateProvider.getBottomControlOffset();
        int availableHeight = mVisibleViewportRect.height() - browserControlsOffset;

        int targetHeight;
        int mode;
        if (ChromeAccessibilityUtil.get().isAccessibilityEnabled() && !mTalkbackResizingDisabled) {
            // TODO(b/143944870): Make this more stable with landscape mode.
            targetHeight = (int) (availableHeight * mTalkbackSheetSizeFraction);
            mode = MeasureSpec.EXACTLY;
        } else {
            targetHeight = Math.min(MeasureSpec.getSize(heightMeasureSpec), availableHeight);
            mode = MeasureSpec.AT_MOST;
        }
        super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(targetHeight, mode));
    }
}

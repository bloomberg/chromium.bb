// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.widget.FrameLayout;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;

/**
 * FrameLayout that supports side-wise slide gesture for history navigation. Inheriting
 * class may need to override {@link #wasLastSideSwipeGestureConsumed()} if
 * {@link #onTouchEvent} cannot be relied upon to know whether the side-swipe related
 * event was handled. Namely {@link android.support.v7.widget.RecyclerView}) always
 * claims to handle touch events.
 */
public class HistoryNavigationLayout extends FrameLayout {
    private final boolean mDelegateSwipes;
    private final boolean mNavigationEnabled;
    private GestureDetector mDetector;
    private NavigationHandler mNavigationHandler;

    public HistoryNavigationLayout(Context context) {
        this(context, null);
    }

    public HistoryNavigationLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mDelegateSwipes = ChromeFeatureList.isEnabled(ChromeFeatureList.DELEGATE_OVERSCROLL_SWIPES);
        mNavigationEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.OVERSCROLL_HISTORY_NAVIGATION)
                && (context instanceof ChromeActivity);
    }

    /**
     * Sets the tab which the native page for this layout is running on.
     * @param tab Tab instance.
     * TODO(jinsukkim): Look into replacing tab with an interface which will make this a pure
     *     UI class. Then navigation tests will also be a pure UI one using a dummy activity.
     */
    public void setTab(Tab tab) {
        if (!mNavigationEnabled) return;
        mDetector = new GestureDetector(getContext(), new SideNavGestureListener());
        mNavigationHandler = new NavigationHandler(this, () -> tab);
    }

    @Override
    public void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        // TODO(jinsukkim): There are callsites enabling HistoryNavigationLayout but
        //         failing to call |setNavigationDelegate| (or |setTab| before renaming).
        //         Find when it can happen.
        if (mNavigationHandler != null) mNavigationHandler.reset();
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent e) {
        if (mDetector != null) {
            mDetector.onTouchEvent(e);
            mNavigationHandler.onTouchEvent(e.getAction());
        }
        return super.dispatchTouchEvent(e);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent e) {
        // Do not propagate touch events down to children if navigation UI was triggered.
        if (mDetector != null && !mDelegateSwipes && mNavigationHandler.isActive()) return true;
        return super.onInterceptTouchEvent(e);
    }

    private class SideNavGestureListener extends GestureDetector.SimpleOnGestureListener {
        @Override
        public boolean onDown(MotionEvent event) {
            return mNavigationHandler.onDown();
        }

        @Override
        public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
            // |onScroll| needs handling only after the state moves away from |NONE|. This helps
            // invoke |wasLastSideSwipeGestureConsumed| which may be expensive less often.
            if (mNavigationHandler.isStopped()) return true;

            if (mDelegateSwipes && wasLastSideSwipeGestureConsumed()) {
                mNavigationHandler.reset();
                return true;
            }
            return mNavigationHandler.onScroll(
                    e1.getX(), distanceX, distanceY, e2.getX(), e2.getY());
        }
    }

    /**
     * Checks if the gesture event was consumed by one of children views, in which case
     * history navigation should not proceed. Whatever the child view does with the gesture
     * events should take precedence and not be disturbed by the navigation.
     *
     * @return {@code true} if gesture event is consumed by one of the children.
     */
    public boolean wasLastSideSwipeGestureConsumed() {
        return false;
    }

    /**
     * Cancel navigation UI with animation effect.
     */
    public void release() {
        if (mNavigationHandler != null) mNavigationHandler.release(false);
    }
}

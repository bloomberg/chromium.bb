// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.support.annotation.IntDef;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import org.chromium.base.Supplier;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.tab.Tab;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Handles history overscroll navigation controlling the underlying UI widget.
 */
public class NavigationHandler {
    // Width of a rectangluar area in dp on the left/right edge used for navigation.
    // Swipe beginning from a point within these rects triggers the operation.
    @VisibleForTesting
    static final float EDGE_WIDTH_DP = 48;

    // Weighted value to determine when to trigger an edge swipe. Initial scroll
    // vector should form 30 deg or below to initiate swipe action.
    private static final float WEIGTHED_TRIGGER_THRESHOLD = 1.73f;

    // |EDGE_WIDTH_DP| in physical pixel.
    private final float mEdgeWidthPx;

    @IntDef({GestureState.NONE, GestureState.STARTED, GestureState.DRAGGED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface GestureState {
        int NONE = 0;
        int STARTED = 1;
        int DRAGGED = 2;
    }

    private final ViewGroup mParentView;
    private final Supplier<Tab> mCurrentTab;

    private @GestureState int mState = GestureState.NONE;

    // Frame layout where the main logic turning the gesture into corresponding UI resides.
    private SideSlideLayout mSideSlideLayout;

    // Async runnable for ending the refresh animation after the page first
    // loads a frame. This is used to provide a reasonable minimum animation time.
    private Runnable mStopNavigatingRunnable;

    // Handles removing the layout from the view hierarchy.  This is posted to ensure
    // it does not conflict with pending Android draws.
    private Runnable mDetachLayoutRunnable;

    public NavigationHandler(ViewGroup parentView, Supplier<Tab> tabProvider) {
        mParentView = parentView;
        mCurrentTab = tabProvider;
        mEdgeWidthPx = EDGE_WIDTH_DP * parentView.getResources().getDisplayMetrics().density;
    }

    private void createLayout() {
        mSideSlideLayout = new SideSlideLayout(mParentView.getContext());
        mSideSlideLayout.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mSideSlideLayout.setOnNavigationListener((forward) -> {
            Tab tab = mCurrentTab.get();
            if (forward) {
                tab.goForward();
            } else {
                // TODO(jinsukkim): Consider removing the direct dependency on Tab/Activity.
                tab.getActivity().onBackPressed();
            }
            cancelStopNavigatingRunnable();
            mSideSlideLayout.post(getStopNavigatingRunnable());
        });

        mSideSlideLayout.setOnResetListener(() -> {
            if (mDetachLayoutRunnable != null) return;
            mDetachLayoutRunnable = () -> {
                mDetachLayoutRunnable = null;
                detachLayoutIfNecessary();
            };
            mSideSlideLayout.post(mDetachLayoutRunnable);
        });
    }

    /**
     * @see View#onTouchEvent(MotionEvent)
     */
    public void onTouchEvent(int action) {
        if (action == MotionEvent.ACTION_UP) {
            if (mState == GestureState.DRAGGED && mSideSlideLayout != null) {
                mSideSlideLayout.release(true);
            }
        }
    }

    /**
     * @see GestureDetector#SimpleOnGestureListener#onDown(MotionEvent)
     */
    public boolean onDown() {
        mState = GestureState.STARTED;
        return true;
    }

    /**
     * Processes scroll event from {@link SimpleOnGestureListener#onScroll()}.
     * @param startX X coordinate of the position where gesture swipes from.
     * @param distanceX X delta between previous and current motion event.
     * @param distanceX Y delta between previous and current motion event.
     * @param endX X coordinate of the current motion event.
     * @param endY Y coordinate of the current motion event.
     */
    public boolean onScroll(
            float startX, float distanceX, float distanceY, float endX, float endY) {
        // onScroll needs handling only after the state moves away from |NONE|.
        if (mState == GestureState.NONE) return true;

        if (mState == GestureState.STARTED) {
            if (shouldTriggerUi(startX, distanceX, distanceY)) {
                boolean forward = distanceX > 0;
                if (canNavigate(forward) || !forward) {
                    showArrowWidget(forward);
                    mState = GestureState.DRAGGED;
                }
            }
            if (mState != GestureState.DRAGGED) mState = GestureState.NONE;
        }
        if (mState == GestureState.DRAGGED) mSideSlideLayout.pull(-distanceX);
        return true;
    }

    private boolean shouldTriggerUi(float sX, float dX, float dY) {
        return Math.abs(dX) > Math.abs(dY) * WEIGTHED_TRIGGER_THRESHOLD
                && (sX < mEdgeWidthPx || (mParentView.getWidth() - mEdgeWidthPx) < sX);
    }

    private boolean canNavigate(boolean forward) {
        Tab tab = mCurrentTab.get();
        return forward ? tab.canGoForward() : tab.canGoBack();
    }

    public void showArrowWidget(boolean forward) {
        if (mSideSlideLayout == null) createLayout();
        mSideSlideLayout.setEnabled(true);
        mSideSlideLayout.setDirection(forward);
        mSideSlideLayout.setEnableCloseIndicator(shouldShowCloseIndicator(forward));
        attachLayoutIfNecessary();
        mSideSlideLayout.start();
    }

    private boolean shouldShowCloseIndicator(boolean forward) {
        // Some tabs, upon back at the beginning of the history stack, should be just closed
        // than closing the entire app. In such case we do not show the close indicator.
        final boolean back = false;
        return !forward && !canNavigate(back) && willBackExitApp();
    }

    private boolean willBackExitApp() {
        boolean inTabbedMode = mCurrentTab.get().getActivity() instanceof ChromeTabbedActivity;
        return inTabbedMode ? !ChromeTabbedActivity.backShouldCloseTab(mCurrentTab.get()) : true;
    }

    /**
     * Signals a pull update.
     * @param delta The change in horizontal pull distance (positive if toward right,
     *         negative if left).
     */
    public void pull(float delta) {
        if (mSideSlideLayout != null) mSideSlideLayout.pull(delta);
    }

    /**
     * @return {@code true} if navigation was triggered, and its UI is in action.
     */
    public boolean isActive() {
        return mState == GestureState.DRAGGED;
    }

    /**
     * @return {@code true} if navigation is not in operation.
     */
    public boolean isStopped() {
        return mState == GestureState.NONE;
    }

    /**
     * Release the active pull. If no pull has started, the release will be ignored.
     * If the pull was sufficiently large, the navigation sequence will be initiated.
     * @param allowNav Whether to allow a sufficiently large pull to trigger
     *         the navigation action and animation sequence.
     */
    public void release(boolean allowNav) {
        if (mSideSlideLayout != null) {
            cancelStopNavigatingRunnable();
            mSideSlideLayout.release(allowNav);
        }
    }

    /**
     * Reset navigation UI in action.
     */
    public void reset() {
        if (mSideSlideLayout != null) {
            cancelStopNavigatingRunnable();
            mSideSlideLayout.reset();
        }
        mState = GestureState.NONE;
    }

    private void cancelStopNavigatingRunnable() {
        if (mStopNavigatingRunnable != null) {
            mSideSlideLayout.removeCallbacks(mStopNavigatingRunnable);
            mStopNavigatingRunnable = null;
        }
    }

    private void cancelDetachLayoutRunnable() {
        if (mDetachLayoutRunnable != null) {
            mSideSlideLayout.removeCallbacks(mDetachLayoutRunnable);
            mDetachLayoutRunnable = null;
        }
    }

    private Runnable getStopNavigatingRunnable() {
        if (mStopNavigatingRunnable == null) {
            mStopNavigatingRunnable = () -> mSideSlideLayout.stopNavigating();
        }
        return mStopNavigatingRunnable;
    }

    private void attachLayoutIfNecessary() {
        // The animation view is attached/detached on-demand to minimize overlap
        // with composited SurfaceView content.
        cancelDetachLayoutRunnable();
        if (mSideSlideLayout.getParent() == null) mParentView.addView(mSideSlideLayout);
    }

    private void detachLayoutIfNecessary() {
        if (mSideSlideLayout == null) return;
        cancelDetachLayoutRunnable();
        if (mSideSlideLayout.getParent() != null) mParentView.removeView(mSideSlideLayout);
    }
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.messages.snackbar;

import android.app.Activity;
import android.os.Handler;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.ui.base.WindowAndroid;

/**
 * Manager for the snackbar showing at the bottom of activity. There should be only one
 * SnackbarManager and one snackbar in the activity.
 * <p/>
 * When action button is clicked, this manager will call {@link SnackbarController#onAction(Object)}
 * in corresponding listener, and show the next entry. Otherwise if no action is taken by user
 * during {@link #DEFAULT_SNACKBAR_DURATION_MS} milliseconds, it will call
 * {@link SnackbarController#onDismissNoAction(Object)}. Note, snackbars of
 * {@link Snackbar#TYPE_PERSISTENT} do not get automatically dismissed after a timeout.
 */
public class SnackbarManager implements OnClickListener, ActivityStateListener {
    /**
     * Interface that shows the ability to provide a snackbar manager.
     */
    public interface SnackbarManageable {
        /**
         * @return The snackbar manager that has a proper anchor view.
         */
        SnackbarManager getSnackbarManager();
    }

    /**
     * Controller that post entries to snackbar manager and interact with snackbar manager during
     * dismissal and action click event.
     */
    public interface SnackbarController {
        /**
         * Called when the user clicks the action button on the snackbar.
         * @param actionData Data object passed when showing this specific snackbar.
         */
        default void onAction(Object actionData) {}

        /**
         * Called when the snackbar is dismissed by timeout or UI environment change.
         * @param actionData Data object associated with the dismissed snackbar entry.
         */
        default void onDismissNoAction(Object actionData) {}
    }

    public static final int DEFAULT_SNACKBAR_DURATION_MS = 3000;
    private static final int ACCESSIBILITY_MODE_SNACKBAR_DURATION_MS = 10000;

    // Used instead of the constant so tests can override the value.
    private static int sSnackbarDurationMs = DEFAULT_SNACKBAR_DURATION_MS;
    private static int sAccessibilitySnackbarDurationMs = ACCESSIBILITY_MODE_SNACKBAR_DURATION_MS;

    private Activity mActivity;
    private SnackbarView mView;
    private final Handler mUIThreadHandler;
    private SnackbarCollection mSnackbars = new SnackbarCollection();
    private boolean mActivityInForeground;
    private boolean mIsDisabledForTesting;
    private ViewGroup mSnackbarParentView;
    private final WindowAndroid mWindowAndroid;
    private final Runnable mHideRunnable = new Runnable() {
        @Override
        public void run() {
            mSnackbars.removeCurrentDueToTimeout();
            updateView();
        }
    };

    /**
     * Constructs a SnackbarManager to show snackbars in the given window.
     * @param activity The embedding activity.
     * @param snackbarParentView The ViewGroup used to display this snackbar.
     * @param windowAndroid The WindowAndroid used for starting animation. If it is null,
     *                      Animator#start is called instead.
     */
    public SnackbarManager(Activity activity, ViewGroup snackbarParentView,
            @Nullable WindowAndroid windowAndroid) {
        mActivity = activity;
        mUIThreadHandler = new Handler();
        mSnackbarParentView = snackbarParentView;
        mWindowAndroid = windowAndroid;

        ApplicationStatus.registerStateListenerForActivity(this, mActivity);
        if (ApplicationStatus.getStateForActivity(mActivity) == ActivityState.STARTED
                || ApplicationStatus.getStateForActivity(mActivity) == ActivityState.RESUMED) {
            onStart();
        }
    }

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        assert activity == mActivity;
        if (newState == ActivityState.STARTED) {
            onStart();
        } else if (newState == ActivityState.STOPPED) {
            onStop();
        }
    }

    /**
     * Notifies the snackbar manager that the activity is running in foreground now.
     */
    private void onStart() {
        mActivityInForeground = true;
    }

    /**
     * Notifies the snackbar manager that the activity has been pushed to background.
     */
    private void onStop() {
        mSnackbars.clear();
        updateView();
        mActivityInForeground = false;
    }

    /**
     * Shows a snackbar at the bottom of the screen, or above the keyboard if the keyboard is
     * visible.
     */
    public void showSnackbar(Snackbar snackbar) {
        if (!mActivityInForeground || mIsDisabledForTesting) return;
        RecordHistogram.recordSparseHistogram("Snackbar.Shown", snackbar.getIdentifier());

        mSnackbars.add(snackbar);
        updateView();
        mView.announceforAccessibility();
    }

    /** Dismisses all snackbars. */
    public void dismissAllSnackbars() {
        if (mSnackbars.isEmpty()) return;

        mSnackbars.clear();
        updateView();
    }

    /**
     * Dismisses snackbars that are associated with the given {@link SnackbarController}.
     *
     * @param controller Only snackbars with this controller will be removed.
     */
    public void dismissSnackbars(SnackbarController controller) {
        if (mSnackbars.removeMatchingSnackbars(controller)) {
            updateView();
        }
    }

    /**
     * Dismisses snackbars that have a certain controller and action data.
     *
     * @param controller Only snackbars with this controller will be removed.
     * @param actionData Only snackbars whose action data is equal to actionData will be removed.
     */
    public void dismissSnackbars(SnackbarController controller, Object actionData) {
        if (mSnackbars.removeMatchingSnackbars(controller, actionData)) {
            updateView();
        }
    }

    /**
     * Handles click event for action button at end of snackbar.
     */
    @Override
    public void onClick(View v) {
        mSnackbars.removeCurrentDueToAction();
        updateView();
    }

    /**
     * After an infobar is added, brings snackbar view above it.
     * TODO(crbug/1028382): Currently SnackbarManager doesn't observe InfobarContainer events.
     * Restore this functionality, only without references to Infobar classes.
     */
    public void onAddInfoBar() {
        // Bring Snackbars to the foreground so that it's not blocked by infobars.
        if (isShowing()) {
            mView.bringToFront();
        }
    }

    /**
     * Temporarily changes the parent {@link ViewGroup} of the snackbar. If a snackbar is currently
     * showing, this method removes the snackbar from its original parent, and attaches it to the
     * given parent. If <code>null</code> is given, the snackbar will be reattached to its original
     * parent.
     *
     * @param overridingParent The temporary parent of the snackbar. If null, previous calls of this
     *                         method will be reverted.
     */
    public void overrideParent(ViewGroup overridingParent) {
        if (mView != null) mView.overrideParent(overridingParent);
    }

    /**
     * @return Whether there is a snackbar on screen.
     */
    public boolean isShowing() {
        return mView != null && mView.isShowing();
    }

    /**
     * Updates the {@link SnackbarView} to reflect the value of mSnackbars.currentSnackbar(), which
     * may be null. This might show, change, or hide the view.
     */
    private void updateView() {
        if (!mActivityInForeground) return;
        Snackbar currentSnackbar = mSnackbars.getCurrent();
        if (currentSnackbar == null) {
            mUIThreadHandler.removeCallbacks(mHideRunnable);
            if (mView != null) {
                mView.dismiss();
                mView = null;
            }
        } else {
            boolean viewChanged = true;
            if (mView == null) {
                mView = new SnackbarView(
                        mActivity, this, currentSnackbar, mSnackbarParentView, mWindowAndroid);
                mView.show();
            } else {
                viewChanged = mView.update(currentSnackbar);
            }

            if (viewChanged) {
                mUIThreadHandler.removeCallbacks(mHideRunnable);
                if (!currentSnackbar.isTypePersistent()) {
                    int durationMs = getDuration(currentSnackbar);
                    mUIThreadHandler.postDelayed(mHideRunnable, durationMs);
                }
                mView.announceforAccessibility();
            }
        }
    }

    private int getDuration(Snackbar snackbar) {
        int durationMs = snackbar.getDuration();
        if (durationMs == 0) durationMs = sSnackbarDurationMs;

        if (AccessibilityUtil.isAccessibilityEnabled()) {
            durationMs *= 2;
            if (durationMs < sAccessibilitySnackbarDurationMs) {
                durationMs = sAccessibilitySnackbarDurationMs;
            }
        }

        return durationMs;
    }

    /**
     * Disables the snackbar manager. This is only intended for testing purposes.
     */
    @VisibleForTesting
    public void disableForTesting() {
        mIsDisabledForTesting = true;
    }

    /**
     * Overrides the default snackbar duration with a custom value for testing.
     * @param durationMs The duration to use in ms.
     */
    @VisibleForTesting
    public static void setDurationForTesting(int durationMs) {
        sSnackbarDurationMs = durationMs;
        sAccessibilitySnackbarDurationMs = durationMs;
    }

    /**
     * @return The currently showing snackbar. For testing only.
     */
    @VisibleForTesting
    public Snackbar getCurrentSnackbarForTesting() {
        return mSnackbars.getCurrent();
    }
}

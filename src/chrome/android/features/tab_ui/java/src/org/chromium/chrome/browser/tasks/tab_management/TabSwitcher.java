// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.support.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * Interface for the Tab Switcher.
 */
public interface TabSwitcher {
    /**
     * Defines an interface to pass out tab selecting event.
     */
    interface OnTabSelectingListener {
        /**
         * Called when a tab is getting selected. Typically when exiting the overview mode.
         * @param time  The current time of the app in ms.
         * @param tabId The ID of selected {@link Tab}.
         * @see Layout#onTabSelecting(long, int)
         */
        void onTabSelecting(long time, int tabId);
    }

    /**
     * Set the listener to get the {@link Layout#onTabSelecting} event from the Tab Switcher.
     * @param listener The {@link OnTabSelectingListener} to use.
     */
    void setOnTabSelectingListener(OnTabSelectingListener listener);

    // TODO(960196): Remove the following interfaces when the associated bug is resolved.
    /**
     * An observer that is notified when the TabSwitcher view state changes.
     */
    interface OverviewModeObserver {
        /**
         * Called when overview mode starts showing.
         */
        void startedShowing();

        /**
         * Called when overview mode finishes showing.
         */
        void finishedShowing();

        /**
         * Called when overview mode starts hiding.
         */
        void startedHiding();

        /**
         * Called when overview mode finishes hiding.
         */
        void finishedHiding();
    }

    /**
     * Interface to control the TabSwitcher.
     */
    interface Controller {
        /**
         * @return Whether or not the overview {@link Layout} is visible.
         */
        boolean overviewVisible();

        /**
         * @param listener Registers {@code listener} for overview mode status changes.
         */
        void addOverviewModeObserver(OverviewModeObserver listener);

        /**
         * @param listener Unregisters {@code listener} for overview mode status changes.
         */
        void removeOverviewModeObserver(OverviewModeObserver listener);

        /**
         * Hide the overview.
         * @param animate Whether we should animate while hiding.
         */
        void hideOverview(boolean animate);

        /**
         * Show the overview.
         * @param animate Whether we should animate while showing.
         */
        void showOverview(boolean animate);

        /**
         * Called by the StartSurfaceLayout when the system back button is pressed.
         * @return Whether or not the TabSwitcher consumed the event.
         */
        boolean onBackPressed();

        /**
         * Set the bottom control height to margin the bottom of the TabListRecyclerView.
         * @param bottomControlsHeight The bottom control height in pixel.
         */
        void setBottomControlsHeight(int bottomControlsHeight);
    }

    /**
     * @return Controller implementation that can be used for controlling
     *         visibility changes.
     */
    Controller getController();

    /**
     * Interface to access the Tab List.
     */
    interface TabListDelegate {
        /**
         * @return The dynamic resource ID of the TabSwitcher RecyclerView.
         */
        int getResourceId();

        /**
         * @return The timestamp of last dirty event of {@link ViewResourceAdapter} of
         * {@link TabListRecyclerView}.
         */
        long getLastDirtyTimeForTesting();

        /**
         * Before calling {@link Controller#showOverview} to start showing the
         * TabSwitcher {@link TabListRecyclerView}, call this to populate it without making it
         * visible.
         * @return Whether the {@link TabListRecyclerView} can be shown quickly.
         */
        boolean prepareOverview();

        /**
         * This is called after the compositor animation is done, for potential clean-up work.
         * {@link OverviewModeObserver#finishedHiding} happens after
         * the Android View animation, but before the compositor animation.
         */
        void postHiding();

        /**
         * @param forceUpdate Whether to measure the current location again. If not, return the last
         *                    location measured on last layout, which can be wrong after scrolling.
         * @return The {@link Rect} of the thumbnail of the current tab, relative to the
         *         TabSwitcher {@link TabListRecyclerView} coordinates.
         */
        @NonNull
        Rect getThumbnailLocationOfCurrentTab(boolean forceUpdate);

        /**
         * Set a hook to receive all the {@link Bitmap}s returned by
         * {@link TabListMediator.ThumbnailFetcher} for testing.
         * @param callback The callback to send bitmaps through.
         */
        @VisibleForTesting
        void setBitmapCallbackForTesting(Callback<Bitmap> callback);

        /**
         * @return The number of thumbnail fetching for testing.
         */
        @VisibleForTesting
        int getBitmapFetchCountForTesting();

        /**
         * @return The soft cleanup delay for testing.
         */
        @VisibleForTesting
        int getSoftCleanupDelayForTesting();

        /**
         * @return The cleanup delay for testing.
         */
        @VisibleForTesting
        int getCleanupDelayForTesting();
    }

    /**
     * @return The {@link TabListDelegate}.
     */
    TabListDelegate getTabListDelegate();
}

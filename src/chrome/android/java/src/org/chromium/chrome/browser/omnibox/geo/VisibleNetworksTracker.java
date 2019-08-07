// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import android.content.Context;
import android.os.SystemClock;
import android.support.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.task.AsyncTask;

/**
 * VisibleNetworksTracker keeps track of the visible networks.
 */
class VisibleNetworksTracker {
    private static final String TAG = "VNTracker";

    @VisibleForTesting
    static final int AGE_THRESHOLD = 5 * 60 * 1000; // 5 min

    @Nullable
    private static VisibleNetworks sVisibleNetworks;
    private static long sVisibleNetworksTime = Long.MAX_VALUE;

    @Nullable
    private static AsyncTask<Void> sOngoingRefresh;

    private static VisibleNetworks sVisibleNetworksForTesting;
    private static boolean sUseVisibleNetworksForTesting;

    /**
     * Returns last known visible networks. It returns the cached value if the cache is valid or it
     * computes the simplest possible visibleNetworks fast, and triggers a background asynchronous
     * refresh. Might return null if visible networks cannot be computed.
     */
    @Nullable
    static VisibleNetworks getLastKnownVisibleNetworks(final Context context) {
        if (sUseVisibleNetworksForTesting) return sVisibleNetworksForTesting;

        if (isValidCachedVisibleNetworks()) return getCachedVisibleNetworks();

        VisibleNetworks visibleNetworks = null;
        try {
            // Include only the connected cell/wifi to minimize latency and compute the simplest
            // visible networks possible.
            visibleNetworks = PlatformNetworksManager.computeConnectedNetworks(context);
        } catch (Exception e) {
            Log.e(TAG, "Failed to get the visible networks. Error: ", e.toString());
        }
        // Update cache asynchronously.
        refreshVisibleNetworks(context);

        return visibleNetworks;
    }

    /**
     * Determines if the visible networks need to be refreshed and asynchronously updates them if
     * needed.
     */
    static void refreshVisibleNetworks(final Context context) {
        ThreadUtils.assertOnUiThread();
        if (isValidCachedVisibleNetworks() || sOngoingRefresh != null) {
            return;
        }

        // TODO(crbug.com/955560): Cleanup this section to avoid wrapping a function that now
        // accepts a callback in an AsyncTask.
        sOngoingRefresh = new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                try {
                    // Include all visible wifis and cells.
                    PlatformNetworksManager.computeVisibleNetworks(
                            context, VisibleNetworksTracker::setCachedVisibleNetworks);
                } catch (Exception e) {
                    Log.e(TAG, "Failed to get the visible networks. Error: ", e.toString());
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void v) {
                sOngoingRefresh = null;
            }
        };
        sOngoingRefresh.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @Nullable
    @VisibleForTesting
    static VisibleNetworks getCachedVisibleNetworks() {
        return sVisibleNetworks;
    }

    @VisibleForTesting
    static long getCachedVisibleNetworksTime() {
        return sVisibleNetworksTime;
    }

    @VisibleForTesting
    static void clearCache() {
        setCachedVisibleNetworks(null);
        sVisibleNetworksTime = Long.MAX_VALUE;
    }

    @VisibleForTesting
    static void setVisibleNetworksForTesting(VisibleNetworks visibleNetworksForTesting) {
        sVisibleNetworksForTesting = visibleNetworksForTesting;
        sUseVisibleNetworksForTesting = true;
    }

    private static void setCachedVisibleNetworks(VisibleNetworks visibleNetworks) {
        ThreadUtils.assertOnUiThread();
        sVisibleNetworks = visibleNetworks;
        sVisibleNetworksTime = SystemClock.elapsedRealtime();
    }

    private static boolean isValidCachedVisibleNetworks() {
        return sVisibleNetworks != null && sVisibleNetworksTime != Long.MAX_VALUE
                && !sVisibleNetworks.isEmpty()
                && SystemClock.elapsedRealtime() - sVisibleNetworksTime < AGE_THRESHOLD;
    }
}

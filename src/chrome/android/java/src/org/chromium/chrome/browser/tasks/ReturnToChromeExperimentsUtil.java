// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import android.app.Activity;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;

import java.util.List;

/**
 * This is a utility class for managing experiments related to returning to Chrome.
 */
public final class ReturnToChromeExperimentsUtil {
    private static final String TAG = "TabSwitcherOnReturn";

    @VisibleForTesting
    public static final String TAB_SWITCHER_ON_RETURN_MS_PARAM = "tab_switcher_on_return_time_ms";
    public static final IntCachedFieldTrialParameter TAB_SWITCHER_ON_RETURN_MS =
            new IntCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_SWITCHER_ON_RETURN, TAB_SWITCHER_ON_RETURN_MS_PARAM, -1);

    @VisibleForTesting
    static final String UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT =
            "Startup.Android.TimeToGTSFirstMeaningfulPaint";

    private static final String UMA_THUMBNAIL_FETCHED_FOR_GTS_FIRST_MEANINGFUL_PAINT =
            "Startup.Android.ThumbnailFetchedForGTSFirstMeaningfulPaint";

    private static boolean sGTSFirstMeaningfulPaintRecorded;

    private ReturnToChromeExperimentsUtil() {}

    /**
     * Determine if we should show the tab switcher on returning to Chrome.
     *   Returns true if enough time has elapsed since the app was last backgrounded.
     *   The threshold time in milliseconds is set by experiment "enable-tab-switcher-on-return"
     *
     * @param lastBackgroundedTimeMillis The last time the application was backgrounded. Set in
     *                                   ChromeTabbedActivity::onStopWithNative
     * @return true if past threshold, false if not past threshold or experiment cannot be loaded.
     */
    public static boolean shouldShowTabSwitcher(final long lastBackgroundedTimeMillis) {
        int tabSwitcherAfterMillis = TAB_SWITCHER_ON_RETURN_MS.getValue();

        if (lastBackgroundedTimeMillis == -1) {
            // No last background timestamp set, use control behavior unless "immediate" was set.
            return tabSwitcherAfterMillis == 0;
        }

        if (tabSwitcherAfterMillis < 0) {
            // If no value for experiment, use control behavior.
            return false;
        }

        long expirationTime = lastBackgroundedTimeMillis + tabSwitcherAfterMillis;

        return System.currentTimeMillis() > expirationTime;
    }

    /**
     * Record the elapsed time from activity creation to first meaningful paint of Grid Tab
     * Switcher.
     * @param elapsedMs Elapsed time in ms.
     * @param numOfThumbnails Number of thumbnails fetched for the Grid Tab Switcher.
     */
    public static void recordTimeToGTSFirstMeaningfulPaint(long elapsedMs, int numOfThumbnails) {
        Log.i(TAG,
                UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT
                        + coldStartBucketName(!sGTSFirstMeaningfulPaintRecorded)
                        + numThumbnailsBucketName(numOfThumbnails) + ": " + numOfThumbnails
                        + " thumbnails " + elapsedMs + "ms");
        RecordHistogram.recordTimesHistogram(UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT
                        + coldStartBucketName(!sGTSFirstMeaningfulPaintRecorded)
                        + numThumbnailsBucketName(numOfThumbnails),
                elapsedMs);
        RecordHistogram.recordTimesHistogram(UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT
                        + coldStartBucketName(!sGTSFirstMeaningfulPaintRecorded),
                elapsedMs);
        RecordHistogram.recordTimesHistogram(UMA_TIME_TO_GTS_FIRST_MEANINGFUL_PAINT, elapsedMs);
        RecordHistogram.recordCount100Histogram(
                UMA_THUMBNAIL_FETCHED_FOR_GTS_FIRST_MEANINGFUL_PAINT, numOfThumbnails);
        sGTSFirstMeaningfulPaintRecorded = true;
    }

    @VisibleForTesting
    static String coldStartBucketName(boolean isColdStart) {
        if (isColdStart) return ".Cold";
        return ".Warm";
    }

    @VisibleForTesting
    static String numThumbnailsBucketName(int numOfThumbnails) {
        return "." + numThumbnailsBucket(numOfThumbnails) + "thumbnails";
    }

    /**
     * On Pixel 3 XL, at most 10 cards are fetched. Multi-thumbnail cards can have up to 4
     * thumbnails, so the maximum should be 40.
     */
    private static String numThumbnailsBucket(int numOfThumbnails) {
        if (numOfThumbnails == 0) return "0";
        if (numOfThumbnails <= 2) return "1~2";
        if (numOfThumbnails <= 5) return "3~5";
        if (numOfThumbnails <= 10) return "6~10";
        if (numOfThumbnails <= 20) return "11~20";
        return "20+";
    }

    /**
     * Check if we should handle the navigation. If so, create a new tab and load the URL.
     *
     * @param url The URL to load.
     * @param transition The page transition type.
     * @return true if we have handled the navigation, false otherwise.
     */
    public static boolean willHandleLoadUrlFromStartSurface(
            String url, @PageTransition int transition) {
        return willHandleLoadUrlWithPostDataFromStartSurface(url, transition, null, null);
    }

    /**
     * Check if we should handle the navigation. If so, create a new tab and load the URL with POST
     * data.
     *
     * @param url The URL to load.
     * @param transition The page transition type.
     * @param postDataType   postData type.
     * @param postData       POST data to include in the tab URL's request body, ex. bitmap when
     *         image search.
     * @return true if we have handled the navigation, false otherwise.
     */
    public static boolean willHandleLoadUrlWithPostDataFromStartSurface(String url,
            @PageTransition int transition, @Nullable String postDataType,
            @Nullable byte[] postData) {
        ChromeActivity chromeActivity = getActivityPresentingOverviewWithOmnibox();
        if (chromeActivity == null) return false;

        // Create a new unparented tab.
        TabModel model = chromeActivity.getCurrentTabModel();
        LoadUrlParams params = new LoadUrlParams(url);
        params.setTransitionType(transition | PageTransition.FROM_ADDRESS_BAR);
        if (!TextUtils.isEmpty(postDataType) && postData != null && postData.length != 0) {
            params.setVerbatimHeaders("Content-Type: " + postDataType);
            params.setPostData(ResourceRequestBody.createFromBytes(postData));
        }

        chromeActivity.getTabCreator(model.isIncognito())
                .createNewTab(params, TabLaunchType.FROM_START_SURFACE, null);

        if (transition == PageTransition.AUTO_BOOKMARK) {
            RecordUserAction.record("Suggestions.Tile.Tapped.GridTabSwitcher");
        } else {
            RecordUserAction.record("MobileOmniboxUse.GridTabSwitcher");

            // These are duplicated here but would have been recorded by LocationBarLayout#loadUrl.
            RecordUserAction.record("MobileOmniboxUse");
            LocaleManager.getInstance().recordLocaleBasedSearchMetrics(false, url, transition);
        }

        return true;
    }

    /**
     * @return Whether the Tab Switcher is showing the omnibox.
     */
    public static boolean isInOverviewWithOmnibox() {
        return getActivityPresentingOverviewWithOmnibox() != null;
    }

    /**
     * @return The ChromeActivity if it is presenting the omnibox on the tab switcher, else null.
     */
    private static ChromeActivity getActivityPresentingOverviewWithOmnibox() {
        if (!StartSurfaceConfiguration.isStartSurfaceEnabled()) return null;

        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (!(activity instanceof ChromeActivity)) return null;

        ChromeActivity chromeActivity = (ChromeActivity) activity;
        if (!chromeActivity.isInOverviewMode()) return null;

        return chromeActivity;
    }

    /**
     * TODO(crbug/1041865): avoid using GURL since {@link #shouldShowStartSurfaceAsTheHomePage()}
     *  is in the critical path in Instant Start.
     */
    private static boolean isNTPUrl(String url) {
        if (CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START)) {
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                return NewTabPage.isNTPUrl(url);
            }
        }
        return NewTabPage.isNTPUrl(url);
    }

    /**
     * Check whether we should show Start Surface as the home page. This is used for all cases
     * except initial tab creation, which uses {@link
     * #shouldShowStartSurfaceAsTheHomePageNoTabs()}.
     *
     * @return Whether Start Surface should be shown as the home page.
     */
    public static boolean shouldShowStartSurfaceAsTheHomePage() {
        return shouldShowStartSurfaceAsTheHomePageNoTabs()
                && !StartSurfaceConfiguration.START_SURFACE_OPEN_NTP_INSTEAD_OF_START.getValue();
    }

    /**
     * Check whether we should show Start Surface as the home page for initial tab creation.
     *
     * @return Whether Start Surface should be shown as the home page.
     */
    public static boolean shouldShowStartSurfaceAsTheHomePageNoTabs() {
        // When creating initial tab, i.e. cold start without restored tabs, we should only show
        // StartSurface as the HomePage if Single Pane is enabled, HomePage is not customized,
        // accessibility is not enabled and not on tablet.
        String homePageUrl = HomepageManager.getHomepageUri();
        return StartSurfaceConfiguration.isStartSurfaceSinglePaneEnabled()
                && (TextUtils.isEmpty(homePageUrl) || isNTPUrl(homePageUrl))
                && !AccessibilityUtil.isAccessibilityEnabled()
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                        ContextUtils.getApplicationContext());
    }

    /**
     * @param tabModelSelector The tab model selector.
     * @return the total tab count, and works before native initialization.
     */
    public static int getTotalTabCount(TabModelSelector tabModelSelector) {
        if (CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START)
                && !tabModelSelector.isTabStateInitialized()) {
            List<PseudoTab> allTabs;
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                allTabs = PseudoTab.getAllPseudoTabsFromStateFile();
            }
            return allTabs != null ? allTabs.size() : 0;
        }
        return tabModelSelector.getTotalTabCount();
    }
}

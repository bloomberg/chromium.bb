// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.StringCachedFieldTrialParameter;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Flag configuration for Start Surface. Source of truth for whether it should be enabled and
 * which variation should be used.
 */
public class StartSurfaceConfiguration {
    private static final String TAG = "StartSurfaceConfig";
    public static final StringCachedFieldTrialParameter START_SURFACE_VARIATION =
            new StringCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "start_surface_variation", "");
    public static final BooleanCachedFieldTrialParameter START_SURFACE_EXCLUDE_MV_TILES =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "exclude_mv_tiles", false);
    public static final BooleanCachedFieldTrialParameter
            START_SURFACE_HIDE_INCOGNITO_SWITCH_NO_TAB =
                    new BooleanCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                            "hide_switch_when_no_incognito_tabs", false);

    public static final BooleanCachedFieldTrialParameter START_SURFACE_LAST_ACTIVE_TAB_ONLY =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "show_last_active_tab_only", false);
    public static final BooleanCachedFieldTrialParameter START_SURFACE_OPEN_NTP_INSTEAD_OF_START =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, "open_ntp_instead_of_start", false);

    private static final String OMNIBOX_FOCUSED_ON_NEW_TAB_PARAM = "omnibox_focused_on_new_tab";
    public static final BooleanCachedFieldTrialParameter OMNIBOX_FOCUSED_ON_NEW_TAB =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                    OMNIBOX_FOCUSED_ON_NEW_TAB_PARAM, false);

    private static final String SHOW_NTP_TILES_ON_OMNIBOX_PARAM = "show_ntp_tiles_on_omnibox";
    public static final BooleanCachedFieldTrialParameter SHOW_NTP_TILES_ON_OMNIBOX =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                    SHOW_NTP_TILES_ON_OMNIBOX_PARAM, false);

    private static final String HOME_BUTTON_ON_GRID_TAB_SWITCHER_PARAM =
            "home_button_on_grid_tab_switcher";
    public static final BooleanCachedFieldTrialParameter HOME_BUTTON_ON_GRID_TAB_SWITCHER =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.START_SURFACE_ANDROID,
                    HOME_BUTTON_ON_GRID_TAB_SWITCHER_PARAM, false);

    private static final String NEW_SURFACE_PARAM = "new_home_surface_from_home_button";
    public static final StringCachedFieldTrialParameter NEW_SURFACE_FROM_HOME_BUTTON =
            new StringCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, NEW_SURFACE_PARAM, "");

    private static final String SHOW_TABS_IN_MRU_ORDER_PARAM = "show_tabs_in_mru_order";
    public static final BooleanCachedFieldTrialParameter SHOW_TABS_IN_MRU_ORDER =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, SHOW_TABS_IN_MRU_ORDER_PARAM, false);

    private static final String SUPPORT_ACCESSIBILITY_PARAM = "support_accessibility";
    public static final BooleanCachedFieldTrialParameter SUPPORT_ACCESSIBILITY =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.START_SURFACE_ANDROID, SUPPORT_ACCESSIBILITY_PARAM, true);

    private static final String STARTUP_UMA_PREFIX = "Startup.Android.";
    private static final String INSTANT_START_SUBFIX = ".Instant";
    private static final String REGULAR_START_SUBFIX = ".NoInstant";

    /**
     * @return Whether the Start Surface is enabled.
     * @Deprecated Use {@link ReturnToStartSurfaceUtil#isStartSurfaceHomepageEnabled()} instead.
     */
    public static boolean isStartSurfaceEnabled() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.START_SURFACE_ANDROID)
                && !SysUtils.isLowEndDevice();
    }

    /**
     * @return Whether the Start Surface SinglePane is enabled.
     */
    public static boolean isStartSurfaceSinglePaneEnabled() {
        return isStartSurfaceEnabled() && START_SURFACE_VARIATION.getValue().equals("single");
    }

    /**
     * @return the PageClassification type of the fake Omnibox on the Start surface homepage.
     */
    public static int getPageClassificationForHomepage() {
        // When NEW_SURFACE_FROM_HOME_BUTTON equals "hide_mv_tiles_and_tab_switcher", the MV (NTP)
        // tiles are removed from the Start surface homepage when tapping the home button. Thus, we
        // have to show MV (NTP) tiles when the fake omnibox get focusing, and there is no need to
        // check SHOW_NTP_TILES_ON_OMNIBOX anymore.
        return TextUtils.equals(
                       NEW_SURFACE_FROM_HOME_BUTTON.getValue(), "hide_mv_tiles_and_tab_switcher")
                ? PageClassification.START_SURFACE_HOMEPAGE_VALUE
                : PageClassification.NTP_VALUE;
    }

    /**
     * @return the PageClassification type of the new Tab.
     */
    public static int getPageClassificationForNewTab() {
        return SHOW_NTP_TILES_ON_OMNIBOX.getValue() ? PageClassification.START_SURFACE_NEW_TAB_VALUE
                                                    : PageClassification.NTP_VALUE;
    }

    /**
     * Add an observer to keep {@link ChromePreferenceKeys.FEED_ARTICLES_LIST_VISIBLE} consistent
     * with {@link Pref.ARTICLES_LIST_VISIBLE}.
     */
    public static void addFeedVisibilityObserver() {
        updateFeedVisibility();
        PrefChangeRegistrar prefChangeRegistrar = new PrefChangeRegistrar();
        prefChangeRegistrar.addObserver(
                Pref.ARTICLES_LIST_VISIBLE, StartSurfaceConfiguration::updateFeedVisibility);
    }

    private static void updateFeedVisibility() {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FEED_ARTICLES_LIST_VISIBLE,
                UserPrefs.get(Profile.getLastUsedRegularProfile())
                        .getBoolean(Pref.ARTICLES_LIST_VISIBLE));
    }

    /**
     * @return Whether the Feed articles are visible.
     */
    public static boolean getFeedArticlesVisibility() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.FEED_ARTICLES_LIST_VISIBLE, true);
    }

    /**
     * Records histograms of showing the StartSurface. Nothing will be recorded if timeDurationMs
     * isn't valid.
     */
    public static void recordHistogram(String name, long timeDurationMs, boolean isInstantStart) {
        if (timeDurationMs < 0) return;
        Log.i(TAG, "Recorded %s = %d ms", getHistogramName(name, isInstantStart), timeDurationMs);
        RecordHistogram.recordTimesHistogram(
                getHistogramName(name, isInstantStart), timeDurationMs);
    }

    @VisibleForTesting
    public static String getHistogramName(String name, boolean isInstantStart) {
        return STARTUP_UMA_PREFIX + name
                + (isInstantStart ? INSTANT_START_SUBFIX : REGULAR_START_SUBFIX);
    }

    /**
     * Returns whether the given Tab has the flag of focusing on its Omnibox on the first time the
     * Tab is showing, and resets the flag in the Tab's UserData. This function returns true only
     * when {@link OMNIBOX_FOCUSED_ON_NEW_TAB} is enabled.
     */
    public static boolean consumeFocusOnOmnibox(Tab tab, @LayoutType int layout) {
        if (tab != null && tab.getUrl().isEmpty() && layout == LayoutType.BROWSING
                && StartSurfaceUserData.getFocusOnOmnibox(tab)) {
            assert OMNIBOX_FOCUSED_ON_NEW_TAB.getValue();
            StartSurfaceUserData.setFocusOnOmnibox(tab, false);
            return true;
        }
        return false;
    }

    /**
     * @return Whether the given tab should be treated as chrome://newTab. This function returns
     *         true only when {@link OMNIBOX_FOCUSED_ON_NEW_TAB} is enabled, the tab is newly
     *         created from the new Tab menu or "+" button, and it hasn't navigate to any URL yet.
     */
    public static boolean shouldHandleAsNtp(Tab tab) {
        if (tab == null || tab.getUrl() == null) return false;

        return tab.getUrl().isEmpty() && StartSurfaceUserData.getCreatedAsNtp(tab);
    }

    /**
     * Sets the UserData if the Tab is a newly created empty Tab when
     * {@link OMNIBOX_FOCUSED_ON_NEW_TAB} is enabled.
     */
    public static void maySetUserDataForEmptyTab(Tab tab, String url) {
        if (!OMNIBOX_FOCUSED_ON_NEW_TAB.getValue() || tab == null || !TextUtils.isEmpty(url)
                || tab.getLaunchType() != TabLaunchType.FROM_START_SURFACE) {
            return;
        }
        StartSurfaceUserData.setFocusOnOmnibox(tab, true);
        StartSurfaceUserData.setCreatedAsNtp(tab);
    }

    /**
     * @return Whether new surface should show when home button is clicked.
     */
    public static boolean shouldShowNewSurfaceFromHomeButton() {
        return NEW_SURFACE_FROM_HOME_BUTTON.getValue().equals("hide_tab_switcher_only")
                || NEW_SURFACE_FROM_HOME_BUTTON.getValue().equals("hide_mv_tiles_and_tab_switcher");
    }

    /**
     * @return Whether start surface should be hidden when accessibility is enabled. If it's true,
     *         NTP is shown as homepage. Also, when time threshold is reached, grid tab switcher or
     *         overview list layout is shown instead of start surface.
     */
    public static boolean shouldHideStartSurfaceWithAccessibilityOn(Context context) {
        return ChromeAccessibilityUtil.get().isAccessibilityEnabled()
                && !(SUPPORT_ACCESSIBILITY.getValue()
                        && TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(context));
    }

    /**
     * @return Whether the {@link Intent} will open a new tab with the omnibox focused.
     */
    public static boolean shouldIntentShowNewTabOmniboxFocused(Intent intent) {
        final String intentUrl = IntentHandler.getUrlFromIntent(intent);
        // If Chrome is launched by tapping the New tab item from the launch icon and
        // OMNIBOX_FOCUSED_ON_NEW_TAB is enabled, a new Tab with omnibox focused will be shown on
        // Startup.
        final boolean isCanonicalizedNTPUrl = UrlUtilities.isCanonicalizedNTPUrl(intentUrl);
        return isCanonicalizedNTPUrl && IntentHandler.isTabOpenAsNewTabFromLauncher(intent)
                && OMNIBOX_FOCUSED_ON_NEW_TAB.getValue()
                && IntentHandler.wasIntentSenderChrome(intent);
    }

    @VisibleForTesting
    static void setFeedVisibilityForTesting(boolean isVisible) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FEED_ARTICLES_LIST_VISIBLE, isVisible);
    }
}

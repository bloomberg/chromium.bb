// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.flags;

import android.text.TextUtils;

import org.chromium.base.annotations.RemovableInRelease;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.CachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tasks.ConditionalTabStripUtils;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.toolbar.bottom.BottomToolbarVariationManager;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Caches the flags that Chrome might require before native is loaded in a later next run.
 */
public class ChromeCachedFlags {
    private boolean mIsFinishedCachingNativeFlags;

    private static final ChromeCachedFlags INSTANCE = new ChromeCachedFlags();

    /**
     * @return The {@link ChromeCachedFlags} singleton.
     */
    public static ChromeCachedFlags getInstance() {
        return INSTANCE;
    }

    /**
     * Caches flags that are needed by Activities that launch before the native library is loaded
     * and stores them in SharedPreferences. Because this function is called during launch after the
     * library has loaded, they won't affect the next launch until Chrome is restarted.
     */
    public void cacheNativeFlags() {
        if (mIsFinishedCachingNativeFlags) return;
        FirstRunUtils.cacheFirstRunPrefs();

        // clang-format off
        List<String> featuresToCache = Arrays.asList(
                ChromeFeatureList.ANDROID_PARTNER_CUSTOMIZATION_PHENOTYPE,
                ChromeFeatureList.CHROME_DUET,
                ChromeFeatureList.CHROME_DUET_ADAPTIVE,
                ChromeFeatureList.CHROME_DUET_LABELED,
                ChromeFeatureList.CLOSE_TAB_SUGGESTIONS,
                ChromeFeatureList.COMMAND_LINE_ON_NON_ROOTED,
                ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID,
                ChromeFeatureList.DOWNLOADS_AUTO_RESUMPTION_NATIVE,
                ChromeFeatureList.DUET_TABSTRIP_INTEGRATION_ANDROID,
                ChromeFeatureList.HOMEPAGE_LOCATION_POLICY,
                ChromeFeatureList.IMMERSIVE_UI_MODE,
                ChromeFeatureList.INSTANT_START,
                ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS,
                ChromeFeatureList.OMNIBOX_SUGGESTIONS_RECYCLER_VIEW,
                ChromeFeatureList.PAINT_PREVIEW_DEMO,
                ChromeFeatureList.PRIORITIZE_BOOTSTRAP_TASKS,
                ChromeFeatureList.START_SURFACE_ANDROID,
                ChromeFeatureList.SWAP_PIXEL_FORMAT_TO_FIX_CONVERT_FROM_TRANSLUCENT,
                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                ChromeFeatureList.TAB_GROUPS_ANDROID,
                ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
                ChromeFeatureList.TAB_TO_GTS_ANIMATION);
        // clang-format on
        CachedFeatureFlags.cacheNativeFlags(featuresToCache);
        CachedFeatureFlags.cacheAdditionalNativeFlags();

        // clang-format off
        List<CachedFieldTrialParameter> fieldTrialsToCache = Arrays.asList(
                BottomToolbarVariationManager.BOTTOM_TOOLBAR_VARIATION,
                ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_INFOBAR_LIMIT,
                ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_INFOBAR_PERIOD,
                ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_SESSION_TIME_MS,
                ReturnToChromeExperimentsUtil.TAB_SWITCHER_ON_RETURN_MS,
                StartSurfaceConfiguration.START_SURFACE_EXCLUDE_MV_TILES,
                StartSurfaceConfiguration.START_SURFACE_HIDE_INCOGNITO_SWITCH,
                StartSurfaceConfiguration.START_SURFACE_LAST_ACTIVE_TAB_ONLY,
                StartSurfaceConfiguration.START_SURFACE_OPEN_NTP_INSTEAD_OF_START,
                StartSurfaceConfiguration.START_SURFACE_SHOW_STACK_TAB_SWITCHER,
                StartSurfaceConfiguration.START_SURFACE_VARIATION,
                StartSurfaceConfiguration.START_SURFACE_OMNIBOX_SCROLL_MODE,
                TabContentManager.ALLOW_TO_REFETCH_TAB_THUMBNAIL_VARIATION,
                TabUiFeatureUtilities.ENABLE_SEARCH_CHIP,
                TabUiFeatureUtilities.ENABLE_SEARCH_CHIP_ADAPTIVE,
                TabUiFeatureUtilities.ZOOMING_MIN_MEMORY,
                TabUiFeatureUtilities.ZOOMING_MIN_SDK,
                TabUiFeatureUtilities.SKIP_SLOW_ZOOMING,
                TabUiFeatureUtilities.TAB_GRID_LAYOUT_ANDROID_NEW_TAB_TILE,
                TabUiFeatureUtilities.THUMBNAIL_ASPECT_RATIO);
        // clang-format on
        tryToCatchMissingParameters(fieldTrialsToCache);
        CachedFeatureFlags.cacheFieldTrialParameters(fieldTrialsToCache);

        // TODO(crbug.com/1062013): Remove this after M85.
        // This pref is only needed while clients are transitioning to caching via
        // {@link StartSurfaceConfiguration#START_SURFACE_VARIATION}. It is still honored by
        // {@link StartSurfaceConfiguration#isStartSurfaceSinglePaneEnabled()}. When it is removed,
        // the cached value will be lost, but this only matters if the client hasn't started Chrome
        // in months, and the effect is only that they will use a default value for the first run.
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.START_SURFACE_SINGLE_PANE_ENABLED_KEY);

        mIsFinishedCachingNativeFlags = true;
    }

    @RemovableInRelease
    private void tryToCatchMissingParameters(List<CachedFieldTrialParameter> listed) {
        // All instances of CachedFieldTrialParameter should be manually passed to
        // CachedFeatureFlags.cacheFieldTrialParameters(). The following checking is a best-effort
        // attempt to try to catch accidental omissions. It cannot replace the list because some
        // instances might not be instantiated if the classes they belong to are not accessed yet.
        List<String> omissions = new ArrayList<>();
        for (CachedFieldTrialParameter trial : CachedFieldTrialParameter.getAllInstances()) {
            if (listed.contains(trial)) continue;
            omissions.add(trial.getFeatureName() + ":" + trial.getParameterName());
        }
        assert omissions.isEmpty()
            : "The following trials are not correctly cached: "
                + TextUtils.join(", ", omissions);
    }

    /**
     * Caches flags that are enabled in ServiceManager only mode and must take effect on startup but
     * are set via native code. This function needs to be called in ServiceManager only mode to mark
     * these field trials as active, otherwise histogram data recorded in ServiceManager only mode
     * won't be tagged with their corresponding field trial experiments.
     */
    public void cacheServiceManagerOnlyFlags() {
        // TODO(crbug.com/995355): Move other related flags from cacheNativeFlags() to here.
        CachedFeatureFlags.cacheNativeFlags(
                Arrays.asList(ChromeFeatureList.SERVICE_MANAGER_FOR_DOWNLOAD,
                        ChromeFeatureList.SERVICE_MANAGER_FOR_BACKGROUND_PREFETCH));
    }
}

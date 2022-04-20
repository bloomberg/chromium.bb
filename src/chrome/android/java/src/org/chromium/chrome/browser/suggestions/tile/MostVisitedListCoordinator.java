// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.app.Activity;
import android.content.res.Configuration;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.suggestions.SuggestionsDependencyFactory;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.tile.MostVisitedListViewBinder.ViewHolder;
import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for displaying a list of {@link SuggestionsTileView} in a {@link ViewGroup}.
 */
public class MostVisitedListCoordinator implements ConfigurationChangedObserver {
    private static final int TITLE_LINES = 1;
    public static final String CONTEXT_MENU_USER_ACTION_PREFIX = "Suggestions";

    private final Activity mActivity;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final MostVisitedListMediator mMediator;
    private final WindowAndroid mWindowAndroid;
    private final UiConfig mUiConfig;
    private final PropertyModelChangeProcessor mModelChangeProcessor;
    private TileRenderer mRenderer;
    private ContextMenuManager mContextMenuManager;
    private OfflinePageBridge mOfflinePageBridge;

    /**
     * @param activity The app activity.
     * @param activityLifecycleDispatcher Dispatcher for activity lifecycle events,
     *                                    e.g.configuration changes. We need this to adjust the
     *                                    paddings and margins of the tile views.
     * @param mvTilesContainerLayout The container view of most visited tiles layout.
     * @param windowAndroid The current {@link WindowAndroid}
     * @param shouldShowSkeletonUIPreNative Whether to show the background icon for pre-native
     *         surface.
     */
    public MostVisitedListCoordinator(Activity activity,
            ActivityLifecycleDispatcher activityLifecycleDispatcher, View mvTilesContainerLayout,
            WindowAndroid windowAndroid, boolean shouldShowSkeletonUIPreNative) {
        mActivity = activity;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mWindowAndroid = windowAndroid;
        MvTilesLayout mvTilesLayout = mvTilesContainerLayout.findViewById(R.id.mv_tiles_layout);
        mUiConfig = new UiConfig(mvTilesLayout);

        PropertyModel propertyModel = new PropertyModel(MostVisitedListProperties.ALL_KEYS);
        mModelChangeProcessor = PropertyModelChangeProcessor.create(propertyModel,
                new ViewHolder(mvTilesContainerLayout, mvTilesLayout),
                MostVisitedListViewBinder::bind);

        mRenderer =
                new TileRenderer(mActivity, SuggestionsConfig.TileStyle.MODERN, TITLE_LINES, null);

        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity);

        mMediator = new MostVisitedListMediator(activity.getResources(), mvTilesContainerLayout,
                mRenderer, propertyModel, shouldShowSkeletonUIPreNative, isTablet);
    }

    /**
     * Called before the TasksSurface is showing to initialize MV tiles.
     * {@link MostVisitedListCoordinator#destroyMVTiles()} is called after the TasksSurface hides.
     *
     * @param suggestionsUiDelegate The UI delegate of suggestion surface.
     * @param tileGroupDelegate The delegate of tile group.
     * @param touchEnabledDelegate The {@link TouchEnabledDelegate} for handling whether touch
     *                             events are allowed.
     */
    public void initWithNative(SuggestionsUiDelegate suggestionsUiDelegate,
            TileGroup.Delegate tileGroupDelegate, TouchEnabledDelegate touchEnabledDelegate) {
        mActivityLifecycleDispatcher.register(this);
        Profile profile = Profile.getLastUsedRegularProfile();
        if (mRenderer == null) {
            mRenderer = new TileRenderer(mActivity, SuggestionsConfig.getTileStyle(mUiConfig),
                    TITLE_LINES, suggestionsUiDelegate.getImageFetcher());
        } else {
            mRenderer.setImageFetcher(suggestionsUiDelegate.getImageFetcher());
        }

        mContextMenuManager = new ContextMenuManager(suggestionsUiDelegate.getNavigationDelegate(),
                touchEnabledDelegate, mActivity::closeContextMenu, CONTEXT_MENU_USER_ACTION_PREFIX);
        mWindowAndroid.addContextMenuCloseListener(mContextMenuManager);
        mOfflinePageBridge =
                SuggestionsDependencyFactory.getInstance().getOfflinePageBridge(profile);
        mMediator.initWithNative(suggestionsUiDelegate, mContextMenuManager, tileGroupDelegate,
                mOfflinePageBridge, mRenderer);
    }

    /** Called when the TasksSurface is hidden or NewTabPageLayout is destroyed. */
    public void destroyMVTiles() {
        mActivityLifecycleDispatcher.unregister(this);

        if (mOfflinePageBridge != null) mOfflinePageBridge = null;
        if (mRenderer != null) mRenderer = null;

        if (mWindowAndroid != null) {
            mWindowAndroid.removeContextMenuCloseListener(mContextMenuManager);
            mContextMenuManager = null;
        }

        if (mMediator != null) mMediator.destroy();
    }

    public boolean isMVTilesCleanedUp() {
        return mMediator.isMVTilesCleanedUp();
    }

    public void onSwitchToForeground() {
        assert !isMVTilesCleanedUp();
        mMediator.onSwitchToForeground();
    }

    /* ConfigurationChangedObserver implementation. */
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        mMediator.onConfigurationChanged();
        mUiConfig.updateDisplayStyle();
    }
}

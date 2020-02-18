// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.MenuOrKeyboardActionController;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/**
 * Parent coordinator that is responsible for showing a grid or carousel of tabs for the main
 * TabSwitcher UI.
 */
public class TabSwitcherCoordinator implements Destroyable, TabSwitcher,
                                               TabSwitcher.TabListDelegate,
                                               TabSwitcherMediator.ResetHandler {
    // TODO(crbug.com/982018): Rename 'COMPONENT_NAME' so as to add different metrics for carousel
    // tab switcher.
    final static String COMPONENT_NAME = "GridTabSwitcher";
    private final PropertyModelChangeProcessor mContainerViewChangeProcessor;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    private final TabListCoordinator mTabListCoordinator;
    private final TabSwitcherMediator mMediator;
    private final MultiThumbnailCardProvider mMultiThumbnailCardProvider;
    private final TabGridDialogCoordinator mTabGridDialogCoordinator;
    private final TabSelectionEditorCoordinator mTabSelectionEditorCoordinator;
    private final UndoGroupSnackbarController mUndoGroupSnackbarController;

    private final MenuOrKeyboardActionController
            .MenuOrKeyboardActionHandler mTabSwitcherMenuActionHandler =
            new MenuOrKeyboardActionController.MenuOrKeyboardActionHandler() {
                @Override
                public boolean handleMenuOrKeyboardAction(int id, boolean fromMenu) {
                    if (id == R.id.menu_group_tabs) {
                        mTabSelectionEditorCoordinator.getController().show();
                        RecordUserAction.record("MobileMenuGroupTabs");
                        return true;
                    }
                    return false;
                }
            };
    private TabGridIphItemCoordinator mTabGridIphItemCoordinator;

    public TabSwitcherCoordinator(Context context, ActivityLifecycleDispatcher lifecycleDispatcher,
            TabModelSelector tabModelSelector, TabContentManager tabContentManager,
            CompositorViewHolder compositorViewHolder, ChromeFullscreenManager fullscreenManager,
            TabCreatorManager tabCreatorManager,
            MenuOrKeyboardActionController menuOrKeyboardActionController,
            SnackbarManager.SnackbarManageable snackbarManageable, @Nullable ViewGroup container,
            @TabListCoordinator.TabListMode int mode) {
        PropertyModel containerViewModel = new PropertyModel(TabListContainerProperties.ALL_KEYS);

        mTabSelectionEditorCoordinator = new TabSelectionEditorCoordinator(
                context, compositorViewHolder, tabModelSelector, tabContentManager);

        mMediator = new TabSwitcherMediator(this, containerViewModel, tabModelSelector,
                fullscreenManager, compositorViewHolder,
                mTabSelectionEditorCoordinator.getController());

        if (FeatureUtilities.isTabGroupsAndroidUiImprovementsEnabled()) {
            mTabGridDialogCoordinator = new TabGridDialogCoordinator(context, tabModelSelector,
                    tabContentManager, tabCreatorManager, compositorViewHolder, this, mMediator,
                    this::getTabGridDialogAnimationParams);

            mUndoGroupSnackbarController =
                    new UndoGroupSnackbarController(context, tabModelSelector, snackbarManageable);

            mMediator.setTabGridDialogResetHandler(mTabGridDialogCoordinator.getResetHandler());
        } else {
            mTabGridDialogCoordinator = null;
            mUndoGroupSnackbarController = null;
        }
        mMultiThumbnailCardProvider =
                new MultiThumbnailCardProvider(context, tabContentManager, tabModelSelector);

        TabListMediator.TitleProvider titleProvider = tab -> {
            int numRelatedTabs = tabModelSelector.getTabModelFilterProvider()
                                         .getCurrentTabModelFilter()
                                         .getRelatedTabList(tab.getId())
                                         .size();
            if (numRelatedTabs == 1) return tab.getTitle();
            return context.getResources().getQuantityString(
                    R.plurals.bottom_tab_grid_title_placeholder, numRelatedTabs, numRelatedTabs);
        };

        ViewGroup tabListContainerView = container != null ? container : compositorViewHolder;
        mTabListCoordinator =
                new TabListCoordinator(mode, context, tabModelSelector, mMultiThumbnailCardProvider,
                        titleProvider, true, mMediator::getCreateGroupButtonOnClickListener,
                        mMediator, null, null, null, tabListContainerView,
                        compositorViewHolder.getDynamicResourceLoader(), true, COMPONENT_NAME);
        mContainerViewChangeProcessor = PropertyModelChangeProcessor.create(containerViewModel,
                mTabListCoordinator.getContainerView(), TabListContainerViewBinder::bind);

        if (FeatureUtilities.isTabGroupsAndroidUiImprovementsEnabled()
                && mode == TabListCoordinator.TabListMode.GRID) {
            mTabGridIphItemCoordinator = new TabGridIphItemCoordinator(
                    context, mTabListCoordinator.getContainerView(), container);
            mMediator.setIphProvider(mTabGridIphItemCoordinator.getIphProvider());
        }

        mMenuOrKeyboardActionController = menuOrKeyboardActionController;
        mMenuOrKeyboardActionController.registerMenuOrKeyboardActionHandler(
                mTabSwitcherMenuActionHandler);

        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
    }

    // TabSwitcher implementation.
    @Override
    public void setOnTabSelectingListener(OnTabSelectingListener listener) {
        mMediator.setOnTabSelectingListener(listener);
    }

    @Override
    public Controller getController() {
        return mMediator;
    }

    @Override
    public TabListDelegate getTabListDelegate() {
        return this;
    }

    @Override
    public void setBottomControlsHeight(int bottomControlsHeight) {
        mMediator.setBottomControlsHeight(bottomControlsHeight);
    }

    @Override
    public boolean prepareOverview() {
        boolean quick = mMediator.prepareOverview();
        mTabListCoordinator.prepareOverview();
        return quick;
    }

    @Override
    public void postHiding() {
        mTabListCoordinator.postHiding();
        mMediator.postHiding();
    }

    @Override
    @NonNull
    public Rect getThumbnailLocationOfCurrentTab(boolean forceUpdate) {
        if (mTabGridDialogCoordinator != null && mTabGridDialogCoordinator.isVisible()) {
            assert forceUpdate;
            Rect thumbnail = mTabGridDialogCoordinator.getGlobalLocationOfCurrentThumbnail();
            // Adjust to the relative coordinate.
            Rect root = mTabListCoordinator.getRecyclerViewLocation();
            thumbnail.offset(-root.left, -root.top);
            return thumbnail;
        }
        if (forceUpdate) mTabListCoordinator.updateThumbnailLocation();
        return mTabListCoordinator.getThumbnailLocationOfCurrentTab();
    }

    @Override
    public int getResourceId() {
        return mTabListCoordinator.getResourceId();
    }

    @Override
    public long getLastDirtyTimeForTesting() {
        return mTabListCoordinator.getLastDirtyTimeForTesting();
    }

    @Override
    @VisibleForTesting
    public void setBitmapCallbackForTesting(Callback<Bitmap> callback) {
        TabListMediator.ThumbnailFetcher.sBitmapCallbackForTesting = callback;
    }

    @Override
    @VisibleForTesting
    public int getBitmapFetchCountForTesting() {
        return TabListMediator.ThumbnailFetcher.sFetchCountForTesting;
    }

    @Override
    @VisibleForTesting
    public int getSoftCleanupDelayForTesting() {
        return mMediator.getCleanupDelayForTesting();
    }

    @Override
    @VisibleForTesting
    public int getCleanupDelayForTesting() {
        return mMediator.getCleanupDelayForTesting();
    }

    // ResetHandler implementation.
    @Override
    public boolean resetWithTabList(@Nullable TabList tabList, boolean quickMode) {
        List<Tab> tabs = null;
        if (tabList != null) {
            tabs = new ArrayList<>();
            for (int i = 0; i < tabList.getCount(); i++) {
                tabs.add(tabList.getTabAt(i));
            }
        }
        return mTabListCoordinator.resetWithListOfTabs(tabs, quickMode);
    }

    private TabGridDialogParent.AnimationParams getTabGridDialogAnimationParams(int index) {
        View itemView = mTabListCoordinator.getContainerView()
                                .findViewHolderForAdapterPosition(index)
                                .itemView;
        Rect rect = mTabListCoordinator.getContainerView().getRectOfCurrentTabGridCard(index);
        return new TabGridDialogParent.AnimationParams(rect, itemView);
    }

    @Override
    public void softCleanup() {
        mTabListCoordinator.softCleanup();
    }

    // ResetHandler implementation.
    @Override
    public void destroy() {
        mMenuOrKeyboardActionController.unregisterMenuOrKeyboardActionHandler(
                mTabSwitcherMenuActionHandler);
        mTabListCoordinator.destroy();
        mContainerViewChangeProcessor.destroy();
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.destroy();
        }
        if (mUndoGroupSnackbarController != null) {
            mUndoGroupSnackbarController.destroy();
        }
        if (mTabGridIphItemCoordinator != null) {
            mTabGridIphItemCoordinator.destroy();
        }
        mMultiThumbnailCardProvider.destroy();
        mMediator.destroy();
        mLifecycleDispatcher.unregister(this);
    }
}

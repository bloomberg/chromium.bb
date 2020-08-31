// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.RectF;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionsOrchestrator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.List;

/**
 * Parent coordinator that is responsible for showing a grid or carousel of tabs for the main
 * TabSwitcher UI.
 */
public class TabSwitcherCoordinator
        implements Destroyable, TabSwitcher, TabSwitcher.TabListDelegate,
                   TabSwitcher.TabDialogDelegation, TabSwitcherMediator.ResetHandler,
                   TabSwitcherMediator.MessageItemsController {
    /**
     * Interface to control the IPH dialog.
     */
    interface IphController {
        /**
         * Show the dialog with IPH.
         */
        void showIph();
    }

    private class TabGroupManualSelectionMode {
        public final String actionString;
        public final int enablingThreshold;
        public final TabSelectionEditorActionProvider actionProvider;
        public final TabSelectionEditorCoordinator
                .TabSelectionEditorNavigationProvider navigationProvider;

        TabGroupManualSelectionMode(String actionString, int enablingThreshold,
                TabSelectionEditorActionProvider actionProvider,
                TabSelectionEditorCoordinator
                        .TabSelectionEditorNavigationProvider navigationProvider) {
            this.actionString = actionString;
            this.enablingThreshold = enablingThreshold;
            this.actionProvider = actionProvider;
            this.navigationProvider = navigationProvider;
        }
    }

    // TODO(crbug.com/982018): Rename 'COMPONENT_NAME' so as to add different metrics for carousel
    // tab switcher.
    static final String COMPONENT_NAME = "GridTabSwitcher";
    private static boolean sAppendedMessagesForTesting;
    private final PropertyModelChangeProcessor mContainerViewChangeProcessor;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    private final TabListCoordinator mTabListCoordinator;
    private final TabSwitcherMediator mMediator;
    private final MultiThumbnailCardProvider mMultiThumbnailCardProvider;
    private final TabGridDialogCoordinator mTabGridDialogCoordinator;
    private final TabModelSelector mTabModelSelector;
    private final @TabListCoordinator.TabListMode int mMode;
    private final MessageCardProviderCoordinator mMessageCardProviderCoordinator;

    private TabSelectionEditorCoordinator mTabSelectionEditorCoordinator;
    private TabGroupManualSelectionMode mTabGroupManualSelectionMode;
    private UndoGroupSnackbarController mUndoGroupSnackbarController;
    private TabSuggestionsOrchestrator mTabSuggestionsOrchestrator;
    private NewTabTileCoordinator mNewTabTileCoordinator;
    private TabAttributeCache mTabAttributeCache;
    private ViewGroup mContainer;
    private TabCreatorManager mTabCreatorManager;
    private boolean mIsInitialized;

    private final MenuOrKeyboardActionController
            .MenuOrKeyboardActionHandler mTabSwitcherMenuActionHandler =
            new MenuOrKeyboardActionController.MenuOrKeyboardActionHandler() {
                @Override
                public boolean handleMenuOrKeyboardAction(int id, boolean fromMenu) {
                    if (id == R.id.menu_group_tabs) {
                        assert mTabGroupManualSelectionMode != null;

                        mTabSelectionEditorCoordinator.getController().configureToolbar(
                                mTabGroupManualSelectionMode.actionString,
                                mTabGroupManualSelectionMode.actionProvider,
                                mTabGroupManualSelectionMode.enablingThreshold,
                                mTabGroupManualSelectionMode.navigationProvider);

                        mTabSelectionEditorCoordinator.getController().show(
                                mTabModelSelector.getTabModelFilterProvider()
                                        .getCurrentTabModelFilter()
                                        .getTabsWithNoOtherRelatedTabs());
                        RecordUserAction.record("MobileMenuGroupTabs");
                        return true;
                    }
                    return false;
                }
            };
    private TabGridIphDialogCoordinator mTabGridIphDialogCoordinator;

    public TabSwitcherCoordinator(Context context, ActivityLifecycleDispatcher lifecycleDispatcher,
            TabModelSelector tabModelSelector, TabContentManager tabContentManager,
            ChromeFullscreenManager fullscreenManager, TabCreatorManager tabCreatorManager,
            MenuOrKeyboardActionController menuOrKeyboardActionController, ViewGroup container,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            @TabListCoordinator.TabListMode int mode) {
        mMode = mode;
        mTabModelSelector = tabModelSelector;
        mContainer = container;
        mTabCreatorManager = tabCreatorManager;

        PropertyModel containerViewModel = new PropertyModel(TabListContainerProperties.ALL_KEYS);

        mMediator = new TabSwitcherMediator(this, containerViewModel, tabModelSelector,
                fullscreenManager, container, tabContentManager, this, mode);

        mMultiThumbnailCardProvider =
                new MultiThumbnailCardProvider(context, tabContentManager, tabModelSelector);

        PseudoTab.TitleProvider titleProvider = tab -> {
            int numRelatedTabs =
                    PseudoTab.getRelatedTabs(tab, tabModelSelector.getTabModelFilterProvider())
                            .size();
            if (numRelatedTabs == 1) return tab.getTitle();
            return context.getResources().getQuantityString(
                    R.plurals.bottom_tab_grid_title_placeholder, numRelatedTabs, numRelatedTabs);
        };

        mTabListCoordinator = new TabListCoordinator(mode, context, tabModelSelector,
                mMultiThumbnailCardProvider, titleProvider, true, mMediator, null,
                TabProperties.UiType.CLOSABLE, null, container, true, COMPONENT_NAME);
        mContainerViewChangeProcessor = PropertyModelChangeProcessor.create(containerViewModel,
                mTabListCoordinator.getContainerView(), TabListContainerViewBinder::bind);

        mMessageCardProviderCoordinator = new MessageCardProviderCoordinator(context,
                (identifier)
                        -> mTabListCoordinator.removeSpecialListItem(
                                TabProperties.UiType.MESSAGE, identifier));

        if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled()) {
            mTabGridDialogCoordinator = new TabGridDialogCoordinator(context, tabModelSelector,
                    tabContentManager, tabCreatorManager,
                    ((ChromeTabbedActivity) context).getCompositorViewHolder(), this, mMediator,
                    this::getTabGridDialogAnimationSourceView, shareDelegateSupplier);

            mMediator.setTabGridDialogController(mTabGridDialogCoordinator.getDialogController());
        } else {
            mTabGridDialogCoordinator = null;
        }

        if (mode == TabListCoordinator.TabListMode.GRID) {
            if (shouldRegisterMessageItemType()) {
                mTabListCoordinator.registerItemType(TabProperties.UiType.MESSAGE,
                        new LayoutViewBuilder(R.layout.tab_grid_message_card_item),
                        MessageCardViewBinder::bind);
            }

            if (TabUiFeatureUtilities.isTabGridLayoutAndroidNewTabTileEnabled()) {
                mTabListCoordinator.registerItemType(TabProperties.UiType.NEW_TAB_TILE,
                        new LayoutViewBuilder(R.layout.new_tab_tile_card_item),
                        NewTabTileViewBinder::bind);
            }
        }

        if (CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START)
                || TabUiFeatureUtilities.ENABLE_SEARCH_CHIP.getValue()
                        && mode != TabListCoordinator.TabListMode.CAROUSEL) {
            mTabAttributeCache = new TabAttributeCache(mTabModelSelector);
        }

        mMenuOrKeyboardActionController = menuOrKeyboardActionController;
        mMenuOrKeyboardActionController.registerMenuOrKeyboardActionHandler(
                mTabSwitcherMenuActionHandler);

        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
    }

    @VisibleForTesting
    public static boolean hasAppendedMessagesForTesting() {
        return sAppendedMessagesForTesting;
    }

    @Override
    public void initWithNative(Context context, TabContentManager tabContentManager,
            DynamicResourceLoader dynamicResourceLoader,
            SnackbarManager.SnackbarManageable snackbarManageable,
            ModalDialogManager modalDialogManager) {
        if (mIsInitialized) return;

        setUpTabGroupManualSelectionMode(context, tabContentManager);

        mTabListCoordinator.initWithNative(dynamicResourceLoader);
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.initWithNative(context, mTabModelSelector, tabContentManager,
                    mTabListCoordinator.getTabGroupTitleEditor());
        }

        mMultiThumbnailCardProvider.initWithNative();

        if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled()) {
            mUndoGroupSnackbarController =
                    new UndoGroupSnackbarController(context, mTabModelSelector, snackbarManageable);
        } else {
            mUndoGroupSnackbarController = null;
        }

        if (mMode == TabListCoordinator.TabListMode.GRID) {
            if (CachedFeatureFlags.isEnabled(ChromeFeatureList.CLOSE_TAB_SUGGESTIONS)) {
                mTabSuggestionsOrchestrator =
                        new TabSuggestionsOrchestrator(mTabModelSelector, mLifecycleDispatcher);
                TabSuggestionMessageService tabSuggestionMessageService =
                        new TabSuggestionMessageService(context, mTabModelSelector,
                                mTabSelectionEditorCoordinator.getController());
                mTabSuggestionsOrchestrator.addObserver(tabSuggestionMessageService);
                mMessageCardProviderCoordinator.subscribeMessageService(
                        tabSuggestionMessageService);
            }

            if (TabUiFeatureUtilities.isTabGridLayoutAndroidNewTabTileEnabled()) {
                mNewTabTileCoordinator =
                        new NewTabTileCoordinator(mTabModelSelector, mTabCreatorManager);
            }

            if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled()
                    && !TabSwitcherMediator.isShowingTabsInMRUOrder()) {
                mTabGridIphDialogCoordinator =
                        new TabGridIphDialogCoordinator(context, mContainer, modalDialogManager);
                IphMessageService iphMessageService =
                        new IphMessageService(mTabGridIphDialogCoordinator);
                mMessageCardProviderCoordinator.subscribeMessageService(iphMessageService);
            }
        }
        mIsInitialized = true;
    }

    private void setUpTabGroupManualSelectionMode(
            Context context, TabContentManager tabContentManager) {
        // For tab switcher in carousel mode, the selection editor should still follow grid style.
        int selectionEditorMode = mMode == TabListCoordinator.TabListMode.CAROUSEL
                ? TabListCoordinator.TabListMode.GRID
                : mMode;
        mTabSelectionEditorCoordinator = new TabSelectionEditorCoordinator(context, mContainer,
                mTabModelSelector, tabContentManager, null, selectionEditorMode);
        mMediator.initWithNative(mTabSelectionEditorCoordinator.getController());

        mTabGroupManualSelectionMode = new TabGroupManualSelectionMode(
                context.getString(R.string.tab_selection_editor_group), 2,
                new TabSelectionEditorActionProvider(mTabSelectionEditorCoordinator.getController(),
                        TabSelectionEditorActionProvider.TabSelectionEditorAction.GROUP),
                new TabSelectionEditorCoordinator.TabSelectionEditorNavigationProvider(
                        mTabSelectionEditorCoordinator.getController()));
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
    public TabDialogDelegation getTabGridDialogDelegation() {
        return this;
    }

    @Override
    public int getTabListTopOffset() {
        return mTabListCoordinator.getTabListTopOffset();
    }

    @Override
    public int getListModeForTesting() {
        return mMode;
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

    // TabListDelegate implementation.
    @Override
    public int getResourceId() {
        return mTabListCoordinator.getResourceId();
    }

    @Override
    public long getLastDirtyTime() {
        return mTabListCoordinator.getLastDirtyTime();
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
        return mMediator.getSoftCleanupDelayForTesting();
    }

    @Override
    @VisibleForTesting
    public int getCleanupDelayForTesting() {
        return mMediator.getCleanupDelayForTesting();
    }

    // TabDialogDelegation implementation.
    @Override
    @VisibleForTesting
    public void setSourceRectCallbackForTesting(Callback<RectF> callback) {
        TabGridDialogParent.setSourceRectCallbackForTesting(callback);
    }

    // ResetHandler implementation.
    @Override
    public boolean resetWithTabList(@Nullable TabList tabList, boolean quickMode, boolean mruMode) {
        return resetWithTabs(PseudoTab.getListOfPseudoTab(tabList), quickMode, mruMode);
    }

    @Override
    public boolean resetWithTabs(
            @Nullable List<PseudoTab> tabs, boolean quickMode, boolean mruMode) {
        mMediator.registerFirstMeaningfulPaintRecorder();
        boolean showQuickly = mTabListCoordinator.resetWithListOfTabs(tabs, quickMode, mruMode);
        if (showQuickly) {
            mTabListCoordinator.removeSpecialListItem(TabProperties.UiType.NEW_TAB_TILE, 0);
            removeAllAppendedMessage();
        }

        int cardsCount = tabs == null ? 0 : tabs.size();
        if (tabs != null && mNewTabTileCoordinator != null) {
            mTabListCoordinator.addSpecialListItem(tabs.size(), TabProperties.UiType.NEW_TAB_TILE,
                    mNewTabTileCoordinator.getModel());
            cardsCount += 1;
        }
        if (tabs != null && tabs.size() > 0) appendMessagesTo(cardsCount);

        return showQuickly;
    }

    // MessageItemsController implementation.
    @Override
    public void removeAllAppendedMessage() {
        mTabListCoordinator.removeSpecialListItem(
                TabProperties.UiType.MESSAGE, MessageService.MessageType.ALL);
        sAppendedMessagesForTesting = false;
    }

    @Override
    public void restoreAllAppendedMessage() {
        sAppendedMessagesForTesting = false;
        List<MessageCardProviderMediator.Message> messages =
                mMessageCardProviderCoordinator.getMessageItems();
        for (int i = 0; i < messages.size(); i++) {
            mTabListCoordinator.addSpecialListItemToEnd(
                    TabProperties.UiType.MESSAGE, messages.get(i).model);
        }
        sAppendedMessagesForTesting = messages.size() > 0;
    }

    private void appendMessagesTo(int index) {
        sAppendedMessagesForTesting = false;
        List<MessageCardProviderMediator.Message> messages =
                mMessageCardProviderCoordinator.getMessageItems();
        for (int i = 0; i < messages.size(); i++) {
            mTabListCoordinator.addSpecialListItem(
                    index + i, TabProperties.UiType.MESSAGE, messages.get(i).model);
        }
        if (messages.size() > 0) sAppendedMessagesForTesting = true;
    }

    private View getTabGridDialogAnimationSourceView(int tabId) {
        int index = mTabListCoordinator.indexOfTab(tabId);
        // TODO(crbug.com/999372): This is band-aid fix that will show basic fade-in/fade-out
        // animation when we cannot find the animation source view holder. This is happening due to
        // current group id in TabGridDialog can not be indexed in TabListModel, which should never
        // happen. Remove this when figure out the actual cause.
        ViewHolder sourceViewHolder =
                mTabListCoordinator.getContainerView().findViewHolderForAdapterPosition(index);
        if (sourceViewHolder == null) return null;
        return sourceViewHolder.itemView;
    }

    private boolean shouldRegisterMessageItemType() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.CLOSE_TAB_SUGGESTIONS)
                || (TabUiFeatureUtilities.isTabGroupsAndroidEnabled()
                        && !TabSwitcherMediator.isShowingTabsInMRUOrder());
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
        mMessageCardProviderCoordinator.destroy();
        mContainerViewChangeProcessor.destroy();
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.destroy();
        }
        if (mUndoGroupSnackbarController != null) {
            mUndoGroupSnackbarController.destroy();
        }
        if (mTabGridIphDialogCoordinator != null) {
            mTabGridIphDialogCoordinator.destroy();
        }
        if (mNewTabTileCoordinator != null) {
            mNewTabTileCoordinator.destroy();
        }
        mMultiThumbnailCardProvider.destroy();
        if (mTabSelectionEditorCoordinator != null) {
            mTabSelectionEditorCoordinator.destroy();
        }
        mMediator.destroy();
        mLifecycleDispatcher.unregister(this);
        if (mTabAttributeCache != null) {
            mTabAttributeCache.destroy();
        }
    }
}

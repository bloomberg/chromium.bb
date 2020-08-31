// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.ANIMATE_VISIBILITY_CHANGES;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.BOTTOM_CONTROLS_HEIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.INITIAL_SCROLL_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.SHADOW_TOP_MARGIN;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.TOP_CONTROLS_HEIGHT;
import static org.chromium.chrome.browser.tasks.tab_management.TabListContainerProperties.VISIBILITY_LISTENER;

import android.graphics.Bitmap;
import android.os.Handler;
import android.os.SystemClock;
import android.text.TextUtils;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsStateProvider;
import org.chromium.chrome.browser.init.FirstDrawDetector;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * The Mediator that is responsible for resetting the tab grid or carousel based on visibility and
 * model changes.
 */
class TabSwitcherMediator implements TabSwitcher.Controller, TabListRecyclerView.VisibilityListener,
                                     TabListMediator.GridCardOnClickListenerProvider {
    private static final String TAG = "TabSwitcherMediator";

    // This should be the same as TabListCoordinator.GRID_LAYOUT_SPAN_COUNT for the selected tab
    // to be on the 2nd row.
    static final int INITIAL_SCROLL_INDEX_OFFSET = 2;

    private static final int DEFAULT_TOP_PADDING = 0;

    // Count histograms for tab counts when showing switcher.
    static final String TAB_COUNT_HISTOGRAM = "Tabs.TabCountInSwitcher";
    static final String TAB_ENTRIES_HISTOGRAM = "Tabs.IndependentTabCountInSwitcher";

    /** Field trial parameter for the {@link TabListRecyclerView} cleanup delay. */
    private static final String SOFT_CLEANUP_DELAY_PARAM = "soft-cleanup-delay";
    private static final int DEFAULT_SOFT_CLEANUP_DELAY_MS = 3_000;
    private static final String CLEANUP_DELAY_PARAM = "cleanup-delay";
    private static final int DEFAULT_CLEANUP_DELAY_MS = 30_000;

    private final Handler mHandler;
    private final Runnable mSoftClearTabListRunnable;
    private final Runnable mClearTabListRunnable;

    private final ResetHandler mResetHandler;
    private final PropertyModel mContainerViewModel;
    private final TabModelSelector mTabModelSelector;
    private final TabModelObserver mTabModelObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final ObserverList<TabSwitcher.OverviewModeObserver> mObservers = new ObserverList<>();
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final BrowserControlsStateProvider.Observer mBrowserControlsObserver;
    private final ViewGroup mContainerView;
    private final TabContentManager mTabContentManager;

    private Integer mSoftCleanupDelayMsForTesting;
    private Integer mCleanupDelayMsForTesting;
    private TabGridDialogMediator.DialogController mTabGridDialogController;
    private TabSelectionEditorCoordinator
            .TabSelectionEditorController mTabSelectionEditorController;
    private TabSwitcher.OnTabSelectingListener mOnTabSelectingListener;

    /**
     * In cases where a didSelectTab was due to switching models with a toggle,
     * we don't change tab grid visibility.
     */
    private boolean mShouldIgnoreNextSelect;

    private int mModelIndexWhenShown;
    private int mTabIdWhenShown;
    private int mIndexInNewModelWhenSwitched;
    private boolean mIsSelectingInTabSwitcher;

    private boolean mShowTabsInMruOrder;

    private static class FirstMeaningfulPaintRecorder {
        private final long mActivityCreationTimeMs;

        private FirstMeaningfulPaintRecorder(long activityCreationTimeMs) {
            mActivityCreationTimeMs = activityCreationTimeMs;
        }

        private void record(int numOfThumbnails) {
            assert numOfThumbnails >= 0;
            long elapsed = SystemClock.elapsedRealtime() - mActivityCreationTimeMs;
            PostTask.postTask(UiThreadTaskTraits.BEST_EFFORT,
                    ()
                            -> ReturnToChromeExperimentsUtil.recordTimeToGTSFirstMeaningfulPaint(
                                    elapsed, numOfThumbnails));
        }
    }
    private FirstMeaningfulPaintRecorder mFirstMeaningfulPaintRecorder;
    private boolean mRegisteredFirstMeaningfulPaintRecorder;
    private @TabListCoordinator.TabListMode int mMode;

    /**
     * Interface to delegate resetting the tab grid.
     */
    interface ResetHandler {
        /**
         * Reset the tab grid with the given {@link TabList}, which can be null.
         * @param tabList The {@link TabList} to show the tabs for in the grid.
         * @param quickMode Whether to skip capturing the selected live tab for the thumbnail.
         * @param mruMode Whether order the Tabs by MRU.
         * @return Whether the {@link TabListRecyclerView} can be shown quickly.
         */
        boolean resetWithTabList(@Nullable TabList tabList, boolean quickMode, boolean mruMode);

        /**
         * Reset the tab grid with the given {@link List<PseudoTab>}, which can be null.
         * @param tabs The {@link List<PseudoTab>} to show the tabs for in the grid.
         * @param quickMode Whether to skip capturing the selected live tab for the thumbnail.
         * @param mruMode Whether order the Tabs by MRU.
         * @return Whether the {@link TabListRecyclerView} can be shown quickly.
         */
        boolean resetWithTabs(@Nullable List<PseudoTab> tabs, boolean quickMode, boolean mruMode);

        /**
         * Release the thumbnail {@link Bitmap} but keep the {@link TabGridView}.
         */
        void softCleanup();
    }

    /**
     * Interface to control message items in grid tab switcher.
     */
    interface MessageItemsController {
        /**
         * Remove all the message items in the model list. Right now this is used when all tabs are
         * closed in the grid tab switcher.
         */
        void removeAllAppendedMessage();

        /**
         * Restore all the message items that should show. Right now this is only used to restore
         * message items when the closure of the last tab in tab switcher is undone.
         */
        void restoreAllAppendedMessage();
    }

    /**
     * Basic constructor for the Mediator.
     * @param resetHandler The {@link ResetHandler} that handles reset for this Mediator.
     * @param containerViewModel The {@link PropertyModel} to keep state on the View containing the
     *         grid or carousel.
     * @param tabModelSelector {@link TabModelSelector} to observer for model and selection changes.
     * @param browserControlsStateProvider {@link BrowserControlsStateProvider} to use.
     * @param containerView The container {@link ViewGroup} to use.
     * @param tabContentManager The {@link TabContentManager} for first meaningful paint event.
     * @param mode One of the {@link TabListCoordinator.TabListMode}.
     */
    TabSwitcherMediator(ResetHandler resetHandler, PropertyModel containerViewModel,
            TabModelSelector tabModelSelector,
            BrowserControlsStateProvider browserControlsStateProvider, ViewGroup containerView,
            TabContentManager tabContentManager, MessageItemsController messageItemsController,
            @TabListCoordinator.TabListMode int mode) {
        mResetHandler = resetHandler;
        mContainerViewModel = containerViewModel;
        mTabModelSelector = tabModelSelector;
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mMode = mode;

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                mShouldIgnoreNextSelect = true;
                mIndexInNewModelWhenSwitched = newModel.index();

                TabList currentTabModelFilter =
                        mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
                mContainerViewModel.set(IS_INCOGNITO, currentTabModelFilter.isIncognito());
                if (mTabGridDialogController != null) {
                    mTabGridDialogController.hideDialog(false);
                }
                if (!mContainerViewModel.get(IS_VISIBLE)) return;
                mResetHandler.resetWithTabList(currentTabModelFilter, false, mShowTabsInMruOrder);

                // Hide the currently selected tab when moving to an empty incognito model.
                // TODO(crbug.com/1082612): If the need arises, generalize this solution in the
                // IncognitoTabModel.
                if (newModel.isIncognito() && newModel.getCount() == 0 && oldModel.getCount() > 0) {
                    oldModel.getTabAt(oldModel.index()).hide(TabHidingType.CHANGED_TABS);
                }
            }
        };
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        mTabModelObserver = new TabModelObserver() {
            @Override
            public void didAddTab(Tab tab, int type, @TabCreationState int creationState) {
                // TODO(wychen): move didAddTab and didSelectTab to another observer and inject
                //  after restoreCompleted.
                if (!mTabModelSelector.getTabModelFilterProvider()
                                .getCurrentTabModelFilter()
                                .isTabModelRestored()) {
                    return;
                }
                mShouldIgnoreNextSelect = false;
            }

            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                if (!mTabModelSelector.getTabModelFilterProvider()
                                .getCurrentTabModelFilter()
                                .isTabModelRestored()) {
                    return;
                }
                if (type == TabSelectionType.FROM_CLOSE || mShouldIgnoreNextSelect) {
                    mShouldIgnoreNextSelect = false;
                    return;
                }
                if (mIsSelectingInTabSwitcher) {
                    mIsSelectingInTabSwitcher = false;
                    TabModelFilter modelFilter = mTabModelSelector.getTabModelFilterProvider()
                                                         .getCurrentTabModelFilter();
                    if (modelFilter instanceof TabGroupModelFilter) {
                        ((TabGroupModelFilter) modelFilter).recordSessionsCount(tab);
                    }

                    // Use TabSelectionType.From_USER to filter the new tab creation case.
                    if (type == TabSelectionType.FROM_USER) recordUserSwitchedTab(tab, lastId);
                }

                if (mContainerViewModel.get(IS_VISIBLE)) {
                    onTabSelecting(tab.getId());
                }
            }

            @Override
            public void restoreCompleted() {
                if (!mContainerViewModel.get(IS_VISIBLE)) return;

                mResetHandler.resetWithTabList(
                        mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter(),
                        false, mShowTabsInMruOrder);
                recordTabCounts();
                setInitialScrollIndexOffset();
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                if (mTabModelSelector.getCurrentModel().getCount() == 1) {
                    messageItemsController.removeAllAppendedMessage();
                }
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                if (mTabModelSelector.getCurrentModel().getCount() == 1) {
                    messageItemsController.restoreAllAppendedMessage();
                }
            }
        };

        mBrowserControlsObserver = new BrowserControlsStateProvider.Observer() {
            @Override
            public void onContentOffsetChanged(int offset) {}

            @Override
            public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
                    int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {}

            @Override
            public void onTopControlsHeightChanged(
                    int topControlsHeight, int topControlsMinHeight) {
                if (mMode == TabListCoordinator.TabListMode.CAROUSEL) return;

                updateTopControlsProperties(topControlsHeight);
            }

            @Override
            public void onBottomControlsHeightChanged(
                    int bottomControlsHeight, int bottomControlsMinHeight) {
                mContainerViewModel.set(BOTTOM_CONTROLS_HEIGHT, bottomControlsHeight);
            }
        };

        mBrowserControlsStateProvider.addObserver(mBrowserControlsObserver);

        if (mTabModelSelector.getModels().isEmpty()) {
            TabModelSelectorObserver selectorObserver = new EmptyTabModelSelectorObserver() {
                @Override
                public void onChange() {
                    assert !mTabModelSelector.getModels().isEmpty();
                    assert mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(false)
                            != null;
                    assert mTabModelSelector.getTabModelFilterProvider().getTabModelFilter(true)
                            != null;
                    mTabModelSelector.removeObserver(this);
                    mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(
                            mTabModelObserver);
                }
            };
            mTabModelSelector.addObserver(selectorObserver);
        } else {
            mTabModelSelector.getTabModelFilterProvider().addTabModelFilterObserver(
                    mTabModelObserver);
        }

        mContainerViewModel.set(VISIBILITY_LISTENER, this);
        TabModelFilter tabModelFilter =
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
        mContainerViewModel.set(
                IS_INCOGNITO, tabModelFilter == null ? false : tabModelFilter.isIncognito());
        mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, true);

        // Container view takes care of padding and margin in start surface.
        if (mode != TabListCoordinator.TabListMode.CAROUSEL) {
            updateTopControlsProperties(browserControlsStateProvider.getTopControlsHeight());
            mContainerViewModel.set(
                    BOTTOM_CONTROLS_HEIGHT, browserControlsStateProvider.getBottomControlsHeight());
        }

        mContainerView = containerView;

        mSoftClearTabListRunnable = mResetHandler::softCleanup;
        mClearTabListRunnable =
                () -> mResetHandler.resetWithTabList(null, false, mShowTabsInMruOrder);
        mHandler = new Handler();
        mTabContentManager = tabContentManager;

        // TODO(crbug.com/982018): Let the start surface pass in the parameter and add unit test for
        // it. This is a temporary solution to keep this change minimum.
        mShowTabsInMruOrder = isShowingTabsInMRUOrder();
    }

    /**
     * Called after native initialization is completed.
     * @param tabSelectionEditorController The controller that can control the visibility of the
     *                                     TabSelectionEditor.
     */
    public void initWithNative(TabSelectionEditorCoordinator
                                       .TabSelectionEditorController tabSelectionEditorController) {
        mTabSelectionEditorController = tabSelectionEditorController;
    }

    /**
     * Set the controller of the TabGridDialog so that it can be directly controlled.
     * @param tabGridDialogController The handler of the Grid Dialog
     */
    void setTabGridDialogController(
            TabGridDialogMediator.DialogController tabGridDialogController) {
        mTabGridDialogController = tabGridDialogController;
    }

    @VisibleForTesting
    int getSoftCleanupDelayForTesting() {
        return getSoftCleanupDelay();
    }

    private int getSoftCleanupDelay() {
        if (mSoftCleanupDelayMsForTesting != null) return mSoftCleanupDelayMsForTesting;

        String delay = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, SOFT_CLEANUP_DELAY_PARAM);
        try {
            return Integer.valueOf(delay);
        } catch (NumberFormatException e) {
            return DEFAULT_SOFT_CLEANUP_DELAY_MS;
        }
    }

    @VisibleForTesting
    int getCleanupDelayForTesting() {
        return getCleanupDelay();
    }

    private int getCleanupDelay() {
        if (mCleanupDelayMsForTesting != null) return mCleanupDelayMsForTesting;

        String delay = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, CLEANUP_DELAY_PARAM);
        try {
            return Integer.valueOf(delay);
        } catch (NumberFormatException e) {
            return DEFAULT_CLEANUP_DELAY_MS;
        }
    }

    private void setVisibility(boolean isVisible) {
        if (isVisible) {
            RecordUserAction.record("MobileToolbarShowStackView");
        }

        mContainerViewModel.set(IS_VISIBLE, isVisible);
    }

    private void updateTopControlsProperties(int topControlsHeight) {
        // The start surface checks in this block are for top controls height and shadow
        // margin to be set correctly for displaying the omnibox above the tab switcher.
        topControlsHeight =
                StartSurfaceConfiguration.isStartSurfaceEnabled() ? 0 : topControlsHeight;
        mContainerViewModel.set(TOP_CONTROLS_HEIGHT, topControlsHeight);
        mContainerViewModel.set(SHADOW_TOP_MARGIN, topControlsHeight);
    }

    /**
     * Record tab switch related metric for GTS.
     * @param tab The new selected tab.
     * @param lastId The id of the previous selected tab, and that tab is still a valid tab
     *               in TabModel.
     */
    private void recordUserSwitchedTab(Tab tab, int lastId) {
        if (tab == null) {
            assert false : "New selected tab cannot be null when recording tab switch.";
            return;
        }

        Tab fromTab = TabModelUtils.getTabById(mTabModelSelector.getCurrentModel(), lastId);
        assert fromTab != null;
        if (mModelIndexWhenShown == mTabModelSelector.getCurrentModelIndex()) {
            if (tab.getId() == mTabIdWhenShown) {
                if (mMode == TabListCoordinator.TabListMode.CAROUSEL) {
                    RecordUserAction.record("MobileTabReturnedToCurrentTab.TabCarousel");
                } else if (mMode == TabListCoordinator.TabListMode.GRID) {
                    RecordUserAction.record("MobileTabReturnedToCurrentTab.TabGrid");
                } else {
                    // TODO(crbug.com/1085246): Differentiate others.
                }
                RecordUserAction.record("MobileTabReturnedToCurrentTab");
                RecordHistogram.recordSparseHistogram(
                        "Tabs.TabOffsetOfSwitch." + TabSwitcherCoordinator.COMPONENT_NAME, 0);
            } else {
                int fromIndex = mTabModelSelector.getTabModelFilterProvider()
                                        .getCurrentTabModelFilter()
                                        .indexOf(fromTab);
                int toIndex = mTabModelSelector.getTabModelFilterProvider()
                                      .getCurrentTabModelFilter()
                                      .indexOf(tab);

                if (fromIndex != toIndex || fromTab.getId() == tab.getId()) {
                    // Only log when you switch a tab page directly from tab switcher.
                    if (!TabUiFeatureUtilities.isTabGroupsAndroidEnabled()
                            || getRelatedTabs(tab.getId()).size() == 1) {
                        RecordUserAction.record(
                                "MobileTabSwitched." + TabSwitcherCoordinator.COMPONENT_NAME);
                    }
                    RecordHistogram.recordSparseHistogram(
                            "Tabs.TabOffsetOfSwitch." + TabSwitcherCoordinator.COMPONENT_NAME,
                            fromIndex - toIndex);
                }
            }
        } else {
            int newSelectedTabIndex =
                    TabModelUtils.getTabIndexById(mTabModelSelector.getCurrentModel(), tab.getId());
            if (newSelectedTabIndex == mIndexInNewModelWhenSwitched) {
                // TabModelImpl logs this action only when a different index is set within a
                // TabModelImpl. If we switch between normal tab model and incognito tab model and
                // leave the index the same (i.e. after switched tab model and select the
                // highlighted tab), TabModelImpl doesn't catch this case. Therefore, we record it
                // here.
                RecordUserAction.record("MobileTabSwitched");
            }
            // Only log when you switch a tab page directly from tab switcher.
            if (!TabUiFeatureUtilities.isTabGroupsAndroidEnabled()
                    || getRelatedTabs(tab.getId()).size() == 1) {
                RecordUserAction.record(
                        "MobileTabSwitched." + TabSwitcherCoordinator.COMPONENT_NAME);
            }
        }
    }

    @Override
    public boolean overviewVisible() {
        return mContainerViewModel.get(IS_VISIBLE);
    }

    @Override
    public void addOverviewModeObserver(TabSwitcher.OverviewModeObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeOverviewModeObserver(TabSwitcher.OverviewModeObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void hideOverview(boolean animate) {
        if (!animate) mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, false);
        setVisibility(false);
        mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, true);

        if (mTabGridDialogController != null) {
            // Don't wait until didSelectTab(), which is after the GTS animation.
            // We need to hide the dialog immediately.
            mTabGridDialogController.hideDialog(false);
        }
    }

    void registerFirstMeaningfulPaintRecorder() {
        ThreadUtils.assertOnUiThread();

        if (mFirstMeaningfulPaintRecorder == null) return;
        if (mRegisteredFirstMeaningfulPaintRecorder) return;
        mRegisteredFirstMeaningfulPaintRecorder = true;

        boolean hasTabs = false;
        if (mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter() != null
                && mTabModelSelector.getTabModelFilterProvider()
                           .getCurrentTabModelFilter()
                           .isTabModelRestored()) {
            hasTabs = mTabModelSelector.getTabModelFilterProvider()
                              .getCurrentTabModelFilter()
                              .getCount()
                    > 0;
        } else {
            List<PseudoTab> allTabs;
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                allTabs = PseudoTab.getAllPseudoTabsFromStateFile();
            }
            hasTabs = allTabs != null && !allTabs.isEmpty();
        }

        if (!hasTabs) {
            FirstDrawDetector.waitForFirstDraw(
                    mContainerView, this::notifyOnFirstMeaningfulPaintNoTab);
        } else {
            mTabContentManager.addOnLastThumbnailListener(this::notifyOnFirstMeaningfulPaint);
        }
    }

    private void notifyOnFirstMeaningfulPaintNoTab() {
        notifyOnFirstMeaningfulPaint(0);
    }

    private void notifyOnFirstMeaningfulPaint(int numOfThumbnails) {
        ThreadUtils.assertOnUiThread();

        mFirstMeaningfulPaintRecorder.record(numOfThumbnails);
        mFirstMeaningfulPaintRecorder = null;
    }

    boolean prepareOverview() {
        mHandler.removeCallbacks(mSoftClearTabListRunnable);
        mHandler.removeCallbacks(mClearTabListRunnable);
        boolean quick = false;
        TabModelFilter currentTabModelFilter =
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
        if (currentTabModelFilter != null && currentTabModelFilter.isTabModelRestored()) {
            if (TabUiFeatureUtilities.isTabToGtsAnimationEnabled()) {
                quick = mResetHandler.resetWithTabList(
                        currentTabModelFilter, false, mShowTabsInMruOrder);
            }
            setInitialScrollIndexOffset();
        }
        return quick;
    }

    private void setInitialScrollIndexOffset() {
        int initialPosition = Math.max(
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter().index()
                        - INITIAL_SCROLL_INDEX_OFFSET,
                0);
        // In MRU order, selected Tab is always at the first position.
        if (mShowTabsInMruOrder) initialPosition = 0;
        mContainerViewModel.set(INITIAL_SCROLL_INDEX, initialPosition);
    }

    @Override
    public void showOverview(boolean animate) {
        mHandler.removeCallbacks(mSoftClearTabListRunnable);
        mHandler.removeCallbacks(mClearTabListRunnable);
        if (mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter() != null
                && mTabModelSelector.getTabModelFilterProvider()
                           .getCurrentTabModelFilter()
                           .isTabModelRestored()) {
            mResetHandler.resetWithTabList(
                    mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter(),
                    TabUiFeatureUtilities.isTabToGtsAnimationEnabled(), mShowTabsInMruOrder);
            recordTabCounts();
        } else if (CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START)) {
            List<PseudoTab> allTabs;
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                allTabs = PseudoTab.getAllPseudoTabsFromStateFile();
            }
            mResetHandler.resetWithTabs(allTabs, TabUiFeatureUtilities.isTabToGtsAnimationEnabled(),
                    mShowTabsInMruOrder);
        }

        if (!animate) mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, false);
        setVisibility(true);
        mModelIndexWhenShown = mTabModelSelector.getCurrentModelIndex();
        mTabIdWhenShown = mTabModelSelector.getCurrentTabId();
        mContainerViewModel.set(ANIMATE_VISIBILITY_CHANGES, true);
    }

    @Override
    public void startedShowing(boolean isAnimating) {
        for (TabSwitcher.OverviewModeObserver observer : mObservers) {
            observer.startedShowing();
        }
    }

    @Override
    public void finishedShowing() {
        for (TabSwitcher.OverviewModeObserver observer : mObservers) {
            observer.finishedShowing();
        }
    }

    @Override
    public void startedHiding(boolean isAnimating) {
        for (TabSwitcher.OverviewModeObserver observer : mObservers) {
            observer.startedHiding();
        }
    }

    @Override
    public void finishedHiding() {
        for (TabSwitcher.OverviewModeObserver observer : mObservers) {
            observer.finishedHiding();
        }
    }

    @Override
    public boolean onBackPressed() {
        if (!mContainerViewModel.get(IS_VISIBLE)) return false;
        if (mTabSelectionEditorController != null
                && mTabSelectionEditorController.handleBackPressed()) {
            return true;
        }
        if (mTabGridDialogController != null && mTabGridDialogController.handleBackPressed()) {
            return true;
        }
        if (mTabModelSelector.getCurrentTab() == null) return false;

        recordUserSwitchedTab(
                mTabModelSelector.getCurrentTab(), mTabModelSelector.getCurrentTabId());
        onTabSelecting(mTabModelSelector.getCurrentTabId());

        return true;
    }

    @Override
    public void enableRecordingFirstMeaningfulPaint(long activityCreateTimeMs) {
        mFirstMeaningfulPaintRecorder = new FirstMeaningfulPaintRecorder(activityCreateTimeMs);
    }

    /**
     * Do clean-up work after the overview hiding animation is finished.
     * @see TabSwitcher.TabListDelegate#postHiding
     */
    void postHiding() {
        Log.d(TAG, "SoftCleanupDelay = " + getSoftCleanupDelay());
        mHandler.postDelayed(mSoftClearTabListRunnable, getSoftCleanupDelay());
        Log.d(TAG, "CleanupDelay = " + getCleanupDelay());
        mHandler.postDelayed(mClearTabListRunnable, getCleanupDelay());
    }

    /**
     * Set the delay for soft cleanup.
     */
    void setSoftCleanupDelayForTesting(int timeoutMs) {
        mSoftCleanupDelayMsForTesting = timeoutMs;
    }

    /**
     * Set the delay for lazy cleanup.
     */
    void setCleanupDelayForTesting(int timeoutMs) {
        mCleanupDelayMsForTesting = timeoutMs;
    }

    /**
     * Check if tabs should show in MRU order in current start surface tab switcher.
     *  @return whether tabs should show in MRU order
     */
    static boolean isShowingTabsInMRUOrder() {
        // TODO(crbug.com/1076449): Support MRU mode in Instant start.
        if (CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START)) return false;

        String feature = StartSurfaceConfiguration.START_SURFACE_VARIATION.getValue();
        return TextUtils.equals(feature, "twopanes");
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
        mBrowserControlsStateProvider.removeObserver(mBrowserControlsObserver);
        mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                mTabModelObserver);
    }

    void setOnTabSelectingListener(TabSwitcher.OnTabSelectingListener listener) {
        mOnTabSelectingListener = listener;
    }

    // GridCardOnClickListenerProvider implementation.
    @Override
    @Nullable
    public TabListMediator.TabActionListener openTabGridDialog(Tab tab) {
        if (!ableToOpenDialog(tab)) return null;
        assert getRelatedTabs(tab.getId()).size() != 1;
        assert mTabGridDialogController != null;
        return tabId -> {
            List<Tab> relatedTabs = getRelatedTabs(tabId);
            if (relatedTabs.size() == 0) {
                relatedTabs = null;
            }
            mTabGridDialogController.resetWithListOfTabs(relatedTabs);
            RecordUserAction.record("TabGridDialog.ExpandedFromSwitcher");
        };
    }

    @Override
    public void onTabSelecting(int tabId) {
        mIsSelectingInTabSwitcher = true;
        if (mOnTabSelectingListener != null) {
            mOnTabSelectingListener.onTabSelecting(LayoutManager.time(), tabId);
        }
    }

    private boolean ableToOpenDialog(Tab tab) {
        return TabUiFeatureUtilities.isTabGroupsAndroidEnabled()
                && mTabModelSelector.isIncognitoSelected() == tab.isIncognito()
                && getRelatedTabs(tab.getId()).size() != 1;
    }

    private List<Tab> getRelatedTabs(int tabId) {
        return mTabModelSelector.getTabModelFilterProvider()
                .getCurrentTabModelFilter()
                .getRelatedTabList(tabId);
    }

    private void recordTabCounts() {
        final TabModel model = mTabModelSelector.getCurrentModel();
        if (model == null) return;
        RecordHistogram.recordCountHistogram(TAB_COUNT_HISTOGRAM, model.getCount());

        final TabModelFilter filter =
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter();
        if (filter == null) return;
        RecordHistogram.recordCountHistogram(TAB_ENTRIES_HISTOGRAM, filter.getCount());
    }
}

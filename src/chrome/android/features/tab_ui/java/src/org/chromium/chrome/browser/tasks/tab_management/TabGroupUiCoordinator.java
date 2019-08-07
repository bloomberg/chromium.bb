// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider.SYNTHETIC_TRIAL_POSTFIX;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.init.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tabgroup.TabGroupModelFilter;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * A coordinator for TabGroupUi component. Manages the communication with
 * {@link TabListCoordinator}, {@link TabGridSheetCoordinator}, and
 * {@link TabStripToolbarCoordinator}, as well as the life-cycle of shared component objects.
 */
public class TabGroupUiCoordinator
        implements TabGroupUiMediator.ResetHandler, TabGroupUi, PauseResumeWithNativeObserver {
    final static String COMPONENT_NAME = "TabStrip";
    private final Context mContext;
    private final PropertyModel mTabStripToolbarModel;
    private final ThemeColorProvider mThemeColorProvider;
    private TabGridSheetCoordinator mTabGridSheetCoordinator;
    private TabListCoordinator mTabStripCoordinator;
    private TabGroupUiMediator mMediator;
    private TabStripToolbarCoordinator mTabStripToolbarCoordinator;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private ChromeActivity mActivity;

    /**
     * Creates a new {@link TabGroupUiCoordinator}
     */
    public TabGroupUiCoordinator(ViewGroup parentView, ThemeColorProvider themeColorProvider) {
        mContext = parentView.getContext();
        mThemeColorProvider = themeColorProvider;
        mTabStripToolbarModel = new PropertyModel(TabStripToolbarViewProperties.ALL_KEYS);

        mTabStripToolbarCoordinator =
                new TabStripToolbarCoordinator(mContext, parentView, mTabStripToolbarModel);
    }

    /**
     * Handle any initialization that occurs once native has been loaded.
     */
    @Override
    public void initializeWithNative(ChromeActivity activity,
            BottomControlsCoordinator.BottomControlsVisibilityController visibilityController) {
        if (ChromeFeatureList.isInitialized()) {
            UmaSessionStats.registerSyntheticFieldTrial(
                    ChromeFeatureList.TAB_GROUPS_ANDROID + SYNTHETIC_TRIAL_POSTFIX,
                    "Downloaded_Enabled");
        }
        assert activity instanceof ChromeTabbedActivity;
        mActivity = activity;
        TabModelSelector tabModelSelector = activity.getTabModelSelector();
        TabContentManager tabContentManager = activity.getTabContentManager();

        mTabStripCoordinator = new TabListCoordinator(TabListCoordinator.TabListMode.STRIP,
                mContext, tabModelSelector, null, null, false, null,
                mTabStripToolbarCoordinator.getTabListContainerView(), true, COMPONENT_NAME);

        mTabGridSheetCoordinator =
                new TabGridSheetCoordinator(mContext, activity.getBottomSheetController(),
                        tabModelSelector, tabContentManager, activity, mThemeColorProvider);

        mMediator = new TabGroupUiMediator(visibilityController, this, mTabStripToolbarModel,
                tabModelSelector, activity,
                ((ChromeTabbedActivity) activity).getOverviewModeBehavior(), mThemeColorProvider);
        mActivityLifecycleDispatcher = activity.getLifecycleDispatcher();
        mActivityLifecycleDispatcher.register(this);
    }

    /**
     * Handles a reset event originated from {@link TabGroupUiMediator}
     * when the bottom sheet is collapsed.
     *
     * @param tabs List of Tabs to reset.
     */
    @Override
    public void resetStripWithListOfTabs(List<Tab> tabs) {
        mTabStripCoordinator.resetWithListOfTabs(tabs);
    }

    /**
     * Handles a reset event originated from {@link TabGroupUiMediator}
     * when the bottom sheet is expanded.
     *
     * @param tabs List of Tabs to reset.
     */
    @Override
    public void resetSheetWithListOfTabs(List<Tab> tabs) {
        mTabGridSheetCoordinator.resetWithListOfTabs(tabs);
    }

    /**
     * Destroy any members that needs clean up.
     */
    @Override
    public void destroy() {
        // Early return if the component hasn't initialized yet.
        if (mActivity == null) return;

        mTabStripCoordinator.destroy();
        mTabGridSheetCoordinator.destroy();
        mMediator.destroy();
        mActivityLifecycleDispatcher.unregister(this);
    }

    // PauseResumeWithNativeObserver implementation.
    @Override
    public void onResumeWithNative() {
        // Since we use AsyncTask for restoring tabs, this method can be called before or after
        // restoring all tabs. Therefore, we skip recording the count here during cold start and
        // record that elsewhere when TabModel emits the restoreCompleted signal.
        if (!mActivity.isWarmOnResume()) return;

        TabModelFilterProvider provider =
                mActivity.getTabModelSelector().getTabModelFilterProvider();
        int groupCount =
                ((TabGroupModelFilter) provider.getTabModelFilter(true)).getTabGroupCount();
        groupCount += ((TabGroupModelFilter) provider.getTabModelFilter(false)).getTabGroupCount();
        RecordHistogram.recordCountHistogram("TabGroups.UserGroupCount", groupCount);

        recordSessionCount();
    }

    private void recordSessionCount() {
        if (mActivity.getOverviewModeBehavior() != null
                && mActivity.getOverviewModeBehavior().overviewVisible()) {
            return;
        }

        Tab currentTab = mActivity.getTabModelSelector().getCurrentTab();
        if (currentTab == null) return;
        TabModelFilterProvider provider =
                mActivity.getTabModelSelector().getTabModelFilterProvider();
        ((TabGroupModelFilter) provider.getCurrentTabModelFilter()).recordSessionsCount(currentTab);
    }

    @Override
    public void onPauseWithNative() {}
}

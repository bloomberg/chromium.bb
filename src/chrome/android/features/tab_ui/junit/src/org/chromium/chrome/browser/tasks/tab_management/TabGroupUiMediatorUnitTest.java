// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isA;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.View;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ThemeColorProvider;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.ConditionalTabStripUtils;
import org.chromium.chrome.browser.tasks.ConditionalTabStripUtils.FeatureStatus;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link TabGroupUiMediator}.
 */
@SuppressWarnings({"ResultOfMethodCallIgnored", "ArraysAsListWithZeroOrOneArgument", "unchecked"})
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupUiMediatorUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int TAB4_ID = 357;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final int POSITION3 = 2;
    private static final int TAB1_ROOT_ID = TAB1_ID;
    private static final int TAB2_ROOT_ID = TAB2_ID;
    private static final int TAB3_ROOT_ID = TAB2_ID;

    @Mock
    BottomControlsCoordinator.BottomControlsVisibilityController mVisibilityController;
    @Mock
    TabGroupUiMediator.ResetHandler mResetHandler;
    @Mock
    TabModelSelectorImpl mTabModelSelector;
    @Mock
    TabCreatorManager mTabCreatorManager;
    @Mock
    TabCreatorManager.TabCreator mTabCreator;
    @Mock
    OverviewModeBehavior mOverviewModeBehavior;
    @Mock
    ThemeColorProvider mThemeColorProvider;
    @Mock
    TabModel mTabModel;
    @Mock
    View mView;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    TabGroupModelFilter mTabGroupModelFilter;
    @Mock
    TabModelFilter mTabModelFilter;
    @Mock
    TabGridDialogMediator.DialogController mTabGridDialogController;
    @Mock
    ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock
    Context mContext;
    @Mock
    SnackbarManager.SnackbarManageable mSnackbarManageable;
    @Mock
    SnackbarManager mSnackbarManager;
    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<OverviewModeBehavior.OverviewModeObserver> mOverviewModeObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<ThemeColorProvider.ThemeColorObserver> mThemeColorObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<ThemeColorProvider.TintObserver> mTintObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<TabGroupModelFilter.Observer> mTabGroupModelFilterObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<PauseResumeWithNativeObserver> mPauseResumeWithNativeObserverArgumentCaptor;
    @Captor
    ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private TabImpl mTab1;
    private TabImpl mTab2;
    private TabImpl mTab3;
    private List<Tab> mTabGroup1;
    private List<Tab> mTabGroup2;
    private List<Tab> mAllTabsList;
    private PropertyModel mModel;
    private TabGroupUiMediator mTabGroupUiMediator;
    private InOrder mResetHandlerInOrder;
    private InOrder mVisibilityControllerInOrder;

    private TabImpl prepareTab(int tabId, int rootId) {
        TabImpl tab = mock(TabImpl.class);
        doReturn(tabId).when(tab).getId();
        doReturn(rootId).when(tab).getRootId();
        doReturn(tab).when(mTabModelSelector).getTabById(tabId);
        return tab;
    }

    private TabModel prepareIncognitoTabModel() {
        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(newTab));
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB4_ID);
        TabModel incognitoTabModel = mock(TabModel.class);
        doReturn(newTab).when(incognitoTabModel).getTabAt(POSITION1);
        doReturn(true).when(incognitoTabModel).isIncognito();
        doReturn(1).when(incognitoTabModel).getCount();
        return incognitoTabModel;
    }

    private void verifyNeverReset() {
        mResetHandlerInOrder.verify(mResetHandler, never()).resetStripWithListOfTabs(any());
        mVisibilityControllerInOrder.verify(mVisibilityController, never())
                .setBottomControlsVisible(anyBoolean());
    }

    private void verifyResetStrip(boolean isVisible, @Nullable List<Tab> tabs) {
        mResetHandlerInOrder.verify(mResetHandler).resetStripWithListOfTabs(tabs);
        mVisibilityControllerInOrder.verify(mVisibilityController)
                .setBottomControlsVisible(isVisible);
    }

    private void initAndAssertProperties(@Nullable Tab currentTab) {
        if (currentTab == null) {
            doReturn(TabModel.INVALID_TAB_INDEX).when(mTabModel).index();
            doReturn(0).when(mTabModel).getCount();
            doReturn(0).when(mTabGroupModelFilter).getCount();
            doReturn(null).when(mTabModelSelector).getCurrentTab();
        } else {
            doReturn(mTabModel.indexOf(currentTab)).when(mTabModel).index();
            doReturn(currentTab).when(mTabModelSelector).getCurrentTab();
        }

        TabGridDialogMediator.DialogController controller =
                TabUiFeatureUtilities.isTabGroupsAndroidEnabled() ? mTabGridDialogController : null;
        mTabGroupUiMediator = new TabGroupUiMediator(mContext, mVisibilityController, mResetHandler,
                mModel, mTabModelSelector, mTabCreatorManager, mOverviewModeBehavior,
                mThemeColorProvider, controller, mActivityLifecycleDispatcher, mSnackbarManageable);

        if (currentTab == null) {
            verifyNeverReset();
            return;
        }

        // Verify strip button content description setup.
        if (TabUiFeatureUtilities.isConditionalTabStripEnabled()) {
            verify(mContext).getString(R.string.accessibility_bottom_tab_strip_close_strip);
            verify(mContext).getString(R.string.accessibility_toolbar_btn_new_tab);
        } else {
            verify(mContext).getString(R.string.accessibility_bottom_tab_strip_expand_tab_sheet);
            verify(mContext).getString(R.string.bottom_tab_grid_new_tab);
        }

        // Verify strip initial reset.
        if (TabUiFeatureUtilities.isConditionalTabStripEnabled()) {
            verifyResetStrip(false, null);
            assertThat(mTabGroupUiMediator.getConditionalTabStripFeatureStatusForTesting(),
                    equalTo(FeatureStatus.DEFAULT));
        } else {
            List<Tab> tabs = mTabGroupModelFilter.getRelatedTabList(currentTab.getId());
            if (tabs.size() < 2) {
                verifyResetStrip(false, null);
            } else {
                verifyResetStrip(true, tabs);
            }
        }
    }

    @Before
    public void setUp() {
        // After setUp(), tabModel has 3 tabs in the following order: mTab1, mTab2 and mTab3. If
        // TabGroup is enabled, mTab2 and mTab3 are in a group. Each test must call
        // initAndAssertProperties(selectedTab) first, with selectedTab being the currently selected
        // tab when the TabGroupUiMediator is created.
        MockitoAnnotations.initMocks(this);

        // Set up Tabs
        mTab1 = prepareTab(TAB1_ID, TAB1_ROOT_ID);
        mTab2 = prepareTab(TAB2_ID, TAB2_ROOT_ID);
        mTab3 = prepareTab(TAB3_ID, TAB3_ROOT_ID);
        mTabGroup1 = new ArrayList<>(Arrays.asList(mTab1));
        mTabGroup2 = new ArrayList<>(Arrays.asList(mTab2, mTab3));
        mAllTabsList = new ArrayList<>(Arrays.asList(mTab1, mTab2, mTab3));

        // Setup TabModel.
        doReturn(mTabModel).when(mTabModel).getComprehensiveModel();
        doReturn(mTabModel).when(mTabModelSelector).getModel(false);
        doReturn(false).when(mTabModel).isIncognito();
        doReturn(mTabModel).when(mTabModelSelector).getModel(false);
        doReturn(3).when(mTabModel).getCount();
        doReturn(0).when(mTabModel).index();
        doReturn(mTab1).when(mTabModel).getTabAt(0);
        doReturn(mTab2).when(mTabModel).getTabAt(1);
        doReturn(mTab3).when(mTabModel).getTabAt(2);
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doReturn(POSITION3).when(mTabModel).indexOf(mTab3);
        doNothing().when(mTab1).addObserver(mTabObserverCaptor.capture());
        doNothing().when(mTab2).addObserver(mTabObserverCaptor.capture());
        doNothing().when(mTab3).addObserver(mTabObserverCaptor.capture());

        if (TabUiFeatureUtilities.isConditionalTabStripEnabled()) {
            doReturn(false).when(mTabModelFilter).isIncognito();
            doReturn(3).when(mTabModelFilter).getCount();
            doReturn(new ArrayList<>(Arrays.asList(mTab1)))
                    .when(mTabModelFilter)
                    .getRelatedTabList(TAB1_ID);
            doReturn(new ArrayList<>(Arrays.asList(mTab1)))
                    .when(mTabModelFilter)
                    .getRelatedTabList(TAB2_ID);
            doReturn(new ArrayList<>(Arrays.asList(mTab1)))
                    .when(mTabModelFilter)
                    .getRelatedTabList(TAB3_ID);

            doReturn(mTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
            doReturn(mTabModelFilter).when(mTabModelFilterProvider).getTabModelFilter(true);
            doReturn(mTabModelFilter).when(mTabModelFilterProvider).getTabModelFilter(false);
            doNothing()
                    .when(mActivityLifecycleDispatcher)
                    .register(mPauseResumeWithNativeObserverArgumentCaptor.capture());
        } else {
            // Setup TabGroupModelFilter.
            doReturn(false).when(mTabGroupModelFilter).isIncognito();
            doReturn(2).when(mTabGroupModelFilter).getCount();
            doReturn(mTabGroup1).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
            doReturn(mTabGroup2).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
            doReturn(mTabGroup2).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);

            doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
            doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(true);
            doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(false);
            doNothing()
                    .when(mTabGroupModelFilter)
                    .addTabGroupObserver(mTabGroupModelFilterObserverArgumentCaptor.capture());
        }

        // Set up TabModelSelector and TabModelFilterProvider.
        List<TabModel> tabModelList = new ArrayList<>();
        tabModelList.add(mTabModel);
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doReturn(TAB1_ID).when(mTabModelSelector).getCurrentTabId();
        doNothing()
                .when(mTabModelSelector)
                .addObserver(mTabModelSelectorObserverArgumentCaptor.capture());
        doReturn(tabModelList).when(mTabModelSelector).getModels();

        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doNothing()
                .when(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverArgumentCaptor.capture());


        // Set up OverviewModeBehavior
        doNothing()
                .when(mOverviewModeBehavior)
                .addOverviewModeObserver(mOverviewModeObserverArgumentCaptor.capture());

        // Set up ThemeColorProvider
        doNothing()
                .when(mThemeColorProvider)
                .addThemeColorObserver(mThemeColorObserverArgumentCaptor.capture());
        doNothing()
                .when(mThemeColorProvider)
                .addTintObserver(mTintObserverArgumentCaptor.capture());

        // Set up ResetHandler
        doNothing().when(mResetHandler).resetStripWithListOfTabs(any());
        doNothing().when(mResetHandler).resetGridWithListOfTabs(any());

        // Set up TabCreatorManager
        doReturn(mTabCreator).when(mTabCreatorManager).getTabCreator(anyBoolean());
        doReturn(null).when(mTabCreator).createNewTab(any(), anyInt(), any());

        // Set up SnackbarManageable.
        doReturn(mSnackbarManager).when(mSnackbarManageable).getSnackbarManager();

        mResetHandlerInOrder = inOrder(mResetHandler);
        mVisibilityControllerInOrder = inOrder(mVisibilityController);
        mModel = new PropertyModel(TabGroupUiProperties.ALL_KEYS);
    }

    @After
    public void tearDown() {
        ConditionalTabStripUtils.setFeatureStatus(FeatureStatus.DEFAULT);
    }

    /*********************** Tab group related tests *************************/

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void verifyInitialization_NoTab_TabGroup() {
        initAndAssertProperties(null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void verifyInitialization_SingleTab() {
        initAndAssertProperties(mTab1);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void verifyInitialization_TabGroup() {
        // Tab 2 is in a tab group.
        initAndAssertProperties(mTab2);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void onClickLeftButton_TabGroup() {
        initAndAssertProperties(mTab2);

        View.OnClickListener listener =
                mModel.get(TabGroupUiProperties.LEFT_BUTTON_ON_CLICK_LISTENER);
        assertThat(listener, instanceOf(View.OnClickListener.class));

        listener.onClick(mView);

        verify(mResetHandler).resetGridWithListOfTabs(mTabGroup2);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void onClickRightButton_TabGroup() {
        initAndAssertProperties(mTab1);

        View.OnClickListener listener =
                mModel.get(TabGroupUiProperties.RIGHT_BUTTON_ON_CLICK_LISTENER);
        assertThat(listener, instanceOf(View.OnClickListener.class));

        listener.onClick(mView);

        verify(mTabCreator)
                .createNewTab(
                        isA(LoadUrlParams.class), eq(TabLaunchType.FROM_CHROME_UI), eq(mTab1));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabSelection_NotSameGroup_GroupToSingleTab() {
        initAndAssertProperties(mTab2);

        // Mock selecting tab 1, and the last selected tab is tab 2 which is in different group.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab1, TabSelectionType.FROM_USER, TAB2_ID);

        // Strip should not be showing since tab 1 is a single tab.
        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabSelection_NotSameGroup_GroupToGroup() {
        initAndAssertProperties(mTab2);

        // Mock that tab 1 is not a single tab.
        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);

        // Mock selecting tab 1, and the last selected tab is tab 2 which is in different group.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab1, TabSelectionType.FROM_USER, TAB2_ID);

        // Strip should be showing since we are selecting a group.
        verifyResetStrip(true, tabs);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabSelection_NotSameGroup_SingleTabToGroup() {
        initAndAssertProperties(mTab1);

        // Mock that tab 2 is not a single tab.
        List<Tab> tabGroup = mTabGroupModelFilter.getRelatedTabList(TAB2_ID);
        assertThat(tabGroup.size(), equalTo(2));

        // Mock selecting tab 2, and the last selected tab is tab 1 which is a single tab.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab2, TabSelectionType.FROM_USER, TAB1_ID);

        // Strip should be showing since we are selecting a group.
        verifyResetStrip(true, tabGroup);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabSelection_NotSameGroup_SingleTabToSingleTab() {
        initAndAssertProperties(mTab1);

        // Mock that new tab is a single tab.
        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(newTab));
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB4_ID);

        // Mock selecting new tab, and the last selected tab is tab 1 which is also a single tab.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                newTab, TabSelectionType.FROM_USER, TAB1_ID);

        // Strip should not be showing since new tab is a single tab.
        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabSelection_SameGroup_TabGroup() {
        initAndAssertProperties(mTab2);

        // Mock selecting tab 3, and the last selected tab is tab 2 which is in the same group.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab3, TabSelectionType.FROM_USER, TAB2_ID);

        // Strip should not be reset since we are selecting in one group.
        verifyNeverReset();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabSelection_ScrollToSelectedIndex() {
        initAndAssertProperties(mTab1);
        assertThat(mModel.get(TabGroupUiProperties.INITIAL_SCROLL_INDEX), equalTo(null));

        // Mock that {tab2, tab3} are in the same tab group.
        List<Tab> tabGroup = mTabGroupModelFilter.getRelatedTabList(TAB2_ID);
        assertThat(tabGroup.size(), equalTo(2));

        // Mock selecting tab 3, and the last selected tab is tab 1 which is a single tab.
        doReturn(mTab3).when(mTabModelSelector).getCurrentTab();
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab3, TabSelectionType.FROM_USER, TAB1_ID);

        // Strip should be showing since we are selecting a group, and it should scroll to the index
        // of currently selected tab.
        verifyResetStrip(true, tabGroup);
        assertThat(mModel.get(TabGroupUiProperties.INITIAL_SCROLL_INDEX), equalTo(1));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabClosure_NotLastTabInGroup() {
        initAndAssertProperties(mTab2);

        // Mock closing tab 2, and tab 3 then gets selected. They are in the same group.
        mTabModelObserverArgumentCaptor.getValue().willCloseTab(mTab2, true);
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab3, TabSelectionType.FROM_CLOSE, TAB2_ID);

        // Strip should not be reset since we are still in this group.
        verifyNeverReset();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabClosure_LastTabInGroup_GroupUiNotVisible() {
        initAndAssertProperties(mTab1);

        // Mock closing tab 1, and tab 2 then gets selected. They are in different group.
        mTabModelObserverArgumentCaptor.getValue().willCloseTab(mTab1, true);
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab2, TabSelectionType.FROM_CLOSE, TAB1_ID);

        // Strip should never be reset since currently tab group UI is invisible.
        verifyNeverReset();
    }

    // TODO(988199): Ignore this test until we have a conclusion from the attached bug.
    @Ignore
    @Test
    public void tabClosure_LastTabInGroup_GroupUiVisible() {
        initAndAssertProperties(mTab2);

        // Mock closing tab 2 and tab, then tab 1 gets selected. They are in different group. Right
        // now tab group UI is visible.
        mTabModelObserverArgumentCaptor.getValue().willCloseTab(mTab2, true);
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab3, TabSelectionType.FROM_CLOSE, TAB2_ID);
        doReturn(new ArrayList<>()).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        mTabModelObserverArgumentCaptor.getValue().willCloseTab(mTab3, true);
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab1, TabSelectionType.FROM_CLOSE, TAB3_ID);

        // Strip should be reset since tab group UI was visible and now we are switching to a
        // different group.
        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabAddition_SingleTab() {
        initAndAssertProperties(mTab1);

        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(newTab));
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB4_ID);

        mTabModelObserverArgumentCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelObserverArgumentCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_RESTORE, TabCreationState.FROZEN_ON_RESTORE);

        // Strip should be not be reset when adding a single new tab.
        verifyNeverReset();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabAddition_TabGroup_NoRefresh() {
        initAndAssertProperties(mTab2);

        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        mTabGroup2.add(newTab);
        doReturn(mTabGroup1).when(mTabGroupModelFilter).getRelatedTabList(TAB4_ID);

        mTabModelObserverArgumentCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModelObserverArgumentCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_RESTORE, TabCreationState.FROZEN_ON_RESTORE);

        // Strip should be not be reset through these two types of launching.
        verifyNeverReset();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabAddition_TabGroup_Refresh() {
        initAndAssertProperties(mTab2);

        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        mTabGroup2.add(newTab);
        doReturn(mTabGroup2).when(mTabGroupModelFilter).getRelatedTabList(TAB4_ID);

        mTabModelObserverArgumentCaptor.getValue().didAddTab(newTab,
                TabLaunchType.FROM_LONGPRESS_BACKGROUND, TabCreationState.LIVE_IN_FOREGROUND);

        // Strip should be be reset when long pressing a link and add a tab into group.
        verifyResetStrip(true, mTabGroup2);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabAddition_TabGroup_ScrollToTheLast() {
        initAndAssertProperties(mTab2);
        assertThat(mModel.get(TabGroupUiProperties.INITIAL_SCROLL_INDEX), equalTo(0));

        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        mTabGroup2.add(newTab);
        doReturn(mTabGroup2).when(mTabGroupModelFilter).getRelatedTabList(TAB4_ID);

        mTabModelObserverArgumentCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);

        // Strip should be not be reset through adding tab from UI.
        verifyNeverReset();
        assertThat(mTabGroupModelFilter.getRelatedTabList(TAB4_ID).size(), equalTo(3));
        assertThat(mModel.get(TabGroupUiProperties.INITIAL_SCROLL_INDEX), equalTo(2));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void restoreCompleted_TabModelNoTab() {
        // Simulate no tab in current TabModel.
        initAndAssertProperties(null);

        // Simulate restore finished.
        mTabModelObserverArgumentCaptor.getValue().restoreCompleted();

        mVisibilityControllerInOrder.verify(mVisibilityController, never())
                .setBottomControlsVisible(anyBoolean());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void restoreCompleted_UiNotVisible() {
        // Assume mTab1 is selected, and it does not have related tabs.
        initAndAssertProperties(mTab1);
        doReturn(POSITION1).when(mTabModel).index();
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        // Simulate restore finished.
        mTabModelObserverArgumentCaptor.getValue().restoreCompleted();

        mVisibilityControllerInOrder.verify(mVisibilityController).setBottomControlsVisible(false);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void restoreCompleted_UiVisible() {
        // Assume mTab2 is selected, and it has related tabs mTab2 and mTab3.
        initAndAssertProperties(mTab2);
        doReturn(POSITION2).when(mTabModel).index();
        doReturn(mTab2).when(mTabModelSelector).getCurrentTab();
        // Simulate restore finished.
        mTabModelObserverArgumentCaptor.getValue().restoreCompleted();

        mVisibilityControllerInOrder.verify(mVisibilityController).setBottomControlsVisible(true);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void restoreCompleted_OverviewModeVisible() {
        // Assume mTab2 is selected, and it has related tabs mTab2 and mTab3. Also, the overview
        // mode is visible when restoring completed.
        initAndAssertProperties(mTab2);
        doReturn(POSITION2).when(mTabModel).index();
        doReturn(mTab2).when(mTabModelSelector).getCurrentTab();
        doReturn(true).when(mOverviewModeBehavior).overviewVisible();
        // Simulate restore finished.
        mTabModelObserverArgumentCaptor.getValue().restoreCompleted();

        mVisibilityControllerInOrder.verify(mVisibilityController, never())
                .setBottomControlsVisible(anyBoolean());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabClosureUndone_UiVisible_NotShowingOverviewMode() {
        // Assume mTab2 is selected, and it has related tabs mTab2 and mTab3.
        initAndAssertProperties(mTab2);
        // OverviewMode is hiding by default.
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(false));

        // Simulate that another member of this group, newTab, is being undone from closure.
        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        doReturn(new ArrayList<>(Arrays.asList(mTab2, mTab3, newTab)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB4_ID);

        mTabModelObserverArgumentCaptor.getValue().tabClosureUndone(newTab);

        // Since the strip is already visible, no resetting.
        mVisibilityControllerInOrder.verify(mVisibilityController, never())
                .setBottomControlsVisible(anyBoolean());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabClosureUndone_UiNotVisible_NotShowingOverviewMode() {
        // Assume mTab1 is selected. Since mTab1 is now a single tab, the strip is invisible.
        initAndAssertProperties(mTab1);
        // OverviewMode is hiding by default.
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(false));

        // Simulate that newTab which was a tab in the same group as mTab1 is being undone from
        // closure.
        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        doReturn(new ArrayList<>(Arrays.asList(mTab1, newTab)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB4_ID);

        mTabModelObserverArgumentCaptor.getValue().tabClosureUndone(newTab);

        // Strip should reset to be visible.
        mVisibilityControllerInOrder.verify(mVisibilityController)
                .setBottomControlsVisible(eq(true));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void tabClosureUndone_UiNotVisible_ShowingOverviewMode() {
        // Assume mTab1 is selected.
        initAndAssertProperties(mTab1);
        // OverviewMode is hiding by default.
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(false));

        // Simulate the overview mode is showing, which hides the strip.
        mOverviewModeObserverArgumentCaptor.getValue().onOverviewModeStartedShowing(true);
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(true));
        mVisibilityControllerInOrder.verify(mVisibilityController).setBottomControlsVisible(false);

        // Simulate that we undo a group closure of {mTab2, mTab3}.
        mTabModelObserverArgumentCaptor.getValue().tabClosureUndone(mTab3);
        mTabModelObserverArgumentCaptor.getValue().tabClosureUndone(mTab2);

        // Since overview mode is showing, we should not show strip.
        mVisibilityControllerInOrder.verify(mVisibilityController, times(2))
                .setBottomControlsVisible(eq(false));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void overViewStartedShowing() {
        initAndAssertProperties(mTab1);

        mOverviewModeObserverArgumentCaptor.getValue().onOverviewModeStartedShowing(true);

        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void overViewFinishedHiding_NoCurrentTab() {
        initAndAssertProperties(null);

        mOverviewModeObserverArgumentCaptor.getValue().onOverviewModeFinishedHiding();

        verifyNeverReset();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void overViewFinishedHiding_CurrentTabSingle() {
        initAndAssertProperties(mTab1);

        mOverviewModeObserverArgumentCaptor.getValue().onOverviewModeFinishedHiding();

        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void overViewFinishedHiding_CurrentTabInGroup() {
        initAndAssertProperties(mTab2);

        mOverviewModeObserverArgumentCaptor.getValue().onOverviewModeFinishedHiding();

        verifyResetStrip(true, mTabGroup2);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void destroy_TabGroup() {
        initAndAssertProperties(mTab1);

        mTabGroupUiMediator.destroy();

        verify(mTabModelFilterProvider)
                .removeTabModelFilterObserver(mTabModelObserverArgumentCaptor.capture());
        verify(mOverviewModeBehavior)
                .removeOverviewModeObserver(mOverviewModeObserverArgumentCaptor.capture());
        verify(mThemeColorProvider)
                .removeThemeColorObserver(mThemeColorObserverArgumentCaptor.capture());
        verify(mThemeColorProvider).removeTintObserver(mTintObserverArgumentCaptor.capture());
        verify(mTabModelSelector).removeObserver(mTabModelSelectorObserverArgumentCaptor.capture());
        verify(mTabGroupModelFilter, times(2))
                .removeTabGroupObserver(mTabGroupModelFilterObserverArgumentCaptor.capture());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void uiNotVisibleAfterDragCurrentTabOutOfGroup() {
        initAndAssertProperties(mTab3);

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab3));
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB3_ID);
        mTabGroupModelFilterObserverArgumentCaptor.getValue().didMoveTabOutOfGroup(mTab3, 1);

        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void backButtonPress_ShouldHandle() {
        initAndAssertProperties(mTab1);
        doReturn(true).when(mTabGridDialogController).handleBackPressed();

        assertThat(mTabGroupUiMediator.onBackPressed(), equalTo(true));
        verify(mTabGridDialogController).handleBackPressed();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void backButtonPress_ShouldNotHandle() {
        initAndAssertProperties(mTab1);
        doReturn(false).when(mTabGridDialogController).handleBackPressed();

        assertThat(mTabGroupUiMediator.onBackPressed(), equalTo(false));
        verify(mTabGridDialogController).handleBackPressed();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void switchTabModel_UiVisible_TabGroup() {
        initAndAssertProperties(mTab1);
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(false));
        TabModel incognitoTabModel = prepareIncognitoTabModel();

        // Mock that tab2 is selected after tab model switch, and tab2 is in a group.
        doReturn(TAB2_ID).when(mTabModelSelector).getCurrentTabId();
        mTabModelSelectorObserverArgumentCaptor.getValue().onTabModelSelected(
                mTabModel, incognitoTabModel);

        verifyResetStrip(true, mTabGroup2);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_ANDROID)
    public void switchTabModel_UiNotVisible_TabGroup() {
        initAndAssertProperties(mTab1);
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(false));
        TabModel incognitoTabModel = prepareIncognitoTabModel();

        // Mock that tab1 is selected after tab model switch, and tab1 is a single tab.
        doReturn(TAB1_ID).when(mTabModelSelector).getCurrentTabId();
        mTabModelSelectorObserverArgumentCaptor.getValue().onTabModelSelected(
                mTabModel, incognitoTabModel);

        verifyResetStrip(false, null);
    }

    /*********************** Conditional tab strip related tests *************************/

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void verifyInitialization_CTS() {
        initAndAssertProperties(mTab1);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void onClickAdd_CTS() {
        initAndAssertProperties(mTab1);

        View.OnClickListener listener =
                mModel.get(TabGroupUiProperties.RIGHT_BUTTON_ON_CLICK_LISTENER);
        assertThat(listener, instanceOf(View.OnClickListener.class));

        listener.onClick(mView);

        verify(mTabCreator)
                .createNewTab(isA(LoadUrlParams.class), eq(TabLaunchType.FROM_CHROME_UI), eq(null));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void onClickDismiss_CTS() {
        initAndAssertProperties(mTab1);

        View.OnClickListener listener =
                mModel.get(TabGroupUiProperties.LEFT_BUTTON_ON_CLICK_LISTENER);
        assertThat(listener, instanceOf(View.OnClickListener.class));

        listener.onClick(mView);

        verifyResetStrip(false, null);
        assertThat(mTabGroupUiMediator.getConditionalTabStripFeatureStatusForTesting(),
                equalTo(FeatureStatus.FORBIDDEN));
        verify(mSnackbarManager).showSnackbar(any(Snackbar.class));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void tabSelection_FromAddition_CTS() {
        initAndAssertProperties(mTab1);

        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab2, TabSelectionType.FROM_NEW, TAB1_ID);

        // Strip should not be showing since this is a selection due to addition.
        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void tabSelection_SameTab_CTS() {
        initAndAssertProperties(mTab1);

        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab1, TabSelectionType.FROM_USER, TAB1_ID);

        // Strip should not be showing since this is a selection of the same tab.
        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void tabSelection_SameTab_NotDismissSnackbar_CTS() {
        initAndAssertProperties(mTab1);

        // Mock that undo snackbar is showing.
        doReturn(true).when(mSnackbarManager).isShowing();

        // Select the same tab.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab1, TabSelectionType.FROM_USER, TAB1_ID);

        // Verify that snackbar is not dismissed since there is no different tab selection.
        verify(mSnackbarManager, never()).dismissSnackbars(eq(mTabGroupUiMediator));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void tabSelection_DifferentTab_DismissSnackbar_CTS() {
        initAndAssertProperties(mTab1);

        // Mock that undo snackbar is showing.
        doReturn(true).when(mSnackbarManager).isShowing();

        // Select a different tab.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab1, TabSelectionType.FROM_USER, TAB2_ID);

        // Verify that snackbar is dismissed when a different tab is selected.
        verify(mSnackbarManager).dismissSnackbars(eq(mTabGroupUiMediator));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void tabSelection_AfterAddition_CTS() {
        initAndAssertProperties(mTab1);

        // When a tab is created from clicking the web content, there will be two subsequent
        // didSelectTab calls, one with type TabSelectionType.FROM_NEW the other with type
        // TabSelectionType.FROM_USER. We should skip this type of selection signal since it might
        // be an unintentional tab interaction.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab2, TabSelectionType.FROM_NEW, TAB1_ID);
        verifyResetStrip(false, null);

        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab2, TabSelectionType.FROM_USER, TAB1_ID);
        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void tabSelection_FromUser_CTS() {
        initAndAssertProperties(mTab1);

        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab2, TabSelectionType.FROM_USER, TAB1_ID);
        verifyResetStrip(true, mAllTabsList);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void tabClosure_NotLastTab_CTS() {
        initAndAssertProperties(mTab1);

        // Trigger tab strip with a selection.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab2, TabSelectionType.FROM_USER, TAB1_ID);
        verifyResetStrip(true, mAllTabsList);

        // Close tab1 and there are two tabs left.
        mTabModelObserverArgumentCaptor.getValue().willCloseTab(mTab1, false);
        verifyNeverReset();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void tabClosure_LastTab_CTS() {
        initAndAssertProperties(mTab1);

        // Trigger tab strip with a selection.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab2, TabSelectionType.FROM_USER, TAB1_ID);
        verifyResetStrip(true, mAllTabsList);

        // Close tab2 and tab3 so that there is only one tab left.
        mTabModelObserverArgumentCaptor.getValue().willCloseTab(mTab3, false);
        verifyNeverReset();
        doReturn(2).when(mTabModel).getCount();
        mTabModelObserverArgumentCaptor.getValue().willCloseTab(mTab2, false);
        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void tabAddition_Unintentional_CTS() {
        initAndAssertProperties(mTab1);

        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        mTabModelObserverArgumentCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_EXTERNAL_APP, TabCreationState.LIVE_IN_FOREGROUND);

        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void tabAddition_FromUI_CTS() {
        initAndAssertProperties(mTab1);

        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        doReturn(4).when(mTabModel).getCount();
        doReturn(newTab).when(mTabModel).getTabAt(3);
        mAllTabsList.add(newTab);

        // The reset will be triggered by the subsequent tab selection instead of tab addition.
        mTabModelObserverArgumentCaptor.getValue().didAddTab(
                newTab, TabLaunchType.FROM_CHROME_UI, TabCreationState.LIVE_IN_FOREGROUND);
        verifyNeverReset();
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                newTab, TabLaunchType.FROM_CHROME_UI, TAB1_ID);
        verifyResetStrip(true, mAllTabsList);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void tabAddition_FromLongPressBackground_CTS() {
        initAndAssertProperties(mTab1);

        TabImpl newTab = prepareTab(TAB4_ID, TAB4_ID);
        doReturn(4).when(mTabModel).getCount();
        doReturn(newTab).when(mTabModel).getTabAt(3);
        mAllTabsList.add(newTab);

        // The reset will be triggered by tab addition because there is no subsequent selection.
        mTabModelObserverArgumentCaptor.getValue().didAddTab(newTab,
                TabLaunchType.FROM_LONGPRESS_BACKGROUND, TabCreationState.LIVE_IN_FOREGROUND);
        verifyResetStrip(true, mAllTabsList);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void restoreCompleted_NotTrigger_CTS() {
        initAndAssertProperties(mTab1);
        // Assume that the tab model is being restored. Also, the overview
        // mode is invisible when restoring is completed.
        doReturn(POSITION1).when(mTabModel).index();
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doReturn(false).when(mOverviewModeBehavior).overviewVisible();

        // Simulate restore finished.
        mTabModelObserverArgumentCaptor.getValue().restoreCompleted();

        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void tabClosureUndone_UiVisible_NotShowingOverviewMode_CTS() {
        initAndAssertProperties(mTab1);
        // OverviewMode is hiding by default.
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(false));

        // Trigger tab strip with a selection.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab2, TabSelectionType.FROM_USER, TAB1_ID);
        verifyResetStrip(true, mAllTabsList);

        // Simulate that we undo the closure of mTab3.
        mTabModelObserverArgumentCaptor.getValue().tabClosureUndone(mTab3);

        // Since the strip is already visible, no resetting.
        verifyNeverReset();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void tabClosureUndone_UiNotVisible_ShowingOverviewMode_CTS() {
        // Assume mTab1 is selected.
        initAndAssertProperties(mTab1);
        // OverviewMode is hiding by default.
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(false));

        // Simulate the overview mode is showing, which hides the strip.
        mOverviewModeObserverArgumentCaptor.getValue().onOverviewModeStartedShowing(true);
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(true));
        verifyResetStrip(false, null);

        // Simulate that we undo the closure of mTab3.
        mTabModelObserverArgumentCaptor.getValue().tabClosureUndone(mTab3);

        // Since overview mode is showing, we should not show strip.
        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void overViewStartedShowing_CTS() {
        initAndAssertProperties(mTab1);

        mOverviewModeObserverArgumentCaptor.getValue().onOverviewModeStartedShowing(true);

        // Showing overview should activate the feature and hide the strip.
        assertThat(mTabGroupUiMediator.getConditionalTabStripFeatureStatusForTesting(),
                equalTo(FeatureStatus.ACTIVATED));
        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void overViewFinishedHiding_CTS() {
        initAndAssertProperties(mTab1);

        // Show the overview first to activate feature.
        mOverviewModeObserverArgumentCaptor.getValue().onOverviewModeStartedShowing(true);
        assertThat(mTabGroupUiMediator.getConditionalTabStripFeatureStatusForTesting(),
                equalTo(FeatureStatus.ACTIVATED));
        verifyResetStrip(false, null);

        // Hide the overview should trigger tab strip show.
        mOverviewModeObserverArgumentCaptor.getValue().onOverviewModeFinishedHiding();
        verifyResetStrip(true, mAllTabsList);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void onPauseWithNative_CTS() {
        initAndAssertProperties(mTab1);

        mPauseResumeWithNativeObserverArgumentCaptor.getValue().onPauseWithNative();

        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void onResumeWithNative_featureActivated_CTS() {
        initAndAssertProperties(mTab1);

        ConditionalTabStripUtils.setFeatureStatus(FeatureStatus.ACTIVATED);
        mPauseResumeWithNativeObserverArgumentCaptor.getValue().onResumeWithNative();

        verifyResetStrip(true, mAllTabsList);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void onResumeWithNative_featureForbidden_CTS() {
        initAndAssertProperties(mTab1);

        ConditionalTabStripUtils.setFeatureStatus(FeatureStatus.FORBIDDEN);
        mPauseResumeWithNativeObserverArgumentCaptor.getValue().onResumeWithNative();

        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void onResumeWithNative_featureDefault_CTS() {
        initAndAssertProperties(mTab1);

        ConditionalTabStripUtils.setFeatureStatus(FeatureStatus.DEFAULT);
        mPauseResumeWithNativeObserverArgumentCaptor.getValue().onResumeWithNative();

        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void onResumeWithNative_featureActivated_ShowingOverviewMode_CTS() {
        initAndAssertProperties(mTab1);

        // Mock that the overview mode is showing.
        mOverviewModeObserverArgumentCaptor.getValue().onOverviewModeStartedShowing(true);
        verifyResetStrip(false, null);
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(true));

        // When overview mode is showing before native resume, i,e. ReturnToClank enabled, strip
        // should not be showing.
        ConditionalTabStripUtils.setFeatureStatus(FeatureStatus.ACTIVATED);
        mPauseResumeWithNativeObserverArgumentCaptor.getValue().onResumeWithNative();

        verifyResetStrip(false, null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void destroy_CTS() {
        initAndAssertProperties(mTab1);

        mTabGroupUiMediator.destroy();

        verify(mTabModelFilterProvider)
                .removeTabModelFilterObserver(mTabModelObserverArgumentCaptor.capture());
        verify(mOverviewModeBehavior)
                .removeOverviewModeObserver(mOverviewModeObserverArgumentCaptor.capture());
        verify(mThemeColorProvider)
                .removeThemeColorObserver(mThemeColorObserverArgumentCaptor.capture());
        verify(mThemeColorProvider).removeTintObserver(mTintObserverArgumentCaptor.capture());
        verify(mTabModelSelector).removeObserver(mTabModelSelectorObserverArgumentCaptor.capture());
        verify(mTabGroupModelFilter, never())
                .removeTabGroupObserver(mTabGroupModelFilterObserverArgumentCaptor.capture());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void switchTabModel_CTS() {
        initAndAssertProperties(mTab1);
        assertThat(mTabGroupUiMediator.getIsShowingOverViewModeForTesting(), equalTo(false));
        TabModel incognitoTabModel = prepareIncognitoTabModel();

        // Trigger tab strip with a selection.
        mTabModelObserverArgumentCaptor.getValue().didSelectTab(
                mTab2, TabSelectionType.FROM_USER, TAB1_ID);
        verifyResetStrip(true, mAllTabsList);

        mTabModelSelectorObserverArgumentCaptor.getValue().onTabModelSelected(
                mTabModel, incognitoTabModel);

        verify(mSnackbarManager).dismissSnackbars(eq(mTabGroupUiMediator));
        verifyResetStrip(true, mAllTabsList);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID)
    public void testUndoSnackbar_action() {
        initAndAssertProperties(mTab1);
        assertThat(ConditionalTabStripUtils.getFeatureStatus(), equalTo(FeatureStatus.DEFAULT));

        mTabGroupUiMediator.onAction(mock(Object.class));

        verifyResetStrip(true, mAllTabsList);
        assertThat(ConditionalTabStripUtils.getFeatureStatus(), equalTo(FeatureStatus.ACTIVATED));
    }

    /*********************** Class common tests *************************/

    @Test
    public void themeColorChange() {
        initAndAssertProperties(mTab1);
        mModel.set(TabGroupUiProperties.PRIMARY_COLOR, -1);

        mThemeColorObserverArgumentCaptor.getValue().onThemeColorChanged(1, false);

        assertThat(mModel.get(TabGroupUiProperties.PRIMARY_COLOR), equalTo(1));
    }

    @Test
    public void tintChange() {
        initAndAssertProperties(mTab1);
        mModel.set(TabGroupUiProperties.TINT, null);
        ColorStateList colorStateList = mock(ColorStateList.class);

        mTintObserverArgumentCaptor.getValue().onTintChanged(colorStateList, true);

        assertThat(mModel.get(TabGroupUiProperties.TINT), equalTo(colorStateList));
    }

    @Test
    public void testSetLeftButtonDrawable() {
        initAndAssertProperties(mTab3);
        int drawableId = 321;

        mModel.set(TabGroupUiProperties.LEFT_BUTTON_DRAWABLE_ID, 0);

        mTabGroupUiMediator.setupLeftButtonDrawable(drawableId);

        assertThat(mModel.get(TabGroupUiProperties.LEFT_BUTTON_DRAWABLE_ID), equalTo(drawableId));
    }

    @Test
    public void testSetLeftButtonOnClickListener() {
        initAndAssertProperties(mTab3);
        View.OnClickListener listener = v -> {};

        mModel.set(TabGroupUiProperties.LEFT_BUTTON_ON_CLICK_LISTENER, null);

        mTabGroupUiMediator.setupLeftButtonOnClickListener(listener);

        assertThat(
                mModel.get(TabGroupUiProperties.LEFT_BUTTON_ON_CLICK_LISTENER), equalTo(listener));
    }

    @Test
    public void testTabModelSelectorTabObserverDestroyWhenDetach() {
        InOrder tabObserverDestroyInOrder = inOrder(mTab1);
        initAndAssertProperties(mTab1);

        mTabObserverCaptor.getValue().onActivityAttachmentChanged(mTab1, null);

        tabObserverDestroyInOrder.verify(mTab1).removeObserver(mTabObserverCaptor.capture());

        mTabGroupUiMediator.destroy();

        tabObserverDestroyInOrder.verify(mTab1, never())
                .removeObserver(mTabObserverCaptor.capture());
    }
}

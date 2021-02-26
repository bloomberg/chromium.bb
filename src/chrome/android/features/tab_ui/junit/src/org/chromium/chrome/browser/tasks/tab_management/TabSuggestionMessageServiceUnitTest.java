// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionFeedback.TabSuggestionResponse.ACCEPTED;
import static org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionFeedback.TabSuggestionResponse.DISMISSED;
import static org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionFeedback.TabSuggestionResponse.NOT_CONSIDERED;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabContext;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestion;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionFeedback;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Unit tests for {@link TabSuggestionMessageService}.
 */
@SuppressWarnings({"ResultOfMethodCallIgnored", "ArraysAsListWithZeroOrOneArgument"})
@RunWith(LocalRobolectricTestRunner.class)
public class TabSuggestionMessageServiceUnitTest {
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final int POSITION3 = 2;
    private static final int TAB1_ROOT_ID = TAB1_ID;
    private static final int TAB2_ROOT_ID = TAB2_ID;
    private static final int TAB3_ROOT_ID = TAB2_ID;

    private static final int CLOSE_SUGGESTION_ACTION_BUTTON_RESOURCE_ID =
            org.chromium.chrome.tab_ui.R.string.tab_suggestion_close_tab_action_button;
    private static final int GROUP_SUGGESTION_ACTION_BUTTON_RESOURCE_ID =
            org.chromium.chrome.tab_ui.R.string.tab_selection_editor_group;

    private Tab mTab1;
    private Tab mTab2;
    private Tab mTab3;

    private TabSuggestionMessageService mMessageService;

    @Rule
    public JniMocker mocker = new JniMocker();
    @Mock
    public Profile.Natives mMockProfileNatives;

    @Mock
    Context mContext;
    @Mock
    TabModelSelector mTabModelSelector;
    @Mock
    TabModel mTabModel;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    TabGroupModelFilter mTabGroupModelFilter;
    @Mock
    TabSelectionEditorCoordinator.TabSelectionEditorController mTabSelectionEditorController;
    @Mock
    Callback<TabSuggestionFeedback> mTabSuggestionFeedbackCallback;
    @Mock
    MessageService.MessageObserver mMessageObserver;

    @Captor
    ArgumentCaptor<TabSuggestionFeedback> mTabSuggestionFeedbackCallbackArgumentCaptor;

    @Before
    public void setUp() {
        // After setUp there are three tabs in TabModel.
        MockitoAnnotations.initMocks(this);
        mocker.mock(ProfileJni.TEST_HOOKS, mMockProfileNatives);

        // Set up Tabs.
        mTab1 = TabUiUnitTestUtils.prepareTab(TAB1_ID, TAB1_ROOT_ID, "");
        mTab2 = TabUiUnitTestUtils.prepareTab(TAB2_ID, TAB2_ROOT_ID, "");
        mTab3 = TabUiUnitTestUtils.prepareTab(TAB3_ID, TAB3_ROOT_ID, "");

        // Set up TabModelSelector.
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(mTab1).when(mTabModelSelector).getTabById(TAB1_ID);
        doReturn(mTab2).when(mTabModelSelector).getTabById(TAB2_ID);
        doReturn(mTab3).when(mTabModelSelector).getTabById(TAB3_ID);

        // Set up TabModel.
        doReturn(3).when(mTabModel).getCount();
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab3).when(mTabModel).getTabAt(POSITION3);

        // Set up TabModelFilter.
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mTabModel).when(mTabGroupModelFilter).getTabModel();

        // Set up MessageService.MessageObserver
        doNothing().when(mMessageObserver).messageReady(anyInt(), any());
        doNothing().when(mMessageObserver).messageInvalidate(anyInt());

        mMessageService = new TabSuggestionMessageService(
                mContext, mTabModelSelector, mTabSelectionEditorController);
        mMessageService.addObserver(mMessageObserver);
    }

    // Tests for Close suggestions.
    @Test
    public void testReviewHandler_closeSuggestion() {
        TabSuggestion tabSuggestion = prepareTabSuggestion(
                Arrays.asList(mTab1, mTab2), TabSuggestion.TabSuggestionAction.CLOSE);
        String closeSuggestionActionButtonText = "close";
        int expectedEnablingThreshold =
                TabSuggestionMessageService.CLOSE_SUGGESTION_ACTION_ENABLING_THRESHOLD;
        doReturn(closeSuggestionActionButtonText)
                .when(mContext)
                .getString(eq(CLOSE_SUGGESTION_ACTION_BUTTON_RESOURCE_ID));

        mMessageService.review(tabSuggestion, mTabSuggestionFeedbackCallback);
        verify(mTabSelectionEditorController)
                .configureToolbar(eq(closeSuggestionActionButtonText), anyInt(), any(),
                        eq(expectedEnablingThreshold), any());
        verify(mTabSelectionEditorController).show(eq(Arrays.asList(mTab1, mTab2, mTab3)), eq(2));

        tabSuggestion = prepareTabSuggestion(
                Arrays.asList(mTab1, mTab3), TabSuggestion.TabSuggestionAction.CLOSE);
        mMessageService.review(tabSuggestion, mTabSuggestionFeedbackCallback);
        verify(mTabSelectionEditorController).show(eq(Arrays.asList(mTab1, mTab3, mTab2)), eq(2));
    }

    @Test
    public void testClosingSuggestionActionHandler() {
        List<Tab> suggestedTabs = Arrays.asList(mTab1, mTab2);
        List<Integer> suggestedTabIds = Arrays.asList(TAB1_ID, TAB2_ID);
        TabSuggestion tabSuggestion =
                prepareTabSuggestion(suggestedTabs, TabSuggestion.TabSuggestionAction.CLOSE);

        assertEquals(3, mTabModelSelector.getCurrentModel().getCount());

        TabSelectionEditorActionProvider actionProvider =
                mMessageService.getActionProvider(tabSuggestion, mTabSuggestionFeedbackCallback);
        actionProvider.processSelectedTabs(suggestedTabs, mTabModelSelector);

        verify(mTabModel).closeMultipleTabs(eq(suggestedTabs), eq(true));
        verify(mTabSuggestionFeedbackCallback)
                .onResult(mTabSuggestionFeedbackCallbackArgumentCaptor.capture());

        TabSuggestionFeedback capturedFeedback =
                mTabSuggestionFeedbackCallbackArgumentCaptor.getValue();
        assertEquals(tabSuggestion, capturedFeedback.tabSuggestion);
        assertEquals(ACCEPTED, capturedFeedback.tabSuggestionResponse);
        assertEquals(suggestedTabIds, capturedFeedback.selectedTabIds);
        assertEquals(3, capturedFeedback.totalTabCount);
    }

    @Test
    public void testClosingSuggestionNavigationHandler() {
        List<Tab> suggestedTabs = Arrays.asList(mTab1, mTab2);
        TabSuggestion tabSuggestion =
                prepareTabSuggestion(suggestedTabs, TabSuggestion.TabSuggestionAction.CLOSE);

        TabSelectionEditorCoordinator.TabSelectionEditorNavigationProvider navigationProvider =
                mMessageService.getNavigationProvider(
                        tabSuggestion, mTabSuggestionFeedbackCallback);
        navigationProvider.goBack();

        verify(mTabSuggestionFeedbackCallback)
                .onResult(mTabSuggestionFeedbackCallbackArgumentCaptor.capture());
        TabSuggestionFeedback capturedFeedback =
                mTabSuggestionFeedbackCallbackArgumentCaptor.getValue();
        assertEquals(tabSuggestion, capturedFeedback.tabSuggestion);
        assertEquals(DISMISSED, capturedFeedback.tabSuggestionResponse);
    }

    // Tests for grouping suggestion
    @Test
    public void testReviewHandler_groupSuggestion() {
        TabSuggestion tabSuggestion = prepareTabSuggestion(
                Arrays.asList(mTab1, mTab2), TabSuggestion.TabSuggestionAction.GROUP);
        String groupSuggestionActionButtonText = "group";
        int expectedEnablingThreshold =
                TabSuggestionMessageService.GROUP_SUGGESTION_ACTION_ENABLING_THRESHOLD;
        doReturn(groupSuggestionActionButtonText)
                .when(mContext)
                .getString(eq(GROUP_SUGGESTION_ACTION_BUTTON_RESOURCE_ID));

        mMessageService.review(tabSuggestion, mTabSuggestionFeedbackCallback);
        verify(mTabSelectionEditorController)
                .configureToolbar(eq(groupSuggestionActionButtonText), anyInt(), any(),
                        eq(expectedEnablingThreshold), any());
        verify(mTabSelectionEditorController).show(eq(Arrays.asList(mTab1, mTab2, mTab3)), eq(2));

        tabSuggestion = prepareTabSuggestion(
                Arrays.asList(mTab1, mTab3), TabSuggestion.TabSuggestionAction.GROUP);
        mMessageService.review(tabSuggestion, mTabSuggestionFeedbackCallback);
        verify(mTabSelectionEditorController).show(eq(Arrays.asList(mTab1, mTab3, mTab2)), eq(2));
    }

    @Test
    public void testGroupingSuggestionActionHandler() {
        List<Tab> suggestedTabs = Arrays.asList(mTab1, mTab2);
        List<Integer> suggestedTabIds = Arrays.asList(TAB1_ID, TAB2_ID);
        TabSuggestion tabSuggestion =
                prepareTabSuggestion(suggestedTabs, TabSuggestion.TabSuggestionAction.GROUP);

        assertEquals(3, mTabModelSelector.getCurrentModel().getCount());

        TabSelectionEditorActionProvider actionProvider =
                mMessageService.getActionProvider(tabSuggestion, mTabSuggestionFeedbackCallback);
        actionProvider.processSelectedTabs(suggestedTabs, mTabModelSelector);

        verify(mTabSuggestionFeedbackCallback)
                .onResult(mTabSuggestionFeedbackCallbackArgumentCaptor.capture());
        verify(mTabGroupModelFilter)
                .mergeListOfTabsToGroup(eq(suggestedTabs), any(), eq(false), eq(true));

        TabSuggestionFeedback capturedFeedback =
                mTabSuggestionFeedbackCallbackArgumentCaptor.getValue();
        assertEquals(tabSuggestion, capturedFeedback.tabSuggestion);
        assertEquals(ACCEPTED, capturedFeedback.tabSuggestionResponse);
        assertEquals(suggestedTabIds, capturedFeedback.selectedTabIds);
        assertEquals(3, capturedFeedback.totalTabCount);
    }

    @Test
    public void testDismiss() {
        List<Tab> suggestedTabs = Arrays.asList(mTab1, mTab2);
        TabSuggestion tabSuggestion =
                prepareTabSuggestion(suggestedTabs, TabSuggestion.TabSuggestionAction.CLOSE);

        mMessageService.dismiss(tabSuggestion, mTabSuggestionFeedbackCallback);

        verify(mTabSuggestionFeedbackCallback)
                .onResult(mTabSuggestionFeedbackCallbackArgumentCaptor.capture());
        TabSuggestionFeedback capturedFeedback =
                mTabSuggestionFeedbackCallbackArgumentCaptor.getValue();
        assertEquals(tabSuggestion, capturedFeedback.tabSuggestion);
        assertEquals(NOT_CONSIDERED, capturedFeedback.tabSuggestionResponse);
    }

    @Test
    public void testInvalidatedSuggestion() {
        mMessageService.onTabSuggestionInvalidated();
        verify(mMessageObserver).messageInvalidate(anyInt());
    }

    @Test
    public void testNewSuggestions() {
        InOrder inOrder = Mockito.inOrder(mMessageObserver);

        mMessageService.onNewSuggestion(Collections.EMPTY_LIST, mTabSuggestionFeedbackCallback);
        inOrder.verify(mMessageObserver, never()).messageReady(anyInt(), any());

        TabSuggestion tabSuggestion = prepareTabSuggestion(
                Arrays.asList(mTab1, mTab2), TabSuggestion.TabSuggestionAction.CLOSE);
        mMessageService.onNewSuggestion(
                Arrays.asList(tabSuggestion), mTabSuggestionFeedbackCallback);
        inOrder.verify(mMessageObserver).messageReady(anyInt(), any());

        TabSuggestion tabSuggestion2 = prepareTabSuggestion(
                Arrays.asList(mTab1, mTab2), TabSuggestion.TabSuggestionAction.CLOSE);
        TabSuggestion tabSuggestion3 = prepareTabSuggestion(
                Arrays.asList(mTab1, mTab2), TabSuggestion.TabSuggestionAction.GROUP);

        mMessageService.onNewSuggestion(
                Arrays.asList(tabSuggestion, tabSuggestion2, tabSuggestion3),
                mTabSuggestionFeedbackCallback);
        inOrder.verify(mMessageObserver, times(3))
                .messageReady(eq(MessageService.MessageType.TAB_SUGGESTION),
                        any(TabSuggestionMessageService.TabSuggestionMessageData.class));
    }

    private TabSuggestion prepareTabSuggestion(
            List<Tab> suggestedTab, @TabSuggestion.TabSuggestionAction int actionCode) {
        List<TabContext.TabInfo> suggestedTabInfo = new ArrayList<>();
        for (int i = 0; i < suggestedTab.size(); i++) {
            TabContext.TabInfo tabInfo = TabContext.TabInfo.createFromTab(suggestedTab.get(i));
            suggestedTabInfo.add(tabInfo);
        }

        return new TabSuggestion(suggestedTabInfo, actionCode, "");
    }
}

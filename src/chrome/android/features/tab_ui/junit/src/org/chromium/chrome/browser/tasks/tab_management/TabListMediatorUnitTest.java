// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertTrue;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.refEq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ChromeFeatureList.TAB_GROUPS_ANDROID;
import static org.chromium.chrome.browser.ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.OTHERS;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.support.v7.widget.GridLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.helper.ItemTouchHelper;
import android.view.View;

import androidx.annotation.IntDef;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.util.UrlUtilitiesJni;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link TabListMediator}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
public class TabListMediatorUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Rule
    public JniMocker mMocker = new JniMocker();

    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String NEW_TITLE = "New title";
    private static final String CUSTOMIZED_DIALOG_TITLE1 = "Cool Tabs";
    private static final String TAB_GROUP_TITLES_FILE_NAME = "tab_group_titles";
    private static final String TAB1_DOMAIN = "tab1.com";
    private static final String TAB2_DOMAIN = "tab2.com";
    private static final String TAB3_DOMAIN = "tab3.com";
    private static final String NEW_DOMAIN = "new.com";
    private static final String TAB1_URL = "https://" + TAB1_DOMAIN;
    private static final String TAB2_URL = "https://" + TAB2_DOMAIN;
    private static final String TAB3_URL = "https://" + TAB3_DOMAIN;
    private static final String NEW_URL = "https://" + NEW_DOMAIN;
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;

    @IntDef({TabListMediatorType.TAB_SWITCHER, TabListMediatorType.TAB_STRIP,
            TabListMediatorType.TAB_GRID_DIALOG})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabListMediatorType {
        int TAB_SWITCHER = 0;
        int TAB_STRIP = 1;
        int TAB_GRID_DIALOG = 2;
        int NUM_ENTRIES = 3;
    }

    @Mock
    TabContentManager mTabContentManager;
    @Mock
    TabModelSelectorImpl mTabModelSelector;
    @Mock
    TabModelFilterProvider mTabModelFilterProvider;
    @Mock
    TabModelFilter mTabModelFilter;
    @Mock
    TabModel mTabModel;
    @Mock
    TabListFaviconProvider mTabListFaviconProvider;
    @Mock
    RecyclerView mRecyclerView;
    @Mock
    RecyclerView.Adapter mAdapter;
    @Mock
    TabGroupModelFilter mTabGroupModelFilter;
    @Mock
    EmptyTabModelFilter mEmptyTabModelFilter;
    @Mock
    TabListMediator.TabGridDialogHandler mTabGridDialogHandler;
    @Mock
    TabListMediator.CreateGroupButtonProvider mCreateGroupButtonProvider;
    @Mock
    TabListMediator.GridCardOnClickListenerProvider mGridCardOnClickListenerProvider;
    @Mock
    Drawable mFaviconDrawable;
    @Mock
    Bitmap mFaviconBitmap;
    @Mock
    Activity mContext;
    @Mock
    TabListMediator.TabActionListener mOpenGroupActionListener;
    @Mock
    GridLayoutManager mGridLayoutManager;
    @Mock
    Profile mProfile;
    @Mock
    Tracker mTracker;
    @Mock
    TabListMediator.TitleProvider mTitleProvider;
    @Mock
    SharedPreferences mSharedPreferences;
    @Mock
    SharedPreferences.Editor mEditor;
    @Mock
    SharedPreferences.Editor mPutStringEditor;
    @Mock
    SharedPreferences.Editor mRemoveEditor;
    @Mock
    UrlUtilities.Natives mUrlUtilitiesJniMock;
    @Mock
    TemplateUrlService mTemplateUrlService;

    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor
    ArgumentCaptor<FaviconHelper.FaviconImageCallback> mFaviconCallbackCaptor;
    @Captor
    ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor
    ArgumentCaptor<Callback<Drawable>> mCallbackCaptor;
    @Captor
    ArgumentCaptor<TabGroupModelFilter.Observer> mTabGroupModelFilterObserverCaptor;
    @Captor
    ArgumentCaptor<ComponentCallbacks> mComponentCallbacksCaptor;
    @Captor
    ArgumentCaptor<TemplateUrlService.TemplateUrlServiceObserver> mTemplateUrlServiceObserver;

    private TabImpl mTab1;
    private TabImpl mTab2;
    private TabListMediator mMediator;
    private TabListModel mModel;
    private SimpleRecyclerViewAdapter.ViewHolder mViewHolder1;
    private SimpleRecyclerViewAdapter.ViewHolder mViewHolder2;
    private RecyclerView.ViewHolder mDummyViewHolder1;
    private RecyclerView.ViewHolder mDummyViewHolder2;
    private View mItemView1 = mock(View.class);
    private View mItemView2 = mock(View.class);
    private TabModelObserver mMediatorTabModelObserver;
    private TabGroupModelFilter.Observer mMediatorTabGroupModelFilterObserver;

    @Before
    public void setUp() {
        RecordUserAction.setDisabledForTests(true);
        RecordHistogram.setDisabledForTests(true);

        MockitoAnnotations.initMocks(this);
        mMocker.mock(UrlUtilitiesJni.TEST_HOOKS, mUrlUtilitiesJniMock);

        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(false);
        FeatureUtilities.setStartSurfaceEnabledForTesting(false);
        TabUiFeatureUtilities.setSearchTermChipEnabledForTesting(true);
        mTab1 = prepareTab(TAB1_ID, TAB1_TITLE, TAB1_URL);
        mTab2 = prepareTab(TAB2_ID, TAB2_TITLE, TAB2_URL);
        mViewHolder1 = prepareViewHolder(TAB1_ID, POSITION1);
        mViewHolder2 = prepareViewHolder(TAB2_ID, POSITION2);
        mDummyViewHolder1 = prepareDummyViewHolder(mItemView1, POSITION1);
        mDummyViewHolder2 = prepareDummyViewHolder(mItemView2, POSITION2);
        List<Tab> tabs1 = new ArrayList<>(Arrays.asList(mTab1));
        List<Tab> tabs2 = new ArrayList<>(Arrays.asList(mTab2));

        List<TabModel> tabModelList = new ArrayList<>();
        tabModelList.add(mTabModel);

        doNothing()
                .when(mTabContentManager)
                .getTabThumbnailWithCallback(any(), any(), anyBoolean(), anyBoolean());
        doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        doReturn(tabModelList).when(mTabModelSelector).getModels();

        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mTab1).when(mTabModelSelector).getCurrentTab();
        doReturn(TAB1_ID).when(mTabModelSelector).getCurrentTabId();
        doNothing()
                .when(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doNothing().when(mTab1).addObserver(mTabObserverCaptor.capture());
        doReturn(0).when(mTabModel).index();
        doReturn(2).when(mTabModel).getCount();
        doNothing()
                .when(mTabListFaviconProvider)
                .getFaviconForUrlAsync(anyString(), anyBoolean(), mCallbackCaptor.capture());
        doReturn(mFaviconDrawable)
                .when(mTabListFaviconProvider)
                .getFaviconForUrlSync(anyString(), anyBoolean(), any(Bitmap.class));
        doReturn(mTab1).when(mTabModelSelector).getTabById(TAB1_ID);
        doReturn(mTab2).when(mTabModelSelector).getTabById(TAB2_ID);
        doReturn(tabs1).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(tabs2).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION2).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(mOpenGroupActionListener)
                .when(mGridCardOnClickListenerProvider)
                .openTabGridDialog(any(Tab.class));
        doNothing().when(mContext).registerComponentCallbacks(mComponentCallbacksCaptor.capture());
        doReturn(mGridLayoutManager).when(mRecyclerView).getLayoutManager();
        doReturn(mSharedPreferences)
                .when(mContext)
                .getSharedPreferences(TAB_GROUP_TITLES_FILE_NAME, Context.MODE_PRIVATE);
        doReturn(mEditor).when(mSharedPreferences).edit();
        doReturn(mRemoveEditor).when(mEditor).remove(any(String.class));
        doReturn(mPutStringEditor).when(mEditor).putString(any(String.class), any(String.class));
        doReturn(TAB1_DOMAIN)
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(eq(TAB1_URL), anyBoolean());
        doReturn(TAB2_DOMAIN)
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(eq(TAB2_URL), anyBoolean());
        doReturn(TAB3_DOMAIN)
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(eq(TAB3_URL), anyBoolean());
        doNothing().when(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());

        mModel = new TabListModel();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        TabListMediator.SearchTermChipUtils.setIsSearchChipAdaptiveIconEnabledForTesting(false);
        mMediator = new TabListMediator(mContext, mModel, mTabModelSelector,
                mTabContentManager::getTabThumbnailWithCallback, mTitleProvider,
                mTabListFaviconProvider, false, null, null, mGridCardOnClickListenerProvider, null,
                getClass().getSimpleName(), UiType.CLOSABLE);
        mMediator.registerOrientationListener(mGridLayoutManager);
        TrackerFactory.setTrackerForTests(mTracker);
        ContextUtils.initApplicationContextForTests(mContext);
    }

    @After
    public void tearDown() {
        RecordUserAction.setDisabledForTests(false);
        RecordHistogram.setDisabledForTests(false);
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(null);
        FeatureUtilities.setStartSurfaceEnabledForTesting(null);
        TabUiFeatureUtilities.setSearchTermChipEnabledForTesting(null);
        TabAttributeCache.clearAllForTesting();
    }

    @Test
    public void initializesWithCurrentTabs() {
        initAndAssertAllProperties();
    }

    @Test
    public void updatesTitle_WithoutStoredTitle() {
        initAndAssertAllProperties();

        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        doReturn(NEW_TITLE).when(mTitleProvider).getTitle(mTab1);
        mTabObserverCaptor.getValue().onTitleUpdated(mTab1);

        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(NEW_TITLE));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void updatesTitle_WithStoredTitle_TabGroup() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);

        // Mock that tab1 and new tab are in the same group with root ID as TAB1_ID.
        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        when(mSharedPreferences.getString(String.valueOf(mTab1.getRootId()), null))
                .thenReturn(CUSTOMIZED_DIALOG_TITLE1);
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        mTabObserverCaptor.getValue().onTitleUpdated(mTab1);

        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE1));
    }

    @Test
    public void updatesFavicon_SingleTab_GTS() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);

        mModel.get(0).model.set(TabProperties.FAVICON, null);
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap);

        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON));
    }

    @Test
    public void updatesFavicon_SingleTab_NonGTS() {
        initAndAssertAllProperties();

        mModel.get(0).model.set(TabProperties.FAVICON, null);
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap);

        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON));
    }

    @Test
    public void updatesFavicon_TabGroup_GTS() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);

        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON));
        // Assert that tab1 is in a group.
        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(Arrays.asList(mTab1, newTab)).when(mTabModelFilter).getRelatedTabList(eq(TAB1_ID));

        mTabObserverCaptor.getValue().onFaviconUpdated(mTab1, mFaviconBitmap);

        assertNull(mModel.get(0).model.get(TabProperties.FAVICON));
    }

    @Test
    public void updateFavicon_StaleIndex() {
        initAndAssertAllProperties();
        mModel.get(0).model.set(TabProperties.FAVICON, null);
        mModel.get(1).model.set(TabProperties.FAVICON, null);

        mMediator.updateFaviconForTab(mTab2, null);
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(1));
        // Before executing callback, there is a deletion in tab list model which makes the index
        // stale.
        mModel.removeAt(0);
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(0));

        // Start to execute callback.
        mCallbackCaptor.getValue().onResult(mFaviconDrawable);

        assertThat(mModel.get(0).model.get(TabProperties.FAVICON), equalTo(mFaviconDrawable));
    }

    @Test
    public void sendsSelectSignalCorrectly() {
        initAndAssertAllProperties();

        mModel.get(1)
                .model.get(TabProperties.TAB_SELECTED_LISTENER)
                .run(mModel.get(1).model.get(TabProperties.TAB_ID));

        verify(mGridCardOnClickListenerProvider)
                .onTabSelecting(mModel.get(1).model.get(TabProperties.TAB_ID));
    }

    @Test
    public void sendsCloseSignalCorrectly() {
        initAndAssertAllProperties();

        mModel.get(1)
                .model.get(TabProperties.TAB_CLOSED_LISTENER)
                .run(mModel.get(1).model.get(TabProperties.TAB_ID));

        verify(mTabModel).closeTab(eq(mTab2), eq(null), eq(false), eq(false), eq(true));
    }

    @Test
    public void sendsMoveTabSignalCorrectlyWithoutGroup() {
        initAndAssertAllProperties();
        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        doReturn(mEmptyTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        itemTouchHelperCallback.onMove(mRecyclerView, mViewHolder1, mViewHolder2);

        verify(mTabModel).moveTab(eq(TAB1_ID), eq(2));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    public void sendsMoveTabSignalCorrectlyWithGroup() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);

        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(true);

        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        itemTouchHelperCallback.onMove(mRecyclerView, mViewHolder1, mViewHolder2);

        verify(mTabGroupModelFilter).moveRelatedTabs(eq(TAB1_ID), eq(2));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    public void sendsMoveTabSignalCorrectlyWithinGroup() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_GRID_DIALOG);

        getItemTouchHelperCallback().onMove(mRecyclerView, mViewHolder1, mViewHolder2);

        verify(mTabModel).moveTab(eq(TAB1_ID), eq(2));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    public void sendsMergeTabSignalCorrectly() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);

        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(true);
        itemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION1);
        itemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION2);
        itemTouchHelperCallback.getMovementFlags(mRecyclerView, mDummyViewHolder1);

        doReturn(mAdapter).when(mRecyclerView).getAdapter();

        // Simulate the drop action.
        itemTouchHelperCallback.onSelectedChanged(
                mDummyViewHolder2, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).mergeTabsToGroup(eq(TAB2_ID), eq(TAB1_ID));
        verify(mGridLayoutManager).removeView(mItemView2);
        verify(mTracker).notifyEvent(eq(EventConstants.TAB_DRAG_AND_DROP_TO_GROUP));
    }

    @Test
    @Features.DisableFeatures({TAB_GROUPS_ANDROID, TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    public void neverSendsMergeTabSignal_Without_Group() {
        initAndAssertAllProperties();

        mMediator.setActionOnAllRelatedTabsForTesting(true);
        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(true);
        itemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION1);
        itemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION2);
        itemTouchHelperCallback.getMovementFlags(mRecyclerView, mDummyViewHolder1);

        doReturn(mAdapter).when(mRecyclerView).getAdapter();

        // Simulate the drop action.
        itemTouchHelperCallback.onSelectedChanged(
                mDummyViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mGridLayoutManager, never()).removeView(any(View.class));
    }

    @Test
    @Features.EnableFeatures({TAB_GROUPS_ANDROID})
    @Features.DisableFeatures({TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    public void neverSendsMergeTabSignal_With_Group_Without_Group_Improvement() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);

        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(true);
        itemTouchHelperCallback.setHoveredTabIndexForTesting(POSITION1);
        itemTouchHelperCallback.setSelectedTabIndexForTesting(POSITION2);
        itemTouchHelperCallback.getMovementFlags(mRecyclerView, mDummyViewHolder1);

        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mAdapter).when(mRecyclerView).getAdapter();

        // Simulate the drop action.
        itemTouchHelperCallback.onSelectedChanged(
                mDummyViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter, never()).mergeTabsToGroup(anyInt(), anyInt());
        verify(mGridLayoutManager, never()).removeView(any(View.class));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void sendsUngroupSignalCorrectly() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_GRID_DIALOG);

        TabGridItemTouchHelperCallback itemTouchHelperCallback = getItemTouchHelperCallback();
        itemTouchHelperCallback.setActionsOnAllRelatedTabsForTesting(false);
        itemTouchHelperCallback.setUnGroupTabIndexForTesting(POSITION1);
        itemTouchHelperCallback.getMovementFlags(mRecyclerView, mDummyViewHolder1);

        doReturn(mAdapter).when(mRecyclerView).getAdapter();
        doReturn(1).when(mAdapter).getItemCount();

        // Simulate the ungroup action.
        itemTouchHelperCallback.onSelectedChanged(
                mDummyViewHolder1, ItemTouchHelper.ACTION_STATE_IDLE);

        verify(mTabGroupModelFilter).moveTabOutOfGroup(eq(TAB1_ID));
        verify(mGridLayoutManager).removeView(mItemView1);
    }

    @Test
    public void tabClosure() {
        initAndAssertAllProperties();

        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
    }

    @Test
    public void tabClosure_IgnoresUpdatesForTabsOutsideOfModel() {
        initAndAssertAllProperties();

        mTabModelObserverCaptor.getValue().willCloseTab(
                prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL), false);

        assertThat(mModel.size(), equalTo(2));
    }

    @Test
    public void tabAddition_RestoreNotComplete() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);

        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(1);
        doReturn(newTab).when(mTabModelFilter).getTabAt(2);
        doReturn(3).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(newTab)).when(mTabModelFilter).getRelatedTabList(eq(TAB3_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_RESTORE);

        // When tab restoring stage is not yet finished, this tab info should not be added to
        // property model.
        assertThat(mModel.size(), equalTo(2));
    }

    @Test
    public void tabAddition_Restore() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        // Mock that tab restoring stage is over.
        mMediator.setTabRestoreCompletedForTesting(true);
        TabListMediator.TabActionListener actionListenerBeforeUpdate =
                mModel.get(1).model.get(TabProperties.TAB_SELECTED_LISTENER);

        // Mock that newTab was in the same group with tab, and now it is restored.
        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = Arrays.asList(mTab2, newTab);
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(1);
        doReturn(2).when(mTabModelFilter).getCount();
        doReturn(1).when(mTabModelFilter).indexOf(newTab);
        doReturn(tabs).when(mTabModelFilter).getRelatedTabList(eq(TAB3_ID));
        doReturn(tabs).when(mTabModelFilter).getRelatedTabList(eq(TAB2_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_RESTORE);

        TabListMediator.TabActionListener actionListenerAfterUpdate =
                mModel.get(1).model.get(TabProperties.TAB_SELECTED_LISTENER);
        // The selection listener should be updated which indicates that corresponding property
        // model is updated.
        assertThat(actionListenerBeforeUpdate, not(actionListenerAfterUpdate));
        assertThat(mModel.size(), equalTo(2));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void tabAddition_Restore_SyncingTabListModelWithTabModel() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);
        // Mock that tab restoring stage is over.
        mMediator.setTabRestoreCompletedForTesting(true);

        // Mock that tab1 and tab2 are in the same group, and they are being restored. The
        // TabListModel has been cleaned out before the restoring happens. This case could happen
        // within a incognito tab group when user switches between light/dark mode.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab2);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(1).when(mTabGroupModelFilter).getCount();
        mModel.clear();

        mMediatorTabModelObserver.didAddTab(mTab2, TabLaunchType.FROM_RESTORE);
        assertThat(mModel.size(), equalTo(0));

        mMediatorTabModelObserver.didAddTab(mTab1, TabLaunchType.FROM_RESTORE);
        assertThat(mModel.size(), equalTo(1));
    }

    @Test
    public void tabAddition_GTS() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        mMediator.setTabRestoreCompletedForTesting(true);

        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(1);
        doReturn(newTab).when(mTabModelFilter).getTabAt(2);
        doReturn(3).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(newTab))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB3_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(2).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAddition_GTS_Skip() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        mMediator.setTabRestoreCompletedForTesting(true);

        // Add a new tab to the group with mTab2.
        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(1);
        doReturn(2).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(mTab2, newTab))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB3_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);

        assertThat(mModel.size(), equalTo(2));
    }

    @Test
    public void tabAddition_GTS_Middle() {
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);
        mMediator.setTabRestoreCompletedForTesting(true);

        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(mTab1).when(mTabModelFilter).getTabAt(0);
        doReturn(newTab).when(mTabModelFilter).getTabAt(1);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(2);
        doReturn(3).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(newTab))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB3_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAddition_Dialog_End() {
        initAndAssertAllProperties();
        mMediator.setTabRestoreCompletedForTesting(true);

        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(3).when(mTabModel).getCount();
        doReturn(Arrays.asList(mTab1, mTab2, newTab))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB1_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(2).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAddition_Dialog_Middle() {
        initAndAssertAllProperties();
        mMediator.setTabRestoreCompletedForTesting(true);

        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(3).when(mTabModel).getCount();
        doReturn(Arrays.asList(mTab1, newTab, mTab2))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB1_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    public void tabAddition_Dialog_Skip() {
        initAndAssertAllProperties();
        mMediator.setTabRestoreCompletedForTesting(true);

        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        // newTab is of another group.
        doReturn(Arrays.asList(mTab1, mTab2)).when(mTabModelFilter).getRelatedTabList(eq(TAB1_ID));
        assertThat(mModel.size(), equalTo(2));

        mTabModelObserverCaptor.getValue().didAddTab(newTab, TabLaunchType.FROM_CHROME_UI);

        assertThat(mModel.size(), equalTo(2));
    }

    @Test
    public void tabSelection() {
        initAndAssertAllProperties();

        mTabModelObserverCaptor.getValue().didSelectTab(
                mTab2, TabLaunchType.FROM_CHROME_UI, TAB1_ID);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
    }

    @Test
    public void tabClosureUndone() {
        initAndAssertAllProperties();

        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        doReturn(3).when(mTabModel).getCount();
        doReturn(Arrays.asList(mTab1, mTab2, newTab))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB1_ID));

        mTabModelObserverCaptor.getValue().tabClosureUndone(newTab);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.get(2).model.get(TabProperties.TAB_ID), equalTo(TAB3_ID));
        assertThat(mModel.get(2).model.get(TabProperties.TITLE), equalTo(TAB3_TITLE));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void tabMergeIntoGroup() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);

        // Assume that moveTab in TabModel is finished. Selected tab in the group becomes mTab1.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);

        // Assume that reset in TabGroupModelFilter is finished.
        doReturn(new ArrayList<>(Arrays.asList(mTab1, mTab2)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB1_ID);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        assertThat(mModel.indexFromId(TAB1_ID), equalTo(POSITION1));
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(POSITION2));
        assertNotNull(mModel.get(0).model.get(TabProperties.FAVICON));
        assertNotNull(mModel.get(1).model.get(TabProperties.FAVICON));

        mMediatorTabGroupModelFilterObserver.didMergeTabToGroup(mTab1, TAB2_ID);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertNull(mModel.get(0).model.get(TabProperties.FAVICON));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void tabMoveOutOfGroup_GTS_Moved_Tab_Selected() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);

        // Assume that two tabs are in the same group before ungroup.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab2));
        mMediator.resetWithListOfTabs(tabs, false, false);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        // Assume that TabGroupModelFilter is already updated.
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(2).when(mTabGroupModelFilter).getCount();

        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab1, POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(false));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(true));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void tabMoveOutOfGroup_GTS_Origin_Tab_Selected() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);

        // Assume that two tabs are in the same group before ungroup.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(tabs, false, false);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        // Assume that TabGroupModelFilter is already updated.
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(2).when(mTabGroupModelFilter).getCount();

        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab2, POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void tabMoveOutOfGroup_GTS_LastTab() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);

        // Assume that tab1 is a single tab.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(tabs, false, false);
        doReturn(1).when(mTabGroupModelFilter).getCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);

        // Ungroup the single tab.
        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab1, POSITION1);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void tabMoveOutOfGroup_Dialog() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_GRID_DIALOG);

        // Assume that filter is already updated.
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab1, POSITION1);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        verify(mTabGridDialogHandler).updateDialogContent(TAB2_ID);
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void tabMoveOutOfGroup_Dialog_LastTab() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_GRID_DIALOG);

        // Assume that tab1 is a single tab.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        mMediator.resetWithListOfTabs(tabs, false, false);
        doReturn(1).when(mTabGroupModelFilter).getCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(tabs).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);

        // Ungroup the single tab.
        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab1, POSITION1);

        verify(mTabGridDialogHandler).updateDialogContent(Tab.INVALID_TAB_ID);
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void tabMoveOutOfGroup_Strip() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_STRIP);

        // Assume that filter is already updated.
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab1, POSITION1);

        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
        verify(mTabGridDialogHandler, never()).updateDialogContent(anyInt());
    }

    @Test
    public void tabMovementWithoutGroup_Forward() {
        initAndAssertAllProperties();

        doReturn(mEmptyTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabModelObserverCaptor.getValue().didMoveTab(mTab2, POSITION1, POSITION2);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    public void tabMovementWithoutGroup_Backward() {
        initAndAssertAllProperties();

        doReturn(mEmptyTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mTabModelObserverCaptor.getValue().didMoveTab(mTab1, POSITION2, POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void tabMovementWithGroup_Forward() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);

        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mMediatorTabGroupModelFilterObserver.didMoveTabGroup(mTab2, POSITION2, POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void tabMovementWithGroup_Backward() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);

        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mMediatorTabGroupModelFilterObserver.didMoveTabGroup(mTab1, POSITION1, POSITION2);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void tabMovementWithinGroup_Forward() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_GRID_DIALOG);

        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(TAB1_ID).when(mTab1).getRootId();
        doReturn(TAB1_ID).when(mTab2).getRootId();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mMediatorTabGroupModelFilterObserver.didMoveWithinGroup(mTab2, POSITION2, POSITION1);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void tabMovementWithinGroup_Backward() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_GRID_DIALOG);

        // Assume that moveTab in TabModel is finished.
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(TAB1_ID).when(mTab1).getRootId();
        doReturn(TAB1_ID).when(mTab2).getRootId();

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        mMediatorTabGroupModelFilterObserver.didMoveWithinGroup(mTab1, POSITION1, POSITION2);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void undoGrouped_One_Adjacent_Tab() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);

        // Assume there are 3 tabs in TabModel, mTab2 just grouped with mTab1;
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, tab3));
        mMediator.resetWithListOfTabs(tabs, false, false);
        assertThat(mModel.size(), equalTo(2));

        // Assume undo grouping mTab2 with mTab1.
        doReturn(3).when(mTabGroupModelFilter).getCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(tab3).when(mTabGroupModelFilter).getTabAt(2);

        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab2, POSITION1);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.indexFromId(TAB1_ID), equalTo(0));
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(1));
        assertThat(mModel.indexFromId(TAB3_ID), equalTo(2));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void undoForwardGrouped_One_Tab() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);

        // Assume there are 3 tabs in TabModel, tab3 just grouped with mTab1;
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        mMediator.resetWithListOfTabs(tabs, false, false);
        assertThat(mModel.size(), equalTo(2));

        // Assume undo grouping tab3 with mTab1.
        doReturn(3).when(mTabGroupModelFilter).getCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(tab3).when(mTabGroupModelFilter).getTabAt(2);

        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(tab3, POSITION1);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.indexFromId(TAB1_ID), equalTo(0));
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(1));
        assertThat(mModel.indexFromId(TAB3_ID), equalTo(2));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void undoBackwardGrouped_One_Tab() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);

        // Assume there are 3 tabs in TabModel, mTab1 just grouped with mTab2;
        Tab tab3 = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab2, tab3));
        mMediator.resetWithListOfTabs(tabs, false, false);
        assertThat(mModel.size(), equalTo(2));

        // Assume undo grouping tab3 with mTab1.
        doReturn(3).when(mTabGroupModelFilter).getCount();
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(tab3).when(mTabGroupModelFilter).getTabAt(2);

        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab1, POSITION2);

        assertThat(mModel.size(), equalTo(3));
        assertThat(mModel.indexFromId(TAB1_ID), equalTo(0));
        assertThat(mModel.indexFromId(TAB2_ID), equalTo(1));
        assertThat(mModel.indexFromId(TAB3_ID), equalTo(2));
    }

    @Test
    public void updateSpanCount_Portrait_SingleWindow() {
        initAndAssertAllProperties();
        // Mock that we are switching to portrait mode.
        Configuration configuration = new Configuration();
        configuration.orientation = Configuration.ORIENTATION_PORTRAIT;
        // Mock that we are in single window mode.
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(false);

        mComponentCallbacksCaptor.getValue().onConfigurationChanged(configuration);

        verify(mGridLayoutManager).setSpanCount(TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_PORTRAIT);
    }

    @Test
    public void updateSpanCount_Landscape_SingleWindow() {
        initAndAssertAllProperties();
        // Mock that we are switching to landscape mode.
        Configuration configuration = new Configuration();
        configuration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        // Mock that we are in single window mode.
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(false);

        mComponentCallbacksCaptor.getValue().onConfigurationChanged(configuration);

        verify(mGridLayoutManager)
                .setSpanCount(TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_LANDSCAPE);
    }

    @Test
    public void updateSpanCount_MultiWindow() {
        initAndAssertAllProperties();
        Configuration portraitConfiguration = new Configuration();
        portraitConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        Configuration landscapeConfiguration = new Configuration();
        landscapeConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        // Mock that we are in multi window mode.
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);

        mComponentCallbacksCaptor.getValue().onConfigurationChanged(landscapeConfiguration);
        mComponentCallbacksCaptor.getValue().onConfigurationChanged(portraitConfiguration);
        mComponentCallbacksCaptor.getValue().onConfigurationChanged(landscapeConfiguration);

        // The span count is fixed to 2 for multi window mode regardless of the orientation change.
        verify(mGridLayoutManager, times(3))
                .setSpanCount(TabListCoordinator.GRID_LAYOUT_SPAN_COUNT_PORTRAIT);
    }

    @Test
    public void resetWithListOfTabs_MruOrder() {
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }
        assertThat(tabs.size(), equalTo(2));

        long timestamp1 = 1;
        long timestamp2 = 2;
        doReturn(timestamp1).when(mTab1).getTimestampMillis();
        doReturn(timestamp2).when(mTab2).getTimestampMillis();
        mMediator.resetWithListOfTabs(tabs, /*quickMode =*/false, /*mruMode =*/true);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mMediator.indexOfTab(TAB1_ID), equalTo(1));
        assertThat(mMediator.indexOfTab(TAB2_ID), equalTo(0));

        doReturn(timestamp2).when(mTab1).getTimestampMillis();
        doReturn(timestamp1).when(mTab2).getTimestampMillis();
        mMediator.resetWithListOfTabs(tabs, /*quickMode =*/false, /*mruMode =*/true);

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));
        assertThat(mMediator.indexOfTab(TAB1_ID), equalTo(0));
        assertThat(mMediator.indexOfTab(TAB2_ID), equalTo(1));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void getLatestTitle_NotGTS() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_GRID_DIALOG);
        // Mock that we have a stored title stored with reference to root ID of tab1.
        when(mSharedPreferences.getString(String.valueOf(mTab1.getRootId()), null))
                .thenReturn(CUSTOMIZED_DIALOG_TITLE1);
        assertThat(mMediator.getTabGroupTitleEditor().getTabGroupTitle(mTab1.getRootId()),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID);

        // Even if we have a stored title, we only show it in tab switcher.
        assertThat(mMediator.getLatestTitleForTab(mTab1), equalTo(TAB1_TITLE));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void getLatestTitle_SingleTab_GTS() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);
        // Mock that we have a stored title stored with reference to root ID of tab1.
        when(mSharedPreferences.getString(String.valueOf(mTab1.getRootId()), null))
                .thenReturn(CUSTOMIZED_DIALOG_TITLE1);
        assertThat(mMediator.getTabGroupTitleEditor().getTabGroupTitle(mTab1.getRootId()),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));

        // Mock that tab1 is a single tab.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1));
        createTabGroup(tabs, TAB1_ID);

        // We never show stored title for single tab.
        assertThat(mMediator.getLatestTitleForTab(mTab1), equalTo(TAB1_TITLE));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void getLatestTitle_Stored_GTS() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);
        // Mock that we have a stored title stored with reference to root ID of tab1.
        when(mSharedPreferences.getString(String.valueOf(mTab1.getRootId()), null))
                .thenReturn(CUSTOMIZED_DIALOG_TITLE1);
        assertThat(mMediator.getTabGroupTitleEditor().getTabGroupTitle(mTab1.getRootId()),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));

        // Mock that tab1 and tab2 are in the same group and group root id is TAB1_ID.
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID);

        assertThat(mMediator.getLatestTitleForTab(mTab1), equalTo(CUSTOMIZED_DIALOG_TITLE1));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void updateTabGroupTitle_GTS() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);
        assertThat(mModel.get(POSITION1).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));

        // Mock that tab1 and newTab are in the same group and group root id is TAB1_ID.
        TabImpl newTab = prepareTab(TAB3_ID, TAB3_TITLE, TAB3_URL);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, newTab));
        createTabGroup(tabs, TAB1_ID);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);

        mMediator.getTabGroupTitleEditor().updateTabGroupTitle(mTab1, CUSTOMIZED_DIALOG_TITLE1);

        assertThat(mModel.get(POSITION1).model.get(TabProperties.TITLE),
                equalTo(CUSTOMIZED_DIALOG_TITLE1));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void tabGroupTitleEditor_storeTitle() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);
        TabGroupTitleEditor tabGroupTitleEditor = mMediator.getTabGroupTitleEditor();

        tabGroupTitleEditor.storeTabGroupTitle(mTab1.getRootId(), CUSTOMIZED_DIALOG_TITLE1);

        verify(mEditor).putString(
                eq(String.valueOf(mTab1.getRootId())), eq(CUSTOMIZED_DIALOG_TITLE1));
        verify(mPutStringEditor).apply();
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    // clang-format off
    public void tabGroupTitleEditor_deleteTitle() {
        // clang-format on
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);
        TabGroupTitleEditor tabGroupTitleEditor = mMediator.getTabGroupTitleEditor();

        tabGroupTitleEditor.deleteTabGroupTitle(mTab1.getRootId());

        verify(mEditor).remove(eq(String.valueOf(mTab1.getRootId())));
        verify(mRemoveEditor).apply();
    }

    @Test
    public void addSpecialItem() {
        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        mMediator.addSpecialItemToModel(0, TabProperties.UiType.DIVIDER, model);

        assertTrue(mModel.size() > 0);
        assertEquals(TabProperties.UiType.DIVIDER, mModel.get(0).type);
    }

    @Test
    public void addSpecialItem_notPersistOnReset() {
        PropertyModel model = mock(PropertyModel.class);
        when(model.get(CARD_TYPE)).thenReturn(MESSAGE);
        mMediator.addSpecialItemToModel(0, TabProperties.UiType.DIVIDER, model);
        assertEquals(TabProperties.UiType.DIVIDER, mModel.get(0).type);

        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        mMediator.resetWithListOfTabs(tabs, false, false);
        assertThat(mModel.size(), equalTo(2));
        assertNotEquals(TabProperties.UiType.DIVIDER, mModel.get(0).type);
        assertNotEquals(TabProperties.UiType.DIVIDER, mModel.get(1).type);

        mMediator.addSpecialItemToModel(1, TabProperties.UiType.DIVIDER, model);
        assertThat(mModel.size(), equalTo(3));
        assertEquals(TabProperties.UiType.DIVIDER, mModel.get(1).type);
    }

    @Test(expected = AssertionError.class)
    public void addSpecialItem_withoutTabListModelProperties() {
        mMediator.addSpecialItemToModel(0, TabProperties.UiType.DIVIDER, new PropertyModel());
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testUrlUpdated_forSingleTab_GTS_GroupNotEnabled() {
        initAndAssertAllProperties();
        assertNotEquals(NEW_URL, mModel.get(POSITION1).model.get(TabProperties.URL));

        doReturn(NEW_URL).when(mTab1).getUrl();
        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);

        assertEquals(NEW_URL, mModel.get(POSITION1).model.get(TabProperties.URL));
        assertEquals(TAB2_URL, mModel.get(POSITION2).model.get(TabProperties.URL));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void
    testUrlUpdated_forSingleTab_GTS() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);
        assertNotEquals(NEW_URL, mModel.get(POSITION1).model.get(TabProperties.URL));

        doReturn(NEW_URL).when(mTab1).getUrl();
        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);

        assertEquals(NEW_URL, mModel.get(POSITION1).model.get(TabProperties.URL));
        assertEquals(TAB2_URL, mModel.get(POSITION2).model.get(TabProperties.URL));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void
    testUrlUpdated_forGroup_GTS() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab2);

        mMediatorTabGroupModelFilterObserver.didMergeTabToGroup(mTab2, TAB1_ID);
        assertEquals(TAB1_DOMAIN + ", " + TAB2_DOMAIN,
                mModel.get(POSITION1).model.get(TabProperties.URL));

        doReturn(NEW_DOMAIN)
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(eq(NEW_URL), anyBoolean());

        // Update URL for mTab1.
        doReturn(NEW_URL).when(mTab1).getUrl();
        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);

        assertEquals(NEW_DOMAIN + ", " + TAB2_DOMAIN,
                mModel.get(POSITION1).model.get(TabProperties.URL));

        // Update URL for mTab2.
        doReturn(NEW_URL).when(mTab2).getUrl();
        mTabObserverCaptor.getValue().onUrlUpdated(mTab2);

        assertEquals(
                NEW_DOMAIN + ", " + NEW_DOMAIN, mModel.get(POSITION1).model.get(TabProperties.URL));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void
    testUrlUpdated_forGroup_Dialog() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_GRID_DIALOG);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab2);

        mMediatorTabGroupModelFilterObserver.didMergeTabToGroup(mTab2, TAB1_ID);
        assertEquals(TAB1_URL, mModel.get(POSITION1).model.get(TabProperties.URL));
        assertEquals(TAB2_URL, mModel.get(POSITION2).model.get(TabProperties.URL));

        doReturn(NEW_DOMAIN)
                .when(mUrlUtilitiesJniMock)
                .getDomainAndRegistry(eq(NEW_URL), anyBoolean());

        // Update URL for mTab1.
        doReturn(NEW_URL).when(mTab1).getUrl();
        mTabObserverCaptor.getValue().onUrlUpdated(mTab1);

        assertEquals(NEW_URL, mModel.get(POSITION1).model.get(TabProperties.URL));
        assertEquals(TAB2_URL, mModel.get(POSITION2).model.get(TabProperties.URL));

        // Update URL for mTab2.
        doReturn(NEW_URL).when(mTab2).getUrl();
        mTabObserverCaptor.getValue().onUrlUpdated(mTab2);

        assertEquals(NEW_URL, mModel.get(POSITION1).model.get(TabProperties.URL));
        assertEquals(NEW_URL, mModel.get(POSITION2).model.get(TabProperties.URL));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void
    testUrlUpdated_forUnGroup() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);
        List<Tab> tabs = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabs, TAB1_ID);

        mMediatorTabGroupModelFilterObserver.didMergeTabToGroup(mTab2, TAB1_ID);
        assertEquals(TAB1_DOMAIN + ", " + TAB2_DOMAIN,
                mModel.get(POSITION1).model.get(TabProperties.URL));

        // Assume that TabGroupModelFilter is already updated.
        when(mTabGroupModelFilter.getRelatedTabList(TAB1_ID)).thenReturn(Arrays.asList(mTab1));
        when(mTabGroupModelFilter.getRelatedTabList(TAB2_ID)).thenReturn(Arrays.asList(mTab2));
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(2).when(mTabGroupModelFilter).getCount();

        mMediatorTabGroupModelFilterObserver.didMoveTabOutOfGroup(mTab2, POSITION1);
        assertEquals(TAB1_URL, mModel.get(POSITION1).model.get(TabProperties.URL));
        assertEquals(TAB2_URL, mModel.get(POSITION2).model.get(TabProperties.URL));
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testTabObserverRemovedFromClosedTab() {
        // clang-format on
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);

        assertThat(mModel.size(), equalTo(2));
        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false);
        verify(mTab2).removeObserver(mTabObserverCaptor.getValue());
        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
    }

    @Test
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testTabObserverReattachToUndoClosedTab() {
        // clang-format on
        initAndAssertAllProperties();
        mMediator.setActionOnAllRelatedTabsForTesting(true);

        assertThat(mModel.size(), equalTo(2));
        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, false);
        assertThat(mModel.size(), equalTo(1));

        // Assume that TabModelFilter is already updated to reflect closed tab is undone.
        doReturn(2).when(mTabModelFilter).getCount();
        doReturn(mTab1).when(mTabModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModelFilter).getTabAt(POSITION2);
        when(mTabModelFilter.getRelatedTabList(TAB1_ID)).thenReturn(Arrays.asList(mTab1));
        when(mTabModelFilter.getRelatedTabList(TAB2_ID)).thenReturn(Arrays.asList(mTab2));

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab2);
        assertThat(mModel.size(), equalTo(2));
        // First time is when mTab2 initially added to mModel; second time is when mTab2 added back
        // to mModel because of undo action.
        verify(mTab2, times(2)).addObserver(mTabObserverCaptor.getValue());
    }

    @Test
    public void testUnchangeCheckIgnoreNonTabs() {
        initAndAssertAllProperties();
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }

        boolean showQuickly = mMediator.resetWithListOfTabs(tabs, false, false);
        assertThat(showQuickly, equalTo(true));

        // Create a PropertyModel that is not a tab and add it to the existing TabListModel.
        PropertyModel propertyModel = new PropertyModel.Builder(NewTabTileViewProperties.ALL_KEYS)
                                              .with(CARD_TYPE, OTHERS)
                                              .build();
        mMediator.addSpecialItemToModel(
                mModel.size(), TabProperties.UiType.NEW_TAB_TILE, propertyModel);
        assertThat(mModel.size(), equalTo(tabs.size() + 1));

        // TabListModel unchange check should ignore the non-Tab item.
        showQuickly = mMediator.resetWithListOfTabs(tabs, false, false);
        assertThat(showQuickly, equalTo(true));
    }

    @Test
    public void testSearchTermProperty() {
        initAndAssertAllProperties();
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }
        // The fast path to trigger updateTab().
        boolean showQuickly = mMediator.resetWithListOfTabs(tabs, false, false);
        assertThat(showQuickly, equalTo(true));

        assertThat(mModel.size(), equalTo(2));
        assertThat(mModel.get(0).model.get(TabProperties.SEARCH_QUERY), equalTo(null));
        assertThat(mModel.get(1).model.get(TabProperties.SEARCH_QUERY), equalTo(null));

        String searchTerm1 = "hello world";
        String searchTerm2 = "y'all";
        TabAttributeCache.setLastSearchTermForTesting(TAB1_ID, searchTerm1);
        TabAttributeCache.setLastSearchTermForTesting(TAB2_ID, searchTerm2);
        showQuickly = mMediator.resetWithListOfTabs(tabs, false, false);
        assertThat(showQuickly, equalTo(true));
        assertThat(mModel.get(0).model.get(TabProperties.SEARCH_QUERY), equalTo(searchTerm1));
        assertThat(mModel.get(1).model.get(TabProperties.SEARCH_QUERY), equalTo(searchTerm2));

        TabAttributeCache.setLastSearchTermForTesting(TAB1_ID, null);
        TabAttributeCache.setLastSearchTermForTesting(TAB2_ID, null);
        showQuickly = mMediator.resetWithListOfTabs(tabs, false, false);
        assertThat(showQuickly, equalTo(true));
        assertThat(mModel.get(0).model.get(TabProperties.SEARCH_QUERY), equalTo(null));
        assertThat(mModel.get(1).model.get(TabProperties.SEARCH_QUERY), equalTo(null));

        // The slow path to trigger addTabInfoToModel().
        tabs = new ArrayList<>(Arrays.asList(mTab1));
        TabAttributeCache.setLastSearchTermForTesting(TAB1_ID, searchTerm1);
        showQuickly = mMediator.resetWithListOfTabs(tabs, false, false);
        assertThat(showQuickly, equalTo(false));
        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.SEARCH_QUERY), equalTo(searchTerm1));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    public void
    testSearchTermProperty_TabGroups_TabSwitcher() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_SWITCHER);
        String searchTerm1 = "hello world";
        TabAttributeCache.setLastSearchTermForTesting(TAB1_ID, searchTerm1);

        mMediator.resetWithListOfTabs(new ArrayList<>(Arrays.asList(mTab1)), false, false);
        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.SEARCH_QUERY), equalTo(searchTerm1));

        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID);
        mMediator.resetWithListOfTabs(new ArrayList<>(Arrays.asList(mTab1)), false, false);
        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.SEARCH_QUERY), equalTo(null));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    public void
    testSearchTermProperty_TabGroups_Dialog() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_GRID_DIALOG);
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID);
        String searchTerm1 = "hello world";
        TabAttributeCache.setLastSearchTermForTesting(TAB1_ID, searchTerm1);

        mMediator.resetWithListOfTabs(new ArrayList<>(Arrays.asList(mTab1)), false, false);
        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.SEARCH_QUERY), equalTo(searchTerm1));
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
    public void
    testSearchTermProperty_TabGroups_Strip() {
        setUpForTabGroupOperation(TabListMediatorType.TAB_STRIP);
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID);
        String searchTerm1 = "hello world";
        TabAttributeCache.setLastSearchTermForTesting(TAB1_ID, searchTerm1);

        mMediator.resetWithListOfTabs(new ArrayList<>(Arrays.asList(mTab1)), false, false);
        assertThat(mModel.size(), equalTo(1));
        assertThat(mModel.get(0).model.get(TabProperties.SEARCH_QUERY), equalTo(null));
    }

    @Test
    public void navigateToLastSearchQuery() {
        initAndAssertAllProperties();

        String otherUrl = "https://example.com";
        String searchUrl = "https://www.google.com/search?q=test";
        String searchTerm = "test";
        String searchUrl2 = "https://www.google.com/search?q=query";
        String searchTerm2 = "query";
        TemplateUrlService service = Mockito.mock(TemplateUrlService.class);
        doReturn(null).when(service).getSearchQueryForUrl(otherUrl);
        doReturn(searchTerm).when(service).getSearchQueryForUrl(searchUrl);
        doReturn(searchTerm2).when(service).getSearchQueryForUrl(searchUrl2);
        TemplateUrlServiceFactory.setInstanceForTesting(service);

        WebContents webContents = mock(WebContents.class);
        doReturn(webContents).when(mTab1).getWebContents();
        NavigationController navigationController = mock(NavigationController.class);
        doReturn(navigationController).when(webContents).getNavigationController();
        NavigationHistory navigationHistory = mock(NavigationHistory.class);
        doReturn(navigationHistory).when(navigationController).getNavigationHistory();
        doReturn(true).when(navigationController).canGoToOffset(anyInt());
        doReturn(2).when(navigationHistory).getCurrentEntryIndex();
        NavigationEntry navigationEntry1 = mock(NavigationEntry.class);
        NavigationEntry navigationEntry0 = mock(NavigationEntry.class);
        doReturn(navigationEntry1).when(navigationHistory).getEntryAtIndex(1);
        doReturn(navigationEntry0).when(navigationHistory).getEntryAtIndex(0);

        InOrder inOrder = Mockito.inOrder(mTab1);

        // No searches.
        doReturn(otherUrl).when(navigationEntry1).getOriginalUrl();
        doReturn(otherUrl).when(navigationEntry0).getOriginalUrl();
        TabListMediator.SearchTermChipUtils.navigateToLastSearchQuery(mTab1);
        inOrder.verify(mTab1, never()).loadUrl(any());

        // Has SRP.
        doReturn(searchUrl).when(navigationEntry1).getOriginalUrl();
        doReturn(otherUrl).when(navigationEntry0).getOriginalUrl();
        TabListMediator.SearchTermChipUtils.navigateToLastSearchQuery(mTab1);
        inOrder.verify(mTab1).loadUrl(
                refEq(new LoadUrlParams(searchUrl, PageTransition.KEYWORD_GENERATED)));

        // Has earlier SRP.
        doReturn(otherUrl).when(navigationEntry1).getOriginalUrl();
        doReturn(searchUrl2).when(navigationEntry0).getOriginalUrl();
        TabListMediator.SearchTermChipUtils.navigateToLastSearchQuery(mTab1);
        inOrder.verify(mTab1).loadUrl(
                refEq(new LoadUrlParams(searchUrl2, PageTransition.KEYWORD_GENERATED)));

        // Latest one wins.
        doReturn(searchUrl).when(navigationEntry1).getOriginalUrl();
        doReturn(searchUrl2).when(navigationEntry0).getOriginalUrl();
        TabListMediator.SearchTermChipUtils.navigateToLastSearchQuery(mTab1);
        inOrder.verify(mTab1).loadUrl(
                refEq(new LoadUrlParams(searchUrl, PageTransition.KEYWORD_GENERATED)));

        // Rejected by canGoToOffset().
        doReturn(false).when(navigationController).canGoToOffset(eq(-1));
        TabListMediator.SearchTermChipUtils.navigateToLastSearchQuery(mTab1);
        inOrder.verify(mTab1).loadUrl(
                refEq(new LoadUrlParams(searchUrl2, PageTransition.KEYWORD_GENERATED)));

        // Reset canGoToOffset().
        doReturn(true).when(navigationController).canGoToOffset(anyInt());
        TabListMediator.SearchTermChipUtils.navigateToLastSearchQuery(mTab1);
        inOrder.verify(mTab1).loadUrl(
                refEq(new LoadUrlParams(searchUrl, PageTransition.KEYWORD_GENERATED)));

        // Only care about previous ones.
        doReturn(1).when(navigationHistory).getCurrentEntryIndex();
        TabListMediator.SearchTermChipUtils.navigateToLastSearchQuery(mTab1);
        inOrder.verify(mTab1).loadUrl(
                refEq(new LoadUrlParams(searchUrl2, PageTransition.KEYWORD_GENERATED)));
    }

    @Test
    public void searchListener() {
        initAndAssertAllProperties();

        String otherUrl = "https://example.com";
        String searchUrl = "https://www.google.com/search?q=test";
        String searchTerm = "test";
        TemplateUrlService service = Mockito.mock(TemplateUrlService.class);
        doReturn(null).when(service).getSearchQueryForUrl(otherUrl);
        doReturn(searchTerm).when(service).getSearchQueryForUrl(searchUrl);
        TemplateUrlServiceFactory.setInstanceForTesting(service);

        WebContents webContents = mock(WebContents.class);
        doReturn(webContents).when(mTab1).getWebContents();
        NavigationController navigationController = mock(NavigationController.class);
        doReturn(navigationController).when(webContents).getNavigationController();
        NavigationHistory navigationHistory = mock(NavigationHistory.class);
        doReturn(navigationHistory).when(navigationController).getNavigationHistory();
        doReturn(true).when(navigationController).canGoToOffset(anyInt());
        doReturn(2).when(navigationHistory).getCurrentEntryIndex();
        NavigationEntry navigationEntry1 = mock(NavigationEntry.class);
        NavigationEntry navigationEntry0 = mock(NavigationEntry.class);
        doReturn(navigationEntry1).when(navigationHistory).getEntryAtIndex(1);
        doReturn(navigationEntry0).when(navigationHistory).getEntryAtIndex(0);
        doReturn(otherUrl).when(navigationEntry1).getOriginalUrl();
        doReturn(searchUrl).when(navigationEntry0).getOriginalUrl();

        mModel.get(0)
                .model.get(TabProperties.SEARCH_LISTENER)
                .run(mModel.get(0).model.get(TabProperties.TAB_ID));

        verify(mGridCardOnClickListenerProvider)
                .onTabSelecting(mModel.get(0).model.get(TabProperties.TAB_ID));
        verify(mTab1).loadUrl(
                refEq(new LoadUrlParams(searchUrl, PageTransition.KEYWORD_GENERATED)));
    }

    @Test
    public void searchListener_frozenTab() {
        initAndAssertAllProperties();

        String searchUrl = "https://www.google.com/search?q=test";
        String searchTerm = "test";
        TemplateUrlService service = Mockito.mock(TemplateUrlService.class);
        doReturn(searchTerm).when(service).getSearchQueryForUrl(searchUrl);
        TemplateUrlServiceFactory.setInstanceForTesting(service);

        WebContents webContents = mock(WebContents.class);
        NavigationController navigationController = mock(NavigationController.class);
        doReturn(navigationController).when(webContents).getNavigationController();
        NavigationHistory navigationHistory = mock(NavigationHistory.class);
        doReturn(navigationHistory).when(navigationController).getNavigationHistory();
        doReturn(true).when(navigationController).canGoToOffset(anyInt());
        doReturn(1).when(navigationHistory).getCurrentEntryIndex();
        NavigationEntry navigationEntry = mock(NavigationEntry.class);
        doReturn(navigationEntry).when(navigationHistory).getEntryAtIndex(0);
        doReturn(searchUrl).when(navigationEntry).getOriginalUrl();

        mModel.get(0)
                .model.get(TabProperties.SEARCH_LISTENER)
                .run(mModel.get(0).model.get(TabProperties.TAB_ID));

        verify(mGridCardOnClickListenerProvider)
                .onTabSelecting(mModel.get(0).model.get(TabProperties.TAB_ID));
        verify(navigationController, never()).goToOffset(0);

        doReturn(webContents).when(mTab1).getWebContents();
        mTabObserverCaptor.getValue().onPageLoadStarted(mTab1, searchUrl);
        verify(mTab1).loadUrl(
                refEq(new LoadUrlParams(searchUrl, PageTransition.KEYWORD_GENERATED)));
    }

    @Test
    public void testSearchChipAdaptiveIcon_Disabled() {
        // Mock that google is the default search engine, and the search chip adaptive icon field
        // is set as false.
        TabListMediator.SearchTermChipUtils.setIsSearchChipAdaptiveIconEnabledForTesting(false);
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        doReturn(mTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(Arrays.asList(mTab1)).when(mTabModelFilter).getRelatedTabList(eq(TAB1_ID));
        doReturn(Arrays.asList(mTab2)).when(mTabModelFilter).getRelatedTabList(eq(TAB2_ID));

        // Re-initialize the mediator to setup TemplateUrlServiceObserver if needed.
        mMediator = new TabListMediator(mContext, mModel, mTabModelSelector,
                mTabContentManager::getTabThumbnailWithCallback, mTitleProvider,
                mTabListFaviconProvider, true, null, null, null, null, getClass().getSimpleName(),
                TabProperties.UiType.CLOSABLE);

        initAndAssertAllProperties();

        // When the search chip adaptive icon is turned off, the search chip icon is initialized as
        // R.drawable.ic_search even if the default search engine is google.
        for (int i = 0; i < mModel.size(); i++) {
            assertThat(mModel.get(i).model.get(TabProperties.SEARCH_CHIP_ICON_DRAWABLE_ID),
                    equalTo(R.drawable.ic_search));
        }
    }

    @Test
    public void testSearchChipAdaptiveIcon_ChangeWithSetting() {
        // Mock that google is the default search engine, and the search chip adaptive icon is
        // turned on.
        TabListMediator.SearchTermChipUtils.setIsSearchChipAdaptiveIconEnabledForTesting(true);
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        doReturn(mTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(Arrays.asList(mTab1)).when(mTabModelFilter).getRelatedTabList(eq(TAB1_ID));
        doReturn(Arrays.asList(mTab2)).when(mTabModelFilter).getRelatedTabList(eq(TAB2_ID));

        // Re-initialize the mediator to setup TemplateUrlServiceObserver if needed.
        mMediator = new TabListMediator(mContext, mModel, mTabModelSelector,
                mTabContentManager::getTabThumbnailWithCallback, mTitleProvider,
                mTabListFaviconProvider, true, null, null, null, null, getClass().getSimpleName(),
                TabProperties.UiType.CLOSABLE);

        initAndAssertAllProperties();

        // The search chip icon should be initialized as R.drawable.ic_logo_googleg_24dp.
        for (int i = 0; i < mModel.size(); i++) {
            assertThat(mModel.get(i).model.get(TabProperties.SEARCH_CHIP_ICON_DRAWABLE_ID),
                    equalTo(R.drawable.ic_logo_googleg_24dp));
        }

        // Mock that user has switched to a non-google search engine as default.
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mTemplateUrlServiceObserver.getValue().onTemplateURLServiceChanged();

        // The search chip icon should be updated to R.drawable.ic_search.
        for (int i = 0; i < mModel.size(); i++) {
            assertThat(mModel.get(i).model.get(TabProperties.SEARCH_CHIP_ICON_DRAWABLE_ID),
                    equalTo(R.drawable.ic_search));
        }

        // Mock that user has switched to google as default search engine.
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mTemplateUrlServiceObserver.getValue().onTemplateURLServiceChanged();

        // The search chip icon should be updated as R.drawable.ic_logo_googleg_24dp.
        for (int i = 0; i < mModel.size(); i++) {
            assertThat(mModel.get(i).model.get(TabProperties.SEARCH_CHIP_ICON_DRAWABLE_ID),
                    equalTo(R.drawable.ic_logo_googleg_24dp));
        }
    }

    private void initAndAssertAllProperties() {
        List<Tab> tabs = new ArrayList<>();
        for (int i = 0; i < mTabModel.getCount(); i++) {
            tabs.add(mTabModel.getTabAt(i));
        }
        mMediator.resetWithListOfTabs(tabs, false, false);
        for (Callback<Drawable> callback : mCallbackCaptor.getAllValues()) {
            callback.onResult(new ColorDrawable(Color.RED));
        }

        assertThat(mModel.size(), equalTo(2));

        assertThat(mModel.get(0).model.get(TabProperties.TAB_ID), equalTo(TAB1_ID));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_ID), equalTo(TAB2_ID));

        assertThat(mModel.get(0).model.get(TabProperties.TITLE), equalTo(TAB1_TITLE));
        assertThat(mModel.get(1).model.get(TabProperties.TITLE), equalTo(TAB2_TITLE));

        assertThat(mModel.get(0).model.get(TabProperties.FAVICON), instanceOf(Drawable.class));
        assertThat(mModel.get(1).model.get(TabProperties.FAVICON), instanceOf(Drawable.class));

        assertThat(mModel.get(0).model.get(TabProperties.URL), equalTo(TAB1_URL));
        assertThat(mModel.get(1).model.get(TabProperties.URL), equalTo(TAB2_URL));

        assertThat(mModel.get(0).model.get(TabProperties.IS_SELECTED), equalTo(true));
        assertThat(mModel.get(1).model.get(TabProperties.IS_SELECTED), equalTo(false));

        assertThat(mModel.get(0).model.get(TabProperties.THUMBNAIL_FETCHER),
                instanceOf(TabListMediator.ThumbnailFetcher.class));
        assertThat(mModel.get(1).model.get(TabProperties.THUMBNAIL_FETCHER),
                instanceOf(TabListMediator.ThumbnailFetcher.class));

        assertThat(mModel.get(0).model.get(TabProperties.TAB_SELECTED_LISTENER),
                instanceOf(TabListMediator.TabActionListener.class));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_SELECTED_LISTENER),
                instanceOf(TabListMediator.TabActionListener.class));

        assertThat(mModel.get(0).model.get(TabProperties.TAB_CLOSED_LISTENER),
                instanceOf(TabListMediator.TabActionListener.class));
        assertThat(mModel.get(1).model.get(TabProperties.TAB_CLOSED_LISTENER),
                instanceOf(TabListMediator.TabActionListener.class));
    }

    private TabImpl prepareTab(int id, String title, String url) {
        TabImpl tab = mock(TabImpl.class);
        when(tab.getView()).thenReturn(mock(View.class));
        when(tab.getUserDataHost()).thenReturn(new UserDataHost());
        doReturn(id).when(tab).getId();
        doReturn(id).when(tab).getRootId();
        doReturn(url).when(tab).getUrl();
        doReturn(title).when(tab).getTitle();
        doReturn(true).when(tab).isIncognito();
        doReturn(title).when(mTitleProvider).getTitle(tab);
        return tab;
    }

    private SimpleRecyclerViewAdapter.ViewHolder prepareViewHolder(int id, int position) {
        SimpleRecyclerViewAdapter.ViewHolder viewHolder =
                mock(SimpleRecyclerViewAdapter.ViewHolder.class);
        PropertyModel model = new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                                      .with(TabProperties.TAB_ID, id)
                                      .build();
        viewHolder.model = model;
        doReturn(position).when(viewHolder).getAdapterPosition();
        return viewHolder;
    }

    private RecyclerView.ViewHolder prepareDummyViewHolder(View itemView, int index) {
        RecyclerView.ViewHolder viewHolder = new RecyclerView.ViewHolder(itemView) {};
        when(mRecyclerView.findViewHolderForAdapterPosition(index)).thenReturn(viewHolder);
        return viewHolder;
    }

    private TabGridItemTouchHelperCallback getItemTouchHelperCallback() {
        return (TabGridItemTouchHelperCallback) mMediator.getItemTouchHelperCallback(
                0f, 0f, 0f, mProfile);
    }

    private void setUpForTabGroupOperation(@TabListMediatorType int type) {
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(true);
        doReturn(mTabGroupModelFilter).when(mTabModelFilterProvider).getTabModelFilter(false);
        doNothing()
                .when(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());
        doNothing()
                .when(mTabModelFilterProvider)
                .addTabModelFilterObserver(mTabModelObserverCaptor.capture());

        TabListMediator.TabGridDialogHandler handler =
                type == TabListMediatorType.TAB_GRID_DIALOG ? mTabGridDialogHandler : null;
        boolean actionOnRelatedTabs = type == TabListMediatorType.TAB_SWITCHER;
        int uiType = 0;
        if (type == TabListMediatorType.TAB_SWITCHER
                || type == TabListMediatorType.TAB_GRID_DIALOG) {
            uiType = TabProperties.UiType.CLOSABLE;
        } else if (type == TabListMediatorType.TAB_STRIP) {
            uiType = TabProperties.UiType.STRIP;
        }
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);

        TabListMediator.SearchTermChipUtils.setIsSearchChipAdaptiveIconEnabledForTesting(false);
        mMediator = new TabListMediator(mContext, mModel, mTabModelSelector,
                mTabContentManager::getTabThumbnailWithCallback, mTitleProvider,
                mTabListFaviconProvider, actionOnRelatedTabs, null, null, null, handler,
                getClass().getSimpleName(), uiType);

        // There are two TabModelObserver and two TabGroupModelFilter.Observer added when
        // initializing TabListMediator, one set from TabListMediator and the other from
        // TabGroupTitleEditor. Here we only test the ones from TabListMediator.
        mMediatorTabModelObserver = mTabModelObserverCaptor.getAllValues().get(1);
        mMediatorTabGroupModelFilterObserver =
                mTabGroupModelFilterObserverCaptor.getAllValues().get(0);

        initAndAssertAllProperties();
    }

    private void createTabGroup(List<Tab> tabs, int rootId) {
        for (Tab tab : tabs) {
            when(mTabGroupModelFilter.getRelatedTabList(tab.getId())).thenReturn(tabs);
            doReturn(rootId).when((TabImpl) tab).getRootId();
        }
    }
}

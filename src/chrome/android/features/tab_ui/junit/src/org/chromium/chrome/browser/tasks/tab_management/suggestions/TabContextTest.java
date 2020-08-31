// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.WebContents;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

/**
 * Tests functionality related to TabContext
 */
@SuppressWarnings({"ResultOfMethodCallIgnored", "ArraysAsListWithZeroOrOneArgument"})
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabContextTest {
    private static final int TAB_0_ID = 0;
    private static final int RELATED_TAB_0_ID = 1;
    private static final int RELATED_TAB_1_ID = 2;
    private static final int NEW_TAB_1_ID = 3;
    private static final int NEW_TAB_2_ID = 4;
    private static final int LAST_COMMITTED_INDEX = 1;

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    public Profile.Natives mMockProfileNatives;

    @Mock
    private TabModelSelector mTabModelSelector;

    @Mock
    private TabModelFilterProvider mTabModelFilterProvider;

    @Mock
    private TabModelFilter mTabModelFilter;

    private Tab mTab0 = mockTab(TAB_0_ID, 6, "mock_title_tab_0", "mock_url_tab_0",
            "mock_original_url_tab_0", "mock_referrer_url_tab_0", 100);
    private Tab mRelatedTab0 =
            mockTab(RELATED_TAB_0_ID, 6, "mock_title_related_tab_0", "mock_url_related_tab_0",
                    "mock_original_url_related_tab_0", "mock_referrer_url_related_tab_0", 200);
    private Tab mRelatedTab1 =
            mockTab(RELATED_TAB_1_ID, 6, "mock_title_related_tab_1", "mock_url_related_tab_1",
                    "mock_original_url_related_tab_1", "mock_referrer_url_related_tab_1", 300);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(ProfileJni.TEST_HOOKS, mMockProfileNatives);
        doReturn(mTabModelFilterProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doReturn(mTabModelFilter).when(mTabModelFilterProvider).getCurrentTabModelFilter();
    }

    private static TabImpl mockTab(int id, int rootId, String title, String url, String originalUrl,
            String referrerUrl, long getTimestampMillis) {
        TabImpl tab = mock(TabImpl.class);
        doReturn(id).when(tab).getId();
        doReturn(rootId).when(tab).getRootId();
        doReturn(title).when(tab).getTitle();
        doReturn(url).when(tab).getUrlString();
        doReturn(originalUrl).when(tab).getOriginalUrl();
        WebContents webContents = mock(WebContents.class);
        GURL gurl = mock(GURL.class);
        doReturn("").when(gurl).getSpec();
        doReturn(gurl).when(webContents).getVisibleUrl();
        doReturn(webContents).when(tab).getWebContents();
        NavigationController navigationController = mock(NavigationController.class);
        doReturn(navigationController).when(webContents).getNavigationController();
        doReturn(LAST_COMMITTED_INDEX).when(navigationController).getLastCommittedEntryIndex();
        NavigationEntry navigationEntry = mock(NavigationEntry.class);
        doReturn(navigationEntry)
                .when(navigationController)
                .getEntryAtIndex(eq(LAST_COMMITTED_INDEX));
        doReturn(referrerUrl).when(navigationEntry).getReferrerUrl();
        return tab;
    }

    /**
     * Test finding related tabs
     */
    @Test
    public void testRelatedTabsExist() {
        doReturn(mTab0).when(mTabModelFilter).getTabAt(eq(TAB_0_ID));
        doReturn(1).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(mTab0, mRelatedTab0, mRelatedTab1))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB_0_ID));
        TabContext tabContext = TabContext.createCurrentContext(mTabModelSelector);
        Assert.assertEquals(tabContext.getUngroupedTabs().size(), 0);
        List<TabContext.TabGroupInfo> tabGroupInfo = tabContext.getTabGroups();
        Assert.assertEquals(1, tabGroupInfo.size());
        List<TabContext.TabInfo> groupedTabs = tabGroupInfo.get(0).tabs;
        Assert.assertEquals(3, groupedTabs.size());
        Assert.assertEquals(TAB_0_ID, groupedTabs.get(0).id);
        Assert.assertEquals(RELATED_TAB_0_ID, groupedTabs.get(1).id);
        Assert.assertEquals(RELATED_TAB_1_ID, groupedTabs.get(2).id);
    }

    /**
     * Test finding no related tabs
     */
    @Test
    public void testFindNoRelatedTabs() {
        doReturn(mTab0).when(mTabModelFilter).getTabAt(eq(TAB_0_ID));
        doReturn(1).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(mTab0)).when(mTabModelFilter).getRelatedTabList(eq(TAB_0_ID));
        TabContext tabContext = TabContext.createCurrentContext(mTabModelSelector);
        Assert.assertEquals(tabContext.getUngroupedTabs().size(), 1);
        List<TabContext.TabGroupInfo> tabGroups = tabContext.getTabGroups();
        Assert.assertEquals(0, tabGroups.size());
        List<TabContext.TabInfo> ungroupedTabs = tabContext.getUngroupedTabs();
        Assert.assertEquals(1, ungroupedTabs.size());
        Assert.assertEquals(TAB_0_ID, ungroupedTabs.get(0).id);
    }

    @Test
    public void testExcludeClosingTabs() {
        Tab newTab1 = mockTab(NEW_TAB_1_ID, NEW_TAB_1_ID, "", "", "", "", 0);
        Tab newTab2 = mockTab(NEW_TAB_2_ID, NEW_TAB_2_ID, "", "", "", "", 0);
        doReturn(mTab0).when(mTabModelFilter).getTabAt(eq(TAB_0_ID));
        doReturn(newTab1).when(mTabModelFilter).getTabAt(eq(TAB_0_ID + 1));
        doReturn(newTab2).when(mTabModelFilter).getTabAt(eq(TAB_0_ID + 2));
        doReturn(3).when(mTabModelFilter).getCount();
        doReturn(Arrays.asList(mTab0, mRelatedTab0, mRelatedTab1))
                .when(mTabModelFilter)
                .getRelatedTabList(eq(TAB_0_ID));
        doReturn(Arrays.asList(newTab1)).when(mTabModelFilter).getRelatedTabList(eq(NEW_TAB_1_ID));
        doReturn(Arrays.asList(newTab2)).when(mTabModelFilter).getRelatedTabList(eq(NEW_TAB_2_ID));

        TabContext tabContext = TabContext.createCurrentContext(mTabModelSelector);
        Assert.assertEquals(2, tabContext.getUngroupedTabs().size());
        Assert.assertEquals(1, tabContext.getTabGroups().size());
        Assert.assertEquals(3, tabContext.getTabGroups().get(0).tabs.size());

        // close newTab1
        doReturn(true).when(newTab1).isClosing();
        tabContext = TabContext.createCurrentContext(mTabModelSelector);
        Assert.assertEquals(1, tabContext.getUngroupedTabs().size());

        // close mTab0
        doReturn(true).when(mTab0).isClosing();
        tabContext = TabContext.createCurrentContext(mTabModelSelector);
        Assert.assertEquals(1, tabContext.getTabGroups().size());
        Assert.assertEquals(2, tabContext.getTabGroups().get(0).tabs.size());
    }
}

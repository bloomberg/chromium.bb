// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static junit.framework.Assert.assertEquals;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.webapps.WebappExtras;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.WebContents;

import java.util.Arrays;
import java.util.Collection;
import java.util.Stack;

/**
 * Tests for {@link CloseButtonNavigator}.
 */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(sdk = 21, manifest = Config.NONE)
public class CloseButtonNavigatorTest {

    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {{true}, {false}});
    }

    @Parameter(0)
    public boolean mIsWebapp;

    @Mock public CustomTabActivityTabController mTabController;
    @Mock public CustomTabActivityTabProvider mTabProvider;
    @Mock
    public WebappExtras mWebappExtras;
    @Mock
    public BrowserServicesIntentDataProvider mIntentDataProvider;

    private final Stack<Tab> mTabs = new Stack<>();
    private CloseButtonNavigator mCloseButtonNavigator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        if (!mIsWebapp) {
            mWebappExtras = null;
        }
        doReturn(mWebappExtras).when(mIntentDataProvider).getWebappExtras();
        doReturn(mIsWebapp ? ActivityType.WEBAPP : ActivityType.CUSTOM_TAB)
                .when(mIntentDataProvider)
                .getActivityType();

        mCloseButtonNavigator =
                new CloseButtonNavigator(mTabController, mTabProvider, mIntentDataProvider);

        // Set up our mTabs to act as the mock tab model:
        // - mTabController.closeTab removes the top tab.
        // - mTabProvider.getTab returns the top tab.
        Mockito.doAnswer((invocation) -> {
            mTabs.pop();
            return null;  // Annoyingly we have to return something.
        }).when(mTabController).closeTab();
        when(mTabProvider.getTab()).thenAnswer(invocation -> {
            if (mTabs.empty()) return null;
            return mTabs.peek();
        });
    }

    private Tab createTabWithNavigationHistory(String... urls) {
        NavigationHistory history = new NavigationHistory();

        for (String url : urls) {
            history.addEntry(new NavigationEntry(0, url, "", "", "", "", null, 0, 0));
        }

        // Point to the most recent entry in history.
        history.setCurrentEntryIndex(history.getEntryCount() - 1);

        Tab tab = mock(Tab.class);
        WebContents webContents = mock(WebContents.class);
        NavigationController navigationController = mock(NavigationController.class);

        when(tab.getUrlString())
                .thenAnswer(invocation
                        -> history.getEntryAtIndex(history.getCurrentEntryIndex()).getUrl());
        when(tab.getWebContents()).thenReturn(webContents);
        when(webContents.getNavigationController()).thenReturn(navigationController);
        when(navigationController.getNavigationHistory()).thenReturn(history);
        setParentTabId(tab, Tab.INVALID_TAB_ID);

        return tab;
    }

    private void setParentTabId(Tab childTab, int parentTabId) {
        when(childTab.getParentId()).thenReturn(parentTabId);
    }

    private NavigationController currentTabsNavigationController() {
        // The navigation controller will be a mock object created in the above method.
        return mTabs.peek().getWebContents().getNavigationController();
    }

    /** Example criteria. */
    private static boolean isRed(String url) {
        return url.contains("red");
    }

    @Test
    public void noCriteria_singleTab() {
        mTabs.push(createTabWithNavigationHistory(
                "www.blue.com/page1",
                "www.blue.com/page2"
        ));

        mCloseButtonNavigator.navigateOnClose();

        assertTrue(mTabs.empty());
    }

    @Test
    public void noCriteria_multipleTabs() {
        mTabs.push(createTabWithNavigationHistory( "www.blue.com/page1"));
        mTabs.push(createTabWithNavigationHistory( "www.blue.com/page2"));
        setParentTabId(mTabs.get(1), mTabs.get(0).getId());

        mCloseButtonNavigator.navigateOnClose();

        if (mIsWebapp) {
            assertEquals(1, mTabs.size());
            verify(currentTabsNavigationController(), never()).goToNavigationIndex(anyInt());
        } else {
            assertTrue(mTabs.empty());
        }
    }

    @Test
    public void noMatchingUrl_singleTab() {
        mCloseButtonNavigator.setLandingPageCriteria(CloseButtonNavigatorTest::isRed);
        mTabs.push(createTabWithNavigationHistory(
                "www.blue.com/page1",
                "www.blue.com/page2"
        ));

        mCloseButtonNavigator.navigateOnClose();

        assertTrue(mTabs.empty());
    }

    @Test
    public void noMatchingUrl_multipleTabs() {
        mCloseButtonNavigator.setLandingPageCriteria(CloseButtonNavigatorTest::isRed);
        mTabs.push(createTabWithNavigationHistory( "www.blue.com/page1"));
        mTabs.push(createTabWithNavigationHistory( "www.blue.com/page2"));
        setParentTabId(mTabs.get(1), mTabs.get(0).getId());

        mCloseButtonNavigator.navigateOnClose();

        if (mIsWebapp) {
            assertEquals(1, mTabs.size());
            verify(currentTabsNavigationController(), never()).goToNavigationIndex(anyInt());
        } else {
            assertTrue(mTabs.empty());
        }
    }

    @Test
    public void matchingUrl_singleTab() {
        mCloseButtonNavigator.setLandingPageCriteria(CloseButtonNavigatorTest::isRed);
        mTabs.push(createTabWithNavigationHistory(
                "www.red.com/page1",
                "www.red.com/page2",
                "www.blue.com/page1",
                "www.blue.com/page2"
        ));

        mCloseButtonNavigator.navigateOnClose();

        assertFalse(mTabs.isEmpty());
        verify(currentTabsNavigationController()).goToNavigationIndex(eq(1));
        // Ensure it was only called with that value.
        verify(currentTabsNavigationController()).goToNavigationIndex(anyInt());
    }

    @Test
    public void matchingUrl_startOfNextTab() {
        mCloseButtonNavigator.setLandingPageCriteria(CloseButtonNavigatorTest::isRed);
        mTabs.push(createTabWithNavigationHistory(
                "www.red.com/page1",
                "www.red.com/page2"
        ));
        mTabs.push(createTabWithNavigationHistory(
                "www.blue.com/page1",
                "www.blue.com/page2"
        ));
        setParentTabId(mTabs.get(1), mTabs.get(0).getId());

        mCloseButtonNavigator.navigateOnClose();

        assertEquals(1, mTabs.size());
        verify(currentTabsNavigationController(), never()).goToNavigationIndex(anyInt());
    }

    @Test
    public void matchingUrl_middleOfNextTab() {
        mCloseButtonNavigator.setLandingPageCriteria(CloseButtonNavigatorTest::isRed);
        mTabs.push(createTabWithNavigationHistory(
                "www.red.com/page1",
                "www.blue.com/page1"
        ));
        mTabs.push(createTabWithNavigationHistory(
                "www.blue.com/page2",
                "www.blue.com/page3"
        ));
        setParentTabId(mTabs.get(1), mTabs.get(0).getId());

        mCloseButtonNavigator.navigateOnClose();

        assertEquals(1, mTabs.size());
        if (mIsWebapp) {
            verify(currentTabsNavigationController(), never()).goToNavigationIndex(anyInt());
        } else {
            verify(currentTabsNavigationController()).goToNavigationIndex(eq(0));
            verify(currentTabsNavigationController()).goToNavigationIndex(anyInt());
        }
    }

    @Test
    public void middleOfHistory() {
        mCloseButtonNavigator.setLandingPageCriteria(CloseButtonNavigatorTest::isRed);
        mTabs.push(createTabWithNavigationHistory(
                "www.red.com/page1",
                "www.red.com/page2",
                "www.blue.com/page1",
                "www.blue.com/page2",
                "www.red.com/page3"
        ));

        mTabs.peek().getWebContents().getNavigationController().getNavigationHistory()
                .setCurrentEntryIndex(3);

        mCloseButtonNavigator.navigateOnClose();

        assertEquals(1, mTabs.size());
        verify(currentTabsNavigationController()).goToNavigationIndex(eq(1));
        verify(currentTabsNavigationController()).goToNavigationIndex(anyInt());
    }

    @Test
    public void navigateFromLandingPage() {
        mCloseButtonNavigator.setLandingPageCriteria(CloseButtonNavigatorTest::isRed);
        mTabs.push(createTabWithNavigationHistory(
                "www.red.com/page1",
                "www.red.com/page2",
                "www.blue.com/page1",
                "www.blue.com/page2",
                "www.red.com/page3"
        ));

        mCloseButtonNavigator.navigateOnClose();

        assertEquals(1, mTabs.size());
        verify(currentTabsNavigationController()).goToNavigationIndex(eq(1));
        verify(currentTabsNavigationController()).goToNavigationIndex(anyInt());
    }
}

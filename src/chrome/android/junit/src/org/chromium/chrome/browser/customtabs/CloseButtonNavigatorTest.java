// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;

/**
 * Tests for {@link CloseButtonNavigator}.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class CloseButtonNavigatorTest {
    @Mock public NavigationController mNavigationController;
    private final NavigationHistory mNavigationHistory = new NavigationHistory();

    private final CloseButtonNavigator mCloseButtonNavigator = new CloseButtonNavigator();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mNavigationController.getNavigationHistory()).thenReturn(mNavigationHistory);
    }

    private void addSitesToHistory(String... urls) {
        for (String url : urls) {
            mNavigationHistory.addEntry(new NavigationEntry(0, url, "", "", "", "", null, 0));
        }

        // Point to the most recent entry in history.
        mNavigationHistory.setCurrentEntryIndex(mNavigationHistory.getEntryCount() - 1);
    }

    /** Example criteria. */
    private static boolean isRed(String url) {
        return url.contains("red");
    }

    @Test
    public void noCriteria() {
        addSitesToHistory(
                "www.blue.com/page1",
                "www.blue.com/page2"
        );

        mCloseButtonNavigator.navigateOnClose(mNavigationController);

        verify(mNavigationController, never()).goToNavigationIndex(anyInt());
    }

    @Test
    public void matchingUrl() {
        addSitesToHistory(
                "www.red.com/page1",
                "www.red.com/page2",
                "www.blue.com/page1",
                "www.blue.com/page2"
        );

        mCloseButtonNavigator.setLandingPageCriteria(CloseButtonNavigatorTest::isRed);
        mCloseButtonNavigator.navigateOnClose(mNavigationController);

        verify(mNavigationController).goToNavigationIndex(eq(1));  // www.red.com/page2
        // Verify that it wasn't called with any other index.
        verify(mNavigationController).goToNavigationIndex(anyInt());
    }

    @Test
    public void noMatchingUrl() {
        addSitesToHistory(
                "www.blue.com/page1",
                "www.blue.com/page2"
        );

        mCloseButtonNavigator.setLandingPageCriteria(CloseButtonNavigatorTest::isRed);
        mCloseButtonNavigator.navigateOnClose(mNavigationController);

        verify(mNavigationController, never()).goToNavigationIndex(anyInt());
    }

    @Test
    public void inMiddleOfHistory() {
        addSitesToHistory(
                "www.red.com/page1",
                "www.red.com/page2",
                "www.blue.com/page1",
                "www.blue.com/page2",
                "www.red.com/page3"
        );
        mNavigationHistory.setCurrentEntryIndex(3);  // www.blue.com/page2

        mCloseButtonNavigator.setLandingPageCriteria(CloseButtonNavigatorTest::isRed);
        mCloseButtonNavigator.navigateOnClose(mNavigationController);

        verify(mNavigationController).goToNavigationIndex(eq(1));  // www.red.com/page2
        // Verify that it wasn't called with any other index.
        verify(mNavigationController).goToNavigationIndex(anyInt());
    }
}

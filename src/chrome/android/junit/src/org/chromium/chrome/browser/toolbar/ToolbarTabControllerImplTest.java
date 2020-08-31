// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.text.TextUtils;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

/** Unit tests for ToolbarTabControllerImpl. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ToolbarTabControllerImplTest {
    private class LoadUrlParamsMatcher implements ArgumentMatcher<LoadUrlParams> {
        LoadUrlParams mLoadUrlParams;
        public LoadUrlParamsMatcher(LoadUrlParams loadUrlParams) {
            mLoadUrlParams = loadUrlParams;
        }

        @Override
        public boolean matches(LoadUrlParams argument) {
            return argument.getUrl().equals(mLoadUrlParams.getUrl())
                    && argument.getTransitionType() == mLoadUrlParams.getTransitionType();
        }
    }

    @Mock
    private Supplier<Tab> mTabSupplier;
    @Mock
    private Tab mTab;
    @Mock
    private Supplier<Boolean> mBottomToolbarVisibilitySupplier;
    @Mock
    private Supplier<Boolean> mOverrideHomePageSupplier;
    @Mock
    private Supplier<Profile> mProfileSupplier;
    @Mock
    private Supplier<BottomControlsCoordinator> mBottomControlsCoordinatorSupplier;
    @Mock
    private BottomControlsCoordinator mBottomControlsCoordinator;
    @Mock
    Tracker mTracker;
    @Mock
    private Runnable mRunnable;

    private ToolbarTabControllerImpl mToolbarTabController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mTab).when(mTabSupplier).get();
        doReturn(false).when(mBottomToolbarVisibilitySupplier).get();
        doReturn(false).when(mOverrideHomePageSupplier).get();
        TrackerFactory.setTrackerForTests(mTracker);
        mToolbarTabController = new ToolbarTabControllerImpl(mTabSupplier,
                mBottomToolbarVisibilitySupplier, mOverrideHomePageSupplier, mProfileSupplier,
                mBottomControlsCoordinatorSupplier, mRunnable);
    }

    @Test
    public void backForward_NotTriggeredWhenTabCannot() {
        doReturn(false).when(mTab).canGoForward();
        doReturn(false).when(mTab).canGoBack();

        assertFalse(mToolbarTabController.forward());
        assertFalse(mToolbarTabController.back());
    }

    @Test
    public void backForward_correspondingTabActionsTriggered() {
        doReturn(true).when(mTab).canGoForward();
        doReturn(true).when(mTab).canGoBack();

        assertTrue(mToolbarTabController.forward());
        assertTrue(mToolbarTabController.back());
        verify(mRunnable, times(2)).run();
        verify(mTab).goForward();
        verify(mTab).goBack();
    }

    @Test
    public void back_handledByBottomControls() {
        doReturn(mBottomControlsCoordinator).when(mBottomControlsCoordinatorSupplier).get();
        doReturn(true).when(mBottomControlsCoordinator).onBackPressed();
        mToolbarTabController.back();

        verify(mBottomControlsCoordinator).onBackPressed();
        verify(mRunnable, never()).run();
        verify(mTab, never()).goBack();
    }

    @Test
    public void stopOrReloadCurrentTab() {
        doReturn(false).when(mTab).isLoading();
        mToolbarTabController.stopOrReloadCurrentTab();

        verify(mTab).reload();
        verify(mRunnable).run();

        doReturn(true).when(mTab).isLoading();
        mToolbarTabController.stopOrReloadCurrentTab();

        verify(mTab).stopLoading();
        verify(mRunnable, times(2)).run();
    }

    @Test
    public void openHomepage_handledByStartSurface() {
        doReturn(true).when(mOverrideHomePageSupplier).get();

        mToolbarTabController.openHomepage();
        verify(mTab, never()).loadUrl(any());
    }

    @Test
    public void openHomepage_loadsHomePage() {
        mToolbarTabController.openHomepage();
        String homePageUrl = HomepageManager.getHomepageUri();
        if (TextUtils.isEmpty(homePageUrl)) {
            homePageUrl = UrlConstants.NTP_URL;
        }
        verify(mTab).loadUrl(argThat(new LoadUrlParamsMatcher(
                new LoadUrlParams(homePageUrl, PageTransition.HOME_PAGE))));
    }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.junit.Assert.assertEquals;
import static org.mockito.AdditionalMatchers.not;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.BUTTONS_CLICKABLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_AT_START;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_CLICK_HANDLER;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_DESCRIPTION;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IMAGE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IDENTITY_DISC_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.INCOGNITO_SWITCHER_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IN_START_SURFACE_MODE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.LOGO_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.MENU_IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_BUTTON_AT_START;
import static org.chromium.chrome.browser.toolbar.top.StartSurfaceToolbarProperties.NEW_TAB_BUTTON_IS_VISIBLE;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeState;
import org.chromium.chrome.browser.identity_disc.IdentityDiscController;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.user_education.IPHCommandBuilder;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link StartSurfaceToolbarMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StartSurfaceToolbarMediatorUnitTest {
    private PropertyModel mPropertyModel;
    private StartSurfaceToolbarMediator mMediator;
    @Mock
    private OverviewModeBehavior mOverviewModeBehavior;
    @Mock
    TemplateUrlService mTemplateUrlService;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private TabModel mIncognitoTabModel;
    @Mock
    Runnable mDismissedCallback;
    @Mock
    View.OnClickListener mOnClickListener;
    @Mock
    IdentityDiscController mIdentityDiscController;
    @Mock
    private Resources mMockResources;
    @Mock
    private Drawable mDrawable;
    @Mock
    Drawable.ConstantState mMockConstantState;
    @Mock
    Callback<IPHCommandBuilder> mMockCallback;
    @Mock
    Tab mMockIncognitoTab;
    @Captor
    private ArgumentCaptor<OverviewModeObserver> mOverviewModeObserverCaptor;
    @Captor
    private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserver;
    @Captor
    private ArgumentCaptor<TemplateUrlServiceObserver> mTemplateUrlServiceObserver;

    private ButtonData mButtonData;
    private ButtonData mDisabledButtonData;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mPropertyModel = mPropertyModel =
                new PropertyModel.Builder(StartSurfaceToolbarProperties.ALL_KEYS)
                        .with(StartSurfaceToolbarProperties.INCOGNITO_SWITCHER_VISIBLE, true)
                        .with(StartSurfaceToolbarProperties.IN_START_SURFACE_MODE, false)
                        .with(StartSurfaceToolbarProperties.MENU_IS_VISIBLE, true)
                        .with(StartSurfaceToolbarProperties.IS_VISIBLE, true)
                        .build();
        mButtonData = new ButtonData(false, mDrawable, mOnClickListener, 0, false, null);
        mDisabledButtonData = new ButtonData(false, null, null, 0, false, null);
        doReturn(mButtonData)
                .when(mIdentityDiscController)
                .getForStartSurface(OverviewModeState.SHOWN_HOMEPAGE);
        doReturn(mDisabledButtonData)
                .when(mIdentityDiscController)
                .getForStartSurface(not(eq(OverviewModeState.SHOWN_HOMEPAGE)));

        mMockConstantState = mock(Drawable.ConstantState.class);
        doReturn(mMockConstantState).when(mDrawable).getConstantState();
        doReturn(mDrawable).when(mMockConstantState).newDrawable();

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();

        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        doReturn(mIncognitoTabModel).when(mTabModelSelector).getModel(true);
        doReturn(mMockIncognitoTab).when(mIncognitoTabModel).getTabAt(0);
        doReturn(false).when(mMockIncognitoTab).isClosing();
    }

    @After
    public void tearDown() {
    }

    @Test
    public void showAndHide() {
        createMediator(false);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);

        mMediator.setStartSurfaceMode(true);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), true);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(BUTTONS_CLICKABLE), false);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(MENU_IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);

        mMediator.setStartSurfaceToolbarVisibility(false);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), true);
        assertEquals(mPropertyModel.get(IS_VISIBLE), false);

        mMediator.setStartSurfaceToolbarVisibility(true);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), true);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);

        mMediator.setStartSurfaceMode(false);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(BUTTONS_CLICKABLE), false);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(MENU_IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
    }

    @Test
    public void showAndHideSetClickable() {
        createMediator(false);
        mMediator.setStartSurfaceMode(true);
        assertEquals(mPropertyModel.get(BUTTONS_CLICKABLE), false);

        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedShowing();
        assertEquals(mPropertyModel.get(BUTTONS_CLICKABLE), true);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedHiding(true, false);
        assertEquals(mPropertyModel.get(BUTTONS_CLICKABLE), false);
    }

    @Test
    public void showAndHideHomePage() {
        createMediator(false);
        mMediator.setTabModelSelector(mTabModelSelector);

        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);

        mMediator.setStartSurfaceMode(true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), true);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedShowing();
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), true);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);
    }

    @Test
    public void showAndHideHomePageNoIncognitoTabs() {
        createMediator(true);
        mMediator.setTabModelSelector(mTabModelSelector);
        doReturn(0).when(mIncognitoTabModel).getCount();

        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);

        mMediator.setStartSurfaceMode(true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), true);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedShowing();
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), true);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedHiding(true, false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedHiding();
        doReturn(1).when(mIncognitoTabModel).getCount();

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedShowing();
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), true);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);
    }

    @Test
    public void showHomePageWithLogo() {
        createMediator(false);
        verify(mOverviewModeBehavior)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());

        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);

        mMediator.setStartSurfaceMode(true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedShowing();
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), true);
    }

    @Test
    public void enableDisableLogo() {
        createMediator(false);
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        mMediator.setStartSurfaceMode(true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);

        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mTemplateUrlServiceObserver.getValue().onTemplateURLServiceChanged();
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), true);

        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mTemplateUrlServiceObserver.getValue().onTemplateURLServiceChanged();
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
    }

    @Test
    public void showHomePageWithIdentityDisc() {
        createMediator(false);
        mMediator.setTabModelSelector(mTabModelSelector);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);

        mMediator.setStartSurfaceMode(true);
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);

        mButtonData.contentDescriptionResId = 5;
        mButtonData.canShow = true;
        mButtonData.drawable = mDrawable;
        mMediator.updateIdentityDisc(mButtonData);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_CLICK_HANDLER), mOnClickListener);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_DESCRIPTION), 5);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IMAGE), mDrawable);

        Drawable testDrawable2 = mock(Drawable.class);
        doReturn(mMockConstantState).when(testDrawable2).getConstantState();
        doReturn(testDrawable2).when(mMockConstantState).newDrawable();
        mButtonData.drawable = testDrawable2;
        mMediator.updateIdentityDisc(mButtonData);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IMAGE), testDrawable2);

        mButtonData.canShow = false;
        mMediator.updateIdentityDisc(mButtonData);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
    }

    @Test
    public void hideIdentityDiscInIncognito() {
        createMediator(false);
        mMediator.setTabModelSelector(mTabModelSelector);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserver.capture());

        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);

        mMediator.setStartSurfaceMode(true);
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);

        mButtonData.canShow = true;
        mMediator.updateIdentityDisc(mButtonData);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), true);

        doReturn(true).when(mTabModelSelector).isIncognitoSelected();
        mTabModelSelectorObserver.getValue().onTabModelSelected(
                mock(TabModel.class), mock(TabModel.class));
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
    }

    @Test
    public void showIPHOnIdentityDisc() {
        createMediator(false);
        mMediator.setTabModelSelector(mTabModelSelector);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);

        mMediator.setStartSurfaceMode(true);
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        mButtonData.canShow = true;
        mButtonData.iphCommandBuilder = new IPHCommandBuilder(mMockResources, "IdentityDisc", 0, 0)
                                                .setOnDismissCallback(mDismissedCallback);
        mMediator.updateIdentityDisc(mButtonData);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), true);

        verify(mMockCallback, times(1)).onResult(mButtonData.iphCommandBuilder);
    }

    @Test
    public void showTabSwitcher() {
        createMediator(false);
        mMediator.setTabModelSelector(mTabModelSelector);
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());

        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), false);

        mMediator.setStartSurfaceMode(true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), true);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedShowing();
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_TABSWITCHER, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);

        mMediator.updateIdentityDisc(mButtonData);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
    }

    @Test
    public void showTabSwitcherNoIncognitoTabs() {
        createMediator(true);
        mMediator.setTabModelSelector(mTabModelSelector);
        doReturn(0).when(mIncognitoTabModel).getCount();
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());

        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), false);

        mMediator.setStartSurfaceMode(true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), true);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedShowing();
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_TABSWITCHER, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);

        mMediator.updateIdentityDisc(mButtonData);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedHiding(true, false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedHiding();
        doReturn(1).when(mIncognitoTabModel).getCount();

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedShowing();
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_TABSWITCHER, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);
    }

    @Test
    public void homePageToTabswitcher() {
        createMediator(false);
        mMediator.setTabModelSelector(mTabModelSelector);

        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        mButtonData.canShow = true;
        mMediator.updateIdentityDisc(mButtonData);
        mMediator.setStartSurfaceMode(true);
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), true);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_TABSWITCHER, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), true);
    }

    @Test
    public void showTabswitcherTasksOnly() {
        createMediator(false);
        mMediator.setTabModelSelector(mTabModelSelector);
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());

        mMediator.setStartSurfaceMode(true);
        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedShowing();
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_TABSWITCHER_TASKS_ONLY, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), true);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), true);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), true);

        mMediator.setStartSurfaceMode(false);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), false);
    }

    @Test
    public void showTabswitcherOmniboxOnlyNoIncognitoTabs() {
        createMediator(true);
        mMediator.setTabModelSelector(mTabModelSelector);
        doReturn(0).when(mIncognitoTabModel).getCount();
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());

        mMediator.setStartSurfaceMode(true);
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_TABSWITCHER_OMNIBOX_ONLY, true);
        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(false);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), true);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), true);

        mMediator.setStartSurfaceMode(false);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), false);
    }

    @Test
    public void testIdentityDiscStateChanges() {
        createMediator(false);
        mMediator.setTabModelSelector(mTabModelSelector);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);

        mMediator.setStartSurfaceMode(true);
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);

        mButtonData.canShow = true;
        mMediator.identityDiscStateChanged(true);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), true);

        mButtonData.canShow = false;
        mMediator.identityDiscStateChanged(false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);

        // updateIdentityDisc() should properly handle a hint that contradicts the true value of
        // canShow.
        mButtonData.canShow = false;
        mMediator.identityDiscStateChanged(true);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
    }

    @Test
    public void testShowAndHideHomePageWithNewTabAndIdentityDiscAtStart() {
        createMediator(false, true);
        mMediator.setTabModelSelector(mTabModelSelector);
        doReturn(0).when(mIncognitoTabModel).getCount();

        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_AT_START), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);

        mMediator.setStartSurfaceMode(true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_AT_START), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), true);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedShowing();
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_AT_START), true);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), true);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), true);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedHiding(true, false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedHiding();
        doReturn(1).when(mIncognitoTabModel).getCount();

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedShowing();
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_HOMEPAGE, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_AT_START), true);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), true);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), true);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);
    }

    @Test
    public void testShowAndHideTabSwitcherWithNewTabAndIdentityDiscAtStart() {
        createMediator(false, true);
        mMediator.setTabModelSelector(mTabModelSelector);
        doReturn(0).when(mIncognitoTabModel).getCount();
        mMediator.onNativeLibraryReady();
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserver.capture());

        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_AT_START), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), false);

        mMediator.setStartSurfaceMode(true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_AT_START), false);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), false);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);
        assertEquals(mPropertyModel.get(IN_START_SURFACE_MODE), true);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedShowing();
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_TABSWITCHER, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_AT_START), true);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), true);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);

        mMediator.updateIdentityDisc(mButtonData);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedHiding(true, false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedHiding();
        doReturn(1).when(mIncognitoTabModel).getCount();

        mOverviewModeObserverCaptor.getValue().onOverviewModeStartedShowing(false);
        mOverviewModeObserverCaptor.getValue().onOverviewModeFinishedShowing();
        mOverviewModeObserverCaptor.getValue().onOverviewModeStateChanged(
                OverviewModeState.SHOWN_TABSWITCHER, true);
        assertEquals(mPropertyModel.get(LOGO_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_IS_VISIBLE), false);
        assertEquals(mPropertyModel.get(IDENTITY_DISC_AT_START), true);
        assertEquals(mPropertyModel.get(INCOGNITO_SWITCHER_VISIBLE), false);
        assertEquals(mPropertyModel.get(NEW_TAB_BUTTON_AT_START), true);
        assertEquals(mPropertyModel.get(IS_VISIBLE), true);
    }

    private void createMediator(boolean hideIncognitoSwitchWhenNoTabs) {
        createMediator(hideIncognitoSwitchWhenNoTabs, false);
    }

    private void createMediator(
            boolean hideIncognitoSwitchWhenNoTabs, boolean showNewTabAndIdentityDiscAtStart) {
        mMediator = new StartSurfaceToolbarMediator(mPropertyModel, mIdentityDiscController,
                mMockCallback, hideIncognitoSwitchWhenNoTabs, showNewTabAndIdentityDiscAtStart);
        mMediator.setOverviewModeBehavior(mOverviewModeBehavior);
        verify(mOverviewModeBehavior)
                .addOverviewModeObserver(mOverviewModeObserverCaptor.capture());
        if (showNewTabAndIdentityDiscAtStart) {
            mPropertyModel.set(INCOGNITO_SWITCHER_VISIBLE, false);
        }
    }
}

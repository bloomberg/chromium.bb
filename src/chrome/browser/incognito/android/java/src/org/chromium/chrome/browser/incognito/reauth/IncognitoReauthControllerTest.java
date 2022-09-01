// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;

/**
 * Unit tests for {@link IncognitoReauthController}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class IncognitoReauthControllerTest {
    @Mock
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcherMock;
    @Mock
    private LayoutStateProvider mLayoutStateProviderMock;
    @Mock
    private TabModelSelector mTabModelSelectorMock;
    @Mock
    private TabModel mIncognitoTabModelMock;
    @Mock
    private TabModel mRegularTabModelMock;
    @Mock
    private Profile mProfileMock;
    @Mock
    private IncognitoReauthCoordinatorFactory mIncognitoReauthCoordinatorFactoryMock;
    @Mock
    private IncognitoReauthCoordinator mIncognitoReauthCoordinatorMock;

    @Captor
    ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;
    @Captor
    ArgumentCaptor<IncognitoTabModelObserver> mIncognitoTabModelObserverCaptor;

    private IncognitoReauthController mIncognitoReauthController;
    private OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderOneshotSupplier;
    private ObservableSupplierImpl<Profile> mProfileObservableSupplier;

    private void switchToIncognitoTabModel() {
        doReturn(true).when(mTabModelSelectorMock).isIncognitoSelected();
        mIncognitoReauthController.onAfterTabModelSelected(
                /*newModel=*/mIncognitoTabModelMock, /*oldModel=*/mRegularTabModelMock);
    }

    private void switchToRegularTabModel() {
        doReturn(false).when(mTabModelSelectorMock).isIncognitoSelected();
        mIncognitoReauthController.onAfterTabModelSelected(
                /*newModel=*/mRegularTabModelMock, /*oldModel=*/mIncognitoTabModelMock);
    }

    private Tab prepareTabForRestoreOrLauncherShortcut(boolean isRestore) {
        TabImpl tab = mock(TabImpl.class);
        doReturn(true).when(tab).isInitialized();
        UserDataHost userDataHost = new UserDataHost();
        CriticalPersistedTabData criticalPersistedTabData = mock(CriticalPersistedTabData.class);
        userDataHost.setUserData(CriticalPersistedTabData.class, criticalPersistedTabData);
        doReturn(userDataHost).when(tab).getUserDataHost();
        doReturn(0).when(criticalPersistedTabData).getRootId();
        @TabLaunchType
        Integer launchType =
                isRestore ? TabLaunchType.FROM_RESTORE : TabLaunchType.FROM_LAUNCHER_SHORTCUT;
        doReturn(launchType).when(criticalPersistedTabData).getTabLaunchTypeAtCreation();
        return tab;
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(false).when(mTabModelSelectorMock).isTabStateInitialized();
        doReturn(false).when(mTabModelSelectorMock).isIncognitoSelected();

        doNothing()
                .when(mTabModelSelectorMock)
                .addObserver(mTabModelSelectorObserverCaptor.capture());
        doNothing()
                .when(mTabModelSelectorMock)
                .addIncognitoTabModelObserver(mIncognitoTabModelObserverCaptor.capture());
        doNothing().when(mTabModelSelectorMock).setIncognitoReauthDialogDelegate(any());
        doNothing().when(mActivityLifecycleDispatcherMock).register(any());

        doReturn(mIncognitoTabModelMock).when(mTabModelSelectorMock).getModel(/*incognito=*/true);
        doReturn(0).when(mIncognitoTabModelMock).getCount();
        doReturn(true).when(mIncognitoTabModelMock).isIncognito();
        doReturn(false).when(mRegularTabModelMock).isIncognito();
        doReturn(false).when(mLayoutStateProviderMock).isLayoutVisible(LayoutType.TAB_SWITCHER);
        doReturn(mIncognitoReauthCoordinatorMock)
                .when(mIncognitoReauthCoordinatorFactoryMock)
                .createIncognitoReauthCoordinator(any(), /*showFullScreen=*/anyBoolean());
        doNothing().when(mIncognitoReauthCoordinatorMock).showDialog();

        mLayoutStateProviderOneshotSupplier = new OneshotSupplierImpl<>();
        mLayoutStateProviderOneshotSupplier.set(mLayoutStateProviderMock);
        mProfileObservableSupplier = new ObservableSupplierImpl<>();
        IncognitoReauthManager.setIsIncognitoReauthFeatureAvailableForTesting(true);

        mIncognitoReauthController = new IncognitoReauthController(mTabModelSelectorMock,
                mActivityLifecycleDispatcherMock, mLayoutStateProviderOneshotSupplier,
                mProfileObservableSupplier, mIncognitoReauthCoordinatorFactoryMock);
        mProfileObservableSupplier.set(mProfileMock);
    }

    @After
    public void tearDown() {
        doNothing().when(mActivityLifecycleDispatcherMock).unregister(any());
        doNothing()
                .when(mTabModelSelectorMock)
                .removeObserver(mTabModelSelectorObserverCaptor.capture());
        doNothing()
                .when(mTabModelSelectorMock)
                .removeIncognitoTabModelObserver(mIncognitoTabModelObserverCaptor.capture());
        mIncognitoReauthController.destroy();

        verify(mActivityLifecycleDispatcherMock, times(1)).unregister(any());
        verify(mTabModelSelectorMock, times(1))
                .removeObserver(mTabModelSelectorObserverCaptor.capture());
        verify(mTabModelSelectorMock, times(1))
                .removeIncognitoTabModelObserver(mIncognitoTabModelObserverCaptor.capture());
    }

    /**
     * This tests that we don't show a re-auth for freshly created Incognito tabs where Chrome has
     * not been backgrounded yet.
     */
    @Test
    @MediumTest
    public void testIncognitoTabsCreated_BeforeBackground_DoesNotShowReauth() {
        // Pretend there's one incognito tab.
        doReturn(1).when(mIncognitoTabModelMock).getCount();
        switchToIncognitoTabModel();

        assertFalse("IncognitoReauthCoordinator should not be created for fresh Incognito"
                        + " session when Chrome has not been backgrounded yet.",
                mIncognitoReauthController.isReauthPageShowing());
    }

    /**
     *  This tests that we do show a re-auth when Incognito tabs already exists after Chrome comes
     * to foreground.
     */
    @Test
    @MediumTest
    public void testIncognitoTabsCreated_BeforeForeground_ShowsReauth() {
        // Pretend there's one incognito tab.
        doReturn(1).when(mIncognitoTabModelMock).getCount();
        switchToIncognitoTabModel();

        mIncognitoReauthController.onStopWithNative();
        mIncognitoReauthController.onStartWithNative();

        assertTrue("IncognitoReauthCoordinator should be created when Incognito tabs"
                        + " exists already after coming to foreground.",
                mIncognitoReauthController.isReauthPageShowing());
        verify(mIncognitoReauthCoordinatorMock).showDialog();
    }

    @Test
    @MediumTest
    public void testRegularTabModel_DoesNotShowReauth() {
        switchToRegularTabModel();
        mIncognitoReauthController.onStopWithNative();
        mIncognitoReauthController.onStartWithNative();

        assertFalse("IncognitoReauthCoordinator should not be created on regular"
                        + " TabModel.",
                mIncognitoReauthController.isReauthPageShowing());
    }

    @Test
    @MediumTest
    public void testIncognitoTabsExisting_AndChromeForegroundedWithRegularTabs_DoesNotShowReauth() {
        doReturn(1).when(mIncognitoTabModelMock).getCount();
        doReturn(false).when(mTabModelSelectorMock).isIncognitoSelected();
        mIncognitoReauthController.onStopWithNative();
        mIncognitoReauthController.onStartWithNative();

        assertFalse("IncognitoReauthCoordinator should not be created on regular"
                        + " TabModel.",
                mIncognitoReauthController.isReauthPageShowing());
    }

    @Test
    @MediumTest
    public void testWhenTabModelChangesToRegularFromIncognito_HidesReauth() {
        // Pretend there's one incognito tab.
        doReturn(1).when(mIncognitoTabModelMock).getCount();
        switchToIncognitoTabModel();
        assertFalse("IncognitoReauthCoordinator should not be created if Chrome has not"
                        + " been to background.",
                mIncognitoReauthController.isReauthPageShowing());

        // Chrome went to background.
        mIncognitoReauthController.onStopWithNative();
        // Chrome coming to foregrounded. Re-auth would now be required since there are existing
        // Incognito tabs.
        mIncognitoReauthController.onStartWithNative();
        assertTrue("IncognitoReauthCoordinator should have been created.",
                mIncognitoReauthController.isReauthPageShowing());
        verify(mIncognitoReauthCoordinatorMock).showDialog();

        switchToRegularTabModel();
        assertFalse("IncognitoReauthCoordinator should have been destroyed"
                        + "when a user switches to regular TabModel.",
                mIncognitoReauthController.isReauthPageShowing());
    }

    @Test
    @MediumTest
    public void testIncognitoTabsRestore_ShowsReauth() {
        // Pretend there's one incognito tab.
        doReturn(1).when(mIncognitoTabModelMock).getCount();
        switchToIncognitoTabModel();
        Tab incognitoTab = prepareTabForRestoreOrLauncherShortcut(/*isRestore=*/true);
        doReturn(true).when(incognitoTab).isIncognito();
        doReturn(incognitoTab).when(mIncognitoTabModelMock).getTabAt(0);

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        assertTrue("IncognitoReauthCoordinator should be created for restored"
                        + " Incognito tabs.",
                mIncognitoReauthController.isReauthPageShowing());
        verify(mIncognitoReauthCoordinatorMock).showDialog();
    }

    @Test
    @MediumTest
    public void testIncognitoTabsFromLauncherShortcut_DoesNotShowReauth() {
        // Pretend there's one incognito tab.
        doReturn(1).when(mIncognitoTabModelMock).getCount();
        switchToIncognitoTabModel();
        Tab incognitoTab = prepareTabForRestoreOrLauncherShortcut(/*isRestore=*/false);
        doReturn(true).when(incognitoTab).isIncognito();
        doReturn(incognitoTab).when(mIncognitoTabModelMock).getTabAt(0);

        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
        assertFalse("IncognitoReauthCoordinator should not be created for Incognito tabs"
                        + " opened from launcher.",
                mIncognitoReauthController.isReauthPageShowing());
    }

    @Test
    @MediumTest
    public void testNewIncognitoSession_AfterClosingIncognitoTabs_DoesNotShowReauth() {
        // Pretend there's one incognito tab.
        doReturn(1).when(mIncognitoTabModelMock).getCount();
        switchToIncognitoTabModel();
        assertFalse("IncognitoReauthCoordinator should not be created if Chrome has not"
                        + " been to background.",
                mIncognitoReauthController.isReauthPageShowing());

        // Chrome went to background.
        mIncognitoReauthController.onStopWithNative();
        // Chrome coming to foregrounded. Re-auth would now be required since there are existing
        // Incognito tabs.
        doReturn(true).when(mTabModelSelectorMock).isIncognitoSelected();
        mIncognitoReauthController.onStartWithNative();
        switchToIncognitoTabModel();
        assertTrue("IncognitoReauthCoordinator should be created when all conditions are"
                        + " met.",
                mIncognitoReauthController.isReauthPageShowing());
        verify(mIncognitoReauthCoordinatorMock).showDialog();

        // Move to regular mode.
        switchToRegularTabModel();
        // close all Incognito tabs.
        mIncognitoTabModelObserverCaptor.getValue().didBecomeEmpty();
        // Open an Incognito tab.
        doReturn(1).when(mIncognitoTabModelMock).getCount();
        switchToIncognitoTabModel();
        assertFalse("IncognitoReauthCoordinator should not be created when starting a"
                        + " fresh Incognito session.",
                mIncognitoReauthController.isReauthPageShowing());
    }
}

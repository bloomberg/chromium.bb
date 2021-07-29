// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/**
 * Tests for {@link MerchantTrustSignalsMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MerchantTrustSignalsMediatorTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private MerchantTrustSignalsMediator.MerchantTrustSignalsCallback mMockDelegate;

    @Mock
    private TabImpl mMockTab;

    @Mock
    private ObservableSupplier<Tab> mMockTabProvider;

    @Mock
    private WebContents mMockWebContents;

    @Mock
    private NavigationHandle mMockNavigationHandle;

    @Mock
    private GURL mMockUrl;

    @Captor
    private ArgumentCaptor<Callback<Tab>> mTabSupplierCallbackCaptor;

    @Captor
    private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private MerchantTrustSignalsMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mMockWebContents).when(mMockTab).getWebContents();
        doReturn(false).when(mMockTab).isIncognito();
        doReturn(true).when(mMockNavigationHandle).hasCommitted();
        doReturn(true).when(mMockNavigationHandle).isInPrimaryMainFrame();
        doReturn(false).when(mMockNavigationHandle).isSameDocument();
        doReturn(mMockUrl).when(mMockNavigationHandle).getUrl();
        doReturn("fake_host").when(mMockUrl).getHost();

        createMediatorAndVerify();
    }

    @After
    public void tearDown() {
        destroyAndVerify();
    }

    private void createMediatorAndVerify() {
        mMediator = new MerchantTrustSignalsMediator(mMockTabProvider, mMockDelegate);
        verify(mMockTabProvider, times(1)).addObserver(mTabSupplierCallbackCaptor.capture());
        mTabSupplierCallbackCaptor.getValue().onResult(mMockTab);
        verify(mMockTab, times(1)).addObserver(mTabObserverCaptor.capture());
    }

    private void destroyAndVerify() {
        mMediator.destroy();
        verify(mMockTab, times(1)).removeObserver(mTabObserverCaptor.getValue());
        verify(mMockTabProvider, times(1)).removeObserver(mTabSupplierCallbackCaptor.getValue());
    }

    @Test
    public void testTabObserverOnDidFinishNavigation() {
        mTabObserverCaptor.getValue().onDidFinishNavigation(mMockTab, mMockNavigationHandle);
        verify(mMockDelegate, times(1)).maybeDisplayMessage(any(MerchantTrustMessageContext.class));
    }

    @Test
    public void testTabObserverOnDidFinishNavigation_IncognitoTab() {
        doReturn(true).when(mMockTab).isIncognito();

        mTabObserverCaptor.getValue().onDidFinishNavigation(mMockTab, mMockNavigationHandle);
        verify(mMockDelegate, never()).maybeDisplayMessage(any(MerchantTrustMessageContext.class));
    }

    @Test
    public void testTabObserverOnDidFinishNavigation_NavigationNonCommit() {
        doReturn(false).when(mMockNavigationHandle).hasCommitted();

        mTabObserverCaptor.getValue().onDidFinishNavigation(mMockTab, mMockNavigationHandle);
        verify(mMockDelegate, never()).maybeDisplayMessage(any(MerchantTrustMessageContext.class));
    }

    @Test
    public void testTabObserverOnDidFinishNavigation_NonMainFrame() {
        doReturn(false).when(mMockNavigationHandle).isInPrimaryMainFrame();

        mTabObserverCaptor.getValue().onDidFinishNavigation(mMockTab, mMockNavigationHandle);
        verify(mMockDelegate, never()).maybeDisplayMessage(any(MerchantTrustMessageContext.class));
    }

    @Test
    public void testTabObserverOnDidFinishNavigation_SameDoc() {
        doReturn(true).when(mMockNavigationHandle).isSameDocument();

        mTabObserverCaptor.getValue().onDidFinishNavigation(mMockTab, mMockNavigationHandle);
        verify(mMockDelegate, never()).maybeDisplayMessage(any(MerchantTrustMessageContext.class));
    }

    @Test
    public void testTabObserverOnDidFinishNavigation_MissingHost() {
        doReturn(null).when(mMockUrl).getHost();

        mTabObserverCaptor.getValue().onDidFinishNavigation(mMockTab, mMockNavigationHandle);
        verify(mMockDelegate, never()).maybeDisplayMessage(any(MerchantTrustMessageContext.class));
    }
}

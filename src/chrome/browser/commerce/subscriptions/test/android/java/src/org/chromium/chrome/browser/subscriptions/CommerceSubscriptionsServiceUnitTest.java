// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

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
import org.chromium.base.FeatureList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.CommerceSubscriptionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.PriceTrackingUtilities;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Unit tests for {@link CommerceSubscriptionsService}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
public class CommerceSubscriptionsServiceUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private SubscriptionsManagerImpl mSubscriptionsManager;
    @Mock
    private IdentityManager mIdentityManager;
    @Mock
    private PrimaryAccountChangeEvent mChangeEvent;
    @Mock
    TabModelSelector mTabModelSelector;
    @Mock
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock
    private ImplicitPriceDropSubscriptionsManager mImplicitSubscriptionsManager;
    @Captor
    private ArgumentCaptor<IdentityManager.Observer> mIdentityManagerObserverCaptor;
    @Captor
    private ArgumentCaptor<PauseResumeWithNativeObserver> mPauseResumeWithNativeObserverCaptor;
    @Captor
    private ArgumentCaptor<Callback<List<CommerceSubscription>>> mLocalSubscriptionsCallbackCaptor;

    private CommerceSubscriptionsService mService;
    private SharedPreferencesManager mSharedPreferencesManager;
    private MockNotificationManagerProxy mMockNotificationManager;
    private FeatureList.TestValues mTestValues;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowRecordHistogram.reset();

        doNothing().when(mActivityLifecycleDispatcher).register(any());
        doNothing().when(mSubscriptionsManager).getSubscriptions(anyString(), anyBoolean(), any());
        mSharedPreferencesManager = SharedPreferencesManager.getInstance();
        mSharedPreferencesManager.writeLong(
                CommerceSubscriptionsService.CHROME_MANAGED_SUBSCRIPTIONS_TIMESTAMP,
                System.currentTimeMillis()
                        - TimeUnit.SECONDS.toMillis(
                                CommerceSubscriptionsServiceConfig.getStaleTabLowerBoundSeconds()));
        PriceTrackingUtilities.setIsSignedInAndSyncEnabledForTesting(true);

        mTestValues = new FeatureList.TestValues();
        mTestValues.addFeatureFlagOverride(ChromeFeatureList.COMMERCE_PRICE_TRACKING, true);
        mTestValues.addFieldTrialParamOverride(ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                PriceTrackingUtilities.PRICE_NOTIFICATION_PARAM, "true");
        FeatureList.setTestValues(mTestValues);

        mMockNotificationManager = new MockNotificationManagerProxy();
        mMockNotificationManager.setNotificationsEnabled(false);
        PriceDropNotificationManager.setNotificationManagerForTesting(mMockNotificationManager);

        mService = new CommerceSubscriptionsService(mSubscriptionsManager, mIdentityManager);
        verify(mIdentityManager, times(1)).addObserver(mIdentityManagerObserverCaptor.capture());
        mService.setImplicitSubscriptionsManagerForTesting(mImplicitSubscriptionsManager);
    }

    @After
    public void tearDown() {
        PriceDropNotificationManager.setNotificationManagerForTesting(null);
    }

    @Test
    @SmallTest
    public void testDestroy() {
        mService.setImplicitSubscriptionsManagerForTesting(null);
        mService.destroy();
        verify(mIdentityManager, times(1))
                .removeObserver(eq(mIdentityManagerObserverCaptor.getValue()));
    }

    @Test
    @SmallTest
    public void testInitDeferredStartupForActivity() {
        mService.initDeferredStartupForActivity(mTabModelSelector, mActivityLifecycleDispatcher);
        verify(mActivityLifecycleDispatcher, times(1))
                .register(mPauseResumeWithNativeObserverCaptor.capture());

        mService.destroy();
        verify(mIdentityManager, times(1))
                .removeObserver(eq(mIdentityManagerObserverCaptor.getValue()));
        verify(mActivityLifecycleDispatcher, times(1))
                .unregister(eq(mPauseResumeWithNativeObserverCaptor.getValue()));
        verify(mImplicitSubscriptionsManager, times(1)).destroy();
    }

    @Test
    @SmallTest
    public void testOnPrimaryAccountChanged() {
        mIdentityManagerObserverCaptor.getValue().onPrimaryAccountChanged(mChangeEvent);
        verify(mSubscriptionsManager, times(1)).onIdentityChanged();
    }

    @Test
    @SmallTest
    public void testOnResume() {
        setupTestOnResume();
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           PriceDropNotificationManager.NOTIFICATION_ENABLED_HISTOGRAM),
                equalTo(1));
        assertThat(
                RecordHistogram.getHistogramTotalCountForTesting(
                        PriceDropNotificationManager.NOTIFICATION_CHROME_MANAGED_COUNT_HISTOGRAM),
                equalTo(1));
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           PriceDropNotificationManager.NOTIFICATION_USER_MANAGED_COUNT_HISTOGRAM),
                equalTo(1));
        verify(mSubscriptionsManager, times(1))
                .getSubscriptions(eq(CommerceSubscriptionType.PRICE_TRACK), eq(false),
                        mLocalSubscriptionsCallbackCaptor.capture());
        verify(mImplicitSubscriptionsManager, times(1)).initializeSubscriptions();

        CommerceSubscription subscription1 = new CommerceSubscription(
                CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK, "offer_id_1",
                CommerceSubscription.SubscriptionManagementType.CHROME_MANAGED,
                CommerceSubscription.TrackingIdType.OFFER_ID);
        CommerceSubscription subscription2 =
                new CommerceSubscription(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK,
                        "offer_id_2", CommerceSubscription.SubscriptionManagementType.USER_MANAGED,
                        CommerceSubscription.TrackingIdType.PRODUCT_CLUSTER_ID);

        mLocalSubscriptionsCallbackCaptor.getValue().onResult(
                new ArrayList<>(Arrays.asList(subscription1, subscription1, subscription2)));
        assertThat(
                RecordHistogram.getHistogramTotalCountForTesting(
                        CommerceSubscriptionsMetrics.SUBSCRIPTION_CHROME_MANAGED_COUNT_HISTOGRAM),
                equalTo(1));
        assertThat(RecordHistogram.getHistogramValueCountForTesting(
                           CommerceSubscriptionsMetrics.SUBSCRIPTION_CHROME_MANAGED_COUNT_HISTOGRAM,
                           2),
                equalTo(1));
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           CommerceSubscriptionsMetrics.SUBSCRIPTION_USER_MANAGED_COUNT_HISTOGRAM),
                equalTo(1));
        assertThat(
                RecordHistogram.getHistogramValueCountForTesting(
                        CommerceSubscriptionsMetrics.SUBSCRIPTION_USER_MANAGED_COUNT_HISTOGRAM, 1),
                equalTo(1));
    }

    @Test
    @SmallTest
    public void testOnResume_FeatureDisabled() {
        mTestValues.addFieldTrialParamOverride(ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                PriceTrackingUtilities.PRICE_NOTIFICATION_PARAM, "false");
        FeatureList.setTestValues(mTestValues);

        setupTestOnResume();
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           PriceDropNotificationManager.NOTIFICATION_ENABLED_HISTOGRAM),
                equalTo(0));
        verify(mSubscriptionsManager, times(0)).getSubscriptions(anyString(), anyBoolean(), any());
        verify(mImplicitSubscriptionsManager, times(0)).initializeSubscriptions();
    }

    @Test
    @SmallTest
    public void testOnResume_TooFrequent() {
        mSharedPreferencesManager.writeLong(
                CommerceSubscriptionsService.CHROME_MANAGED_SUBSCRIPTIONS_TIMESTAMP,
                System.currentTimeMillis());

        setupTestOnResume();
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           PriceDropNotificationManager.NOTIFICATION_ENABLED_HISTOGRAM),
                equalTo(0));
        verify(mSubscriptionsManager, times(0)).getSubscriptions(anyString(), anyBoolean(), any());
        verify(mImplicitSubscriptionsManager, times(0)).initializeSubscriptions();
    }

    private void setupTestOnResume() {
        mService.initDeferredStartupForActivity(mTabModelSelector, mActivityLifecycleDispatcher);
        verify(mActivityLifecycleDispatcher, times(1))
                .register(mPauseResumeWithNativeObserverCaptor.capture());
        mPauseResumeWithNativeObserverCaptor.getValue().onResumeWithNative();
    }
}

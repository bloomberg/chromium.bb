// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.Mockito.doReturn;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeBrowserTestRule;

import java.util.concurrent.TimeUnit;

/**
 * Test relating to {@link PriceDropsMetricsLogger}
 */
@RunWith(BaseJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class PriceDropMetricsLoggerTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Mock
    private ShoppingPersistedTabData mShoppingPersistedTabData;

    private PriceDropMetricsLogger mPriceDropMetricsLogger;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn("offer-id").when(mShoppingPersistedTabData).getMainOfferId();
        doReturn(true).when(mShoppingPersistedTabData).hasPriceMicros();
        doReturn(true).when(mShoppingPersistedTabData).hasPreviousPriceMicros();
        mPriceDropMetricsLogger = new PriceDropMetricsLogger(mShoppingPersistedTabData);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testTabUsageStatus() {
        Assert.assertEquals(PriceDropMetricsLogger.TabUsageStatus.ABANDONED,
                PriceDropMetricsLogger.getTabUsageStatus(TimeUnit.DAYS.toMillis(100)));
        Assert.assertEquals(PriceDropMetricsLogger.TabUsageStatus.ABANDONED,
                PriceDropMetricsLogger.getTabUsageStatus(TimeUnit.DAYS.toMillis(90)));
        Assert.assertEquals(PriceDropMetricsLogger.TabUsageStatus.STALE,
                PriceDropMetricsLogger.getTabUsageStatus(TimeUnit.DAYS.toMillis(45)));
        Assert.assertEquals(PriceDropMetricsLogger.TabUsageStatus.STALE,
                PriceDropMetricsLogger.getTabUsageStatus(TimeUnit.DAYS.toMillis(1)));
        Assert.assertEquals(PriceDropMetricsLogger.TabUsageStatus.ACTIVE,
                PriceDropMetricsLogger.getTabUsageStatus(TimeUnit.HOURS.toMillis(12)));
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testMetricsStaleTabNavigation() {
        mPriceDropMetricsLogger.logPriceDropMetrics(
                "NavigationComplete", TimeUnit.DAYS.toMillis(45));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Commerce.PriceDrops.StaleTabNavigationComplete.IsProductDetailPage"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Commerce.PriceDrops.StaleTabNavigationComplete.ContainsPrice"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Commerce.PriceDrops.StaleTabNavigationComplete.ContainsPriceDrop"));
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testMetrics2StaleTabEnterTabSwitcher() {
        mPriceDropMetricsLogger.logPriceDropMetrics("EnterTabSwitcher", TimeUnit.DAYS.toMillis(45));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Commerce.PriceDrops.StaleTabEnterTabSwitcher.IsProductDetailPage"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Commerce.PriceDrops.StaleTabEnterTabSwitcher.ContainsPrice"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Commerce.PriceDrops.StaleTabEnterTabSwitcher.ContainsPriceDrop"));
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testMetricsActiveTabNavigationComplete() {
        mPriceDropMetricsLogger.logPriceDropMetrics(
                "NavigationComplete", TimeUnit.HOURS.toMillis(12));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Commerce.PriceDrops.ActiveTabNavigationComplete.IsProductDetailPage"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Commerce.PriceDrops.ActiveTabNavigationComplete.ContainsPrice"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Commerce.PriceDrops.ActiveTabNavigationComplete.ContainsPriceDrop"));
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testMetricsActiveTabEnterTabSwitcher() {
        mPriceDropMetricsLogger.logPriceDropMetrics(
                "EnterTabSwitcher", TimeUnit.HOURS.toMillis(12));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.IsProductDetailPage"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.ContainsPrice"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.ContainsPriceDrop"));
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testMetricsPriceNoPriceDrop() {
        mPriceDropMetricsLogger.logPriceDropMetrics(
                "EnterTabSwitcher", TimeUnit.HOURS.toMillis(12));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.IsProductDetailPage"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.ContainsPrice"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Commerce.PriceDrops.ActiveTabEnterTabSwitcher.ContainsPriceDrop"));
    }
}

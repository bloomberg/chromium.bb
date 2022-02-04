// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics.LaunchCause;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.components.webapps.ShortcutSource;
import org.chromium.components.webapps.WebApkDistributor;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/**
 * Tests basic functionality of WebappLaunchCauseMetrics.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public final class WebappLaunchCauseMetricsTest {
    @Mock
    private Activity mActivity;
    @Mock
    private WebappInfo mWebappInfo;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        });
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ApplicationStatus.resetActivitiesForInstrumentationTests();
            LaunchCauseMetrics.resetForTests();
        });
    }

    private static int histogramCountForValue(int value) {
        return RecordHistogram.getHistogramValueCountForTesting(
                LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM, value);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testHomescreenLaunch() throws Throwable {
        int otherCount = histogramCountForValue(LaunchCause.WEBAPK_OTHER_DISTRIBUTOR);
        int chromeCount = histogramCountForValue(LaunchCause.WEBAPK_CHROME_DISTRIBUTOR);
        Mockito.when(mWebappInfo.isLaunchedFromHomescreen()).thenReturn(true);
        Mockito.when(mWebappInfo.isForWebApk()).thenReturn(true);
        Mockito.when(mWebappInfo.distributor()).thenReturn(WebApkDistributor.BROWSER);

        WebappLaunchCauseMetrics metrics = new WebappLaunchCauseMetrics(mActivity, mWebappInfo);

        metrics.onReceivedIntent();
        metrics.recordLaunchCause();
        ++chromeCount;
        Assert.assertEquals(
                chromeCount, histogramCountForValue(LaunchCause.WEBAPK_CHROME_DISTRIBUTOR));

        LaunchCauseMetrics.resetForTests();

        Mockito.when(mWebappInfo.distributor()).thenReturn(WebApkDistributor.OTHER);
        metrics.onReceivedIntent();
        metrics.recordLaunchCause();
        ++otherCount;
        Assert.assertEquals(
                chromeCount, histogramCountForValue(LaunchCause.WEBAPK_OTHER_DISTRIBUTOR));

        LaunchCauseMetrics.resetForTests();

        Mockito.when(mWebappInfo.isForWebApk()).thenReturn(false);
        metrics.onReceivedIntent();
        metrics.recordLaunchCause();
        ++chromeCount;
        Assert.assertEquals(
                chromeCount, histogramCountForValue(LaunchCause.WEBAPK_CHROME_DISTRIBUTOR));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisabledTest(message = "https://crbug.com/1287572")
    public void testViewIntentLaunch() throws Throwable {
        int count = histogramCountForValue(LaunchCause.EXTERNAL_VIEW_INTENT);
        Mockito.when(mWebappInfo.isLaunchedFromHomescreen()).thenReturn(false);
        Mockito.when(mWebappInfo.source()).thenReturn(ShortcutSource.EXTERNAL_INTENT);

        WebappLaunchCauseMetrics metrics = new WebappLaunchCauseMetrics(mActivity, mWebappInfo);

        metrics.onReceivedIntent();
        metrics.recordLaunchCause();
        ++count;
        Assert.assertEquals(count, histogramCountForValue(LaunchCause.EXTERNAL_VIEW_INTENT));
    }
}

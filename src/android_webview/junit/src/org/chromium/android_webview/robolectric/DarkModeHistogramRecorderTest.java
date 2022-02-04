// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.DarkModeHelper;
import org.chromium.android_webview.DarkModeHistogramRecorder;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Junit tests for DarkModeHistogrmRecorder.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
public class DarkModeHistogramRecorderTest {
    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRecordDarkModeMetrics() {
        // The hardcode numbers can't be changed, they are written to histogram.
        DarkModeHistogramRecorder.recordDarkModeMetrics(DarkModeHelper.NightMode.NIGHT_MODE_ON,
                DarkModeHelper.LightTheme.LIGHT_THEME_TRUE, /*isDarkMode=*/true,
                AwSettings.FORCE_DARK_ON, AwSettings.MEDIA_QUERY_ONLY,
                DarkModeHelper.TextLuminance.TEXT_LUMINACE_LIGHT);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.WebView.DarkMode.ForceDarkBehavior"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.DarkMode.ForceDarkBehavior", 1 /*MEDIA_QUERY_ONLY*/));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.WebView.DarkMode.ForceDarkMode"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.DarkMode.ForceDarkMode", 2 /*FORCE_DARK_ON*/));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.WebView.DarkMode.InDarkMode"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.DarkMode.InDarkMode", 1 /*isDarkMode=true*/));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.WebView.DarkMode.InDarkModeVsLightTheme"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.DarkMode.InDarkModeVsLightTheme",
                        2 /*isDarkMode=true && LIGHT_THEME_TRUE*/));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.WebView.DarkMode.InDarkModeVsNightMode"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.DarkMode.InDarkModeVsNightMode",
                        1 /*isDarkMode=true && NIGHT_MODE_ON*/));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.WebView.DarkMode.LightTheme"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.DarkMode.LightTheme", 2 /*LIGHT_THEME_TRUE*/));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.WebView.DarkMode.PrimaryTextLuminanceVsLightTheme"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.DarkMode.PrimaryTextLuminanceVsLightTheme",
                        5 /*TEXT_LUMINACE_LIGHT && LIGHT_THEME_TRUE*/));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.WebView.DarkMode.PrimaryTextLuminanceVsNightMode"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.DarkMode.PrimaryTextLuminanceVsNightMode",
                        4 /*TEXT_LUMINACE_LIGHT && NIGHT_MODE_ON*/));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.WebView.DarkMode.NightMode"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.DarkMode.NightMode", 1 /*NIGHT_MODE_ON*/));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.WebView.DarkMode.NightModeVsLightTheme"));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.DarkMode.NightModeVsLightTheme",
                        5 /*NIGHT_MODE_ON && LIGHT_THEME_TRUE*/));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRecordForceDarkModeAPIUsage() {
        Configuration mockedConfiguration = Mockito.mock(Configuration.class);
        mockedConfiguration.uiMode = Configuration.UI_MODE_NIGHT_NO;
        Resources mockedResource = Mockito.mock(Resources.class);
        when(mockedResource.getConfiguration()).thenReturn(mockedConfiguration);
        Context mockedContext = Mockito.mock(Context.class);
        when(mockedContext.getResources()).thenReturn(mockedResource);

        DarkModeHistogramRecorder.recordForceDarkModeAPIUsage(
                mockedContext, AwSettings.FORCE_DARK_AUTO);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting("Android.WebView.ForceDarkMode"));
        // The hardcode numbers can't be changed, they are written to histogram.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.ForceDarkMode", 7 /* NIGHT_MODE_OFF && FOCE_DARK_AUTO*/));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRecordForceDarkBehaviorAPIUsage() {
        DarkModeHistogramRecorder.recordForceDarkBehaviorAPIUsage(
                AwSettings.PREFER_MEDIA_QUERY_OVER_FORCE_DARK);
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Android.WebView.ForceDarkBehavior"));
        // The hardcode numbers can't be changed, they are written to histogram.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.ForceDarkBehavior",
                        2 /* PREFER_MEDIA_QUERY_OVER_FORCE_DARK*/));
    }
}

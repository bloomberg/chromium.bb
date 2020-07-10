// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.content.ContentResolver;
import android.provider.Settings;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests for {@link BrowserAccessibilityState}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
public final class BrowserAccessibilityStateTest {
    private ContentResolver mContentResolver;

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
        mContentResolver = ContextUtils.getApplicationContext().getContentResolver();
    }

    @Test
    public void recordAccessibilityHistograms_animationsDefaultValue() {
        // By default in the test environment this flag shouldn't be set.
        Assert.assertEquals(-1,
                Settings.Global.getFloat(
                        mContentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, -1),
                0.01);
        BrowserAccessibilityState.recordAccessibilityHistograms();
        assertUma(1, 0, 0);
    }

    @Test
    public void recordAccessibilityHistograms_animationsDisabled() {
        Settings.Global.putFloat(mContentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, 0);
        Assert.assertEquals(0,
                Settings.Global.getFloat(
                        mContentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, -1),
                0.01);
        BrowserAccessibilityState.recordAccessibilityHistograms();
        assertUma(0, 1, 0);
    }

    @Test
    public void recordAccessibilityHistograms_animationsEnabled() {
        // Enabled is any positive value, so try a few common ones to make sure.
        Settings.Global.putFloat(mContentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, 0.1f);
        Assert.assertEquals(0.1f,
                Settings.Global.getFloat(
                        mContentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, -1),
                0.01);
        BrowserAccessibilityState.recordAccessibilityHistograms();
        assertUma(0, 0, 1);

        Settings.Global.putFloat(mContentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, 0.5f);
        Assert.assertEquals(0.5f,
                Settings.Global.getFloat(
                        mContentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, -1),
                0.01);
        BrowserAccessibilityState.recordAccessibilityHistograms();
        assertUma(0, 0, 2);

        Settings.Global.putFloat(mContentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, 1);
        Assert.assertEquals(1,
                Settings.Global.getFloat(
                        mContentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, -1),
                0.01);
        BrowserAccessibilityState.recordAccessibilityHistograms();
        assertUma(0, 0, 3);

        Settings.Global.putFloat(mContentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, 5);
        Assert.assertEquals(5,
                Settings.Global.getFloat(
                        mContentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, -1),
                0.01);
        BrowserAccessibilityState.recordAccessibilityHistograms();
        assertUma(0, 0, 4);

        Settings.Global.putFloat(mContentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, 10);
        Assert.assertEquals(10,
                Settings.Global.getFloat(
                        mContentResolver, Settings.Global.ANIMATOR_DURATION_SCALE, -1),
                0.01);
        BrowserAccessibilityState.recordAccessibilityHistograms();
        assertUma(0, 0, 5);
    }

    private void assertUma(int defaultCount, int disabledCount, int enabledCount) {
        Assert.assertEquals("default count", defaultCount,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Accessibility.Android.AnimationsEnabled2",
                        BrowserAccessibilityState.ANIMATIONS_STATE_DEFAULT_VALUE));
        Assert.assertEquals("disabled count", disabledCount,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Accessibility.Android.AnimationsEnabled2",
                        BrowserAccessibilityState.ANIMATIONS_STATE_DISABLED));
        Assert.assertEquals("enabled count", enabledCount,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Accessibility.Android.AnimationsEnabled2",
                        BrowserAccessibilityState.ANIMATIONS_STATE_ENABLED));
    }
}

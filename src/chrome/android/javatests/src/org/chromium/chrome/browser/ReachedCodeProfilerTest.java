// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.BaseSwitches;
import org.chromium.base.test.ReachedCodeProfiler;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

/**
 * Tests for the reached code profiler feature setup.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class ReachedCodeProfilerTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    // Shared preferences key for the reached code profiler.
    private static final String REACHED_CODE_PROFILER_ENABLED_KEY = "reached_code_profiler_enabled";

    /**
     * Tests that passing a command line switch enables the reached code profiler no matter what.
     */
    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.REACHED_CODE_PROFILER)
    @CommandLineFlags.Add(BaseSwitches.ENABLE_REACHED_CODE_PROFILER)
    public void testExplicitlyEnableViaCommandLineSwitch() throws Exception {
        mActivityTestRule.startMainActivityFromLauncher();
        assertReachedCodeProfilerIsEnabled();
    }

    /**
     * Tests that setting a shared preference enables the reached code profiler. This test imitates
     * the second launch after enabling the feature
     */
    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.REACHED_CODE_PROFILER)
    public void testEnabledViaCachedSharedPreference() throws Exception {
        setReachedCodeProfilerSharedPreference(true);
        mActivityTestRule.startMainActivityFromLauncher();
        assertReachedCodeProfilerIsEnabled();
    }

    /**
     * Tests that the feature state is cached in shared preferences after native initialization.
     * This test imitates the first run when the feature is enabled.
     */
    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.REACHED_CODE_PROFILER)
    public void testSharedPreferenceIsCached_Enable() throws Exception {
        mActivityTestRule.startMainActivityFromLauncher();

        Assert.assertTrue(getReachedCodeProfilerSharedPreference());
        // Enabling takes effect only on the second startup.
        Assert.assertFalse(ReachedCodeProfiler.isEnabled());
    }

    /**
     * Tests that the feature state is cached in shared preferences after native initialization.
     * This test imitates disabling the reached code profiler after it has been enabled for some
     * time.
     */
    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.REACHED_CODE_PROFILER)
    public void testSharedPreferenceIsCached_Disable() throws Exception {
        setReachedCodeProfilerSharedPreference(true);
        mActivityTestRule.startMainActivityFromLauncher();

        Assert.assertFalse(getReachedCodeProfilerSharedPreference());
        // Disabling takes effect only on the second startup.
        assertReachedCodeProfilerIsEnabled();
    }

    /**
     * The reached code profiler is always disabled in some configurations. This helper allows to
     * check if the profiler is enabled in supported configurations.
     */
    private void assertReachedCodeProfilerIsEnabled() {
        if (!ReachedCodeProfiler.isSupported()) {
            Assert.assertFalse(ReachedCodeProfiler.isEnabled());
            return;
        }

        Assert.assertTrue(ReachedCodeProfiler.isEnabled());
    }

    private boolean getReachedCodeProfilerSharedPreference() {
        return ChromePreferenceManager.getInstance().readBoolean(
                REACHED_CODE_PROFILER_ENABLED_KEY, false);
    }

    private void setReachedCodeProfilerSharedPreference(boolean value) {
        ChromePreferenceManager.getInstance().writeBoolean(
                REACHED_CODE_PROFILER_ENABLED_KEY, value);
    }
}

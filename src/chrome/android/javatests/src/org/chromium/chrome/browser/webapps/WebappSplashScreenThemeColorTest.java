// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.graphics.Color;
import android.os.Build;
import android.support.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.ThemeTestUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Tests for splash screens with EXTRA_THEME_COLOR specified in the Intent.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebappSplashScreenThemeColorTest {
    @Rule
    public final WebappActivityTestRule mActivityTestRule = new WebappActivityTestRule();

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP_MR1)
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // Customizing status bar color is disallowed for tablets.
    @Feature({"Webapps"})
    public void testThemeColorWhenSpecified() {
        // This is Color.Magenta with 50% opacity.
        final int intentThemeColor = Color.argb(0x80, 0xFF, 0, 0xFF);
        Intent intent = mActivityTestRule.createIntent().putExtra(
                ShortcutHelper.EXTRA_THEME_COLOR, (long) intentThemeColor);
        mActivityTestRule.startWebappActivity(intent);

        final int expectedThemeColor = Color.MAGENTA;
        ThemeTestUtils.assertStatusBarColor(mActivityTestRule.getActivity(), expectedThemeColor);
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP_MR1)
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    // Customizing status bar color is disallowed for tablets.
    @Feature({"Webapps"})
    public void testThemeColorNotUsedIfPagesHasOne() throws ExecutionException, TimeoutException {
        final int intentThemeColor = Color.MAGENTA;
        final int pageThemeColor = Color.RED;
        String pageWithThemeColorUrl = mActivityTestRule.getTestServer().getURL(
                "/chrome/test/data/android/theme_color_test.html");
        Intent intent =
                mActivityTestRule.createIntent()
                        .putExtra(ShortcutHelper.EXTRA_URL, pageWithThemeColorUrl)
                        .putExtra(ShortcutHelper.EXTRA_THEME_COLOR, (long) intentThemeColor);
        mActivityTestRule.startWebappActivity(intent);

        ThemeTestUtils.waitForThemeColor(mActivityTestRule.getActivity(), pageThemeColor);
        ThemeTestUtils.assertStatusBarColor(mActivityTestRule.getActivity(), pageThemeColor);
    }
}

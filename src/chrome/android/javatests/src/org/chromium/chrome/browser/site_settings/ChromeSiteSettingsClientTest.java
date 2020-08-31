// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import static com.google.common.truth.Truth.assertThat;

import android.graphics.Bitmap;
import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.concurrent.TimeoutException;

/**
 * Tests for Chrome's SiteSettingsClient implementation.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ChromeSiteSettingsClientTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    // Tests that a fallback favicon is generated when a real one isn't found locally.
    // This is a regression test for crbug.com/1077716.
    @Test
    @SmallTest
    public void testFallbackFaviconLoads() throws TimeoutException {
        ChromeSiteSettingsClient client =
                new ChromeSiteSettingsClient(mActivityTestRule.getActivity());

        Bitmap[] holder = new Bitmap[1];
        CallbackHelper helper = new CallbackHelper();
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            client.getFaviconImageForURL("url.with.no.favicon", favicon -> {
                holder[0] = favicon;
                helper.notifyCalled();
            });
        });
        helper.waitForCallback(helper.getCallCount());

        Bitmap favicon = holder[0];
        assertThat(favicon).isNotNull();
        assertThat(favicon.getWidth()).isGreaterThan(0);
        assertThat(favicon.getHeight()).isGreaterThan(0);
    }
}

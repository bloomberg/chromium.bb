// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.content.Intent;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

/**
 * Tests for DinoActivity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DinoActivityTest {
    @Rule
    public ChromeActivityTestRule<DinoActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(DinoActivity.class);

    private EmbeddedTestServer mTestServer;
    private DinoActivity mActivity;

    @Before
    public void setUp() throws InterruptedException {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    /**
     * Tests that the Dino game is launchable.
     */
    @Test
    @MediumTest
    public void testDinoGameIsLaunchable() throws Throwable {
        Intent i = new Intent();
        i.setAction(Intent.ACTION_MAIN);
        i.addCategory(Intent.CATEGORY_LAUNCHER);
        i.setClassName(InstrumentationRegistry.getTargetContext().getPackageName(),
                NoTouchActivity.DINOSAUR_GAME_INTENT);
        mActivityTestRule.startMainActivityFromIntent(i, null);
        mActivity = mActivityTestRule.getActivity();
        Assert.assertFalse(mActivity.getActivityTab().isNativePage());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("chrome://dino/",
                    mActivity.getActivityTab().getWebContents().getLastCommittedUrl());
        });
    }

    /**
     * Tests that the Dino ACTION but with different URL loads dino.
     */
    @Test
    @MediumTest
    public void testDinoGameOnlyLoadsDino() throws Throwable {
        Intent i = new Intent();
        i.setAction(Intent.ACTION_MAIN);
        i.addCategory(Intent.CATEGORY_LAUNCHER);
        i.setClassName(InstrumentationRegistry.getTargetContext().getPackageName(),
                NoTouchActivity.DINOSAUR_GAME_INTENT);
        i.setData(Uri.parse("https://www.google.com"));
        mActivityTestRule.startMainActivityFromIntent(i, null);
        mActivity = mActivityTestRule.getActivity();
        Assert.assertFalse(mActivity.getActivityTab().isNativePage());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("chrome://dino/",
                    mActivity.getActivityTab().getWebContents().getLastCommittedUrl());
        });
    }
}

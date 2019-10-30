// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static org.junit.Assert.assertTrue;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;
import static org.chromium.weblayer.BrowsingDataType.CACHE;
import static org.chromium.weblayer.BrowsingDataType.COOKIES_AND_SITE_DATA;

import android.os.Bundle;
import android.support.test.filters.SmallTest;
import android.support.v4.app.FragmentManager;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.shell.WebLayerShellActivity;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Example test that just starts the weblayer shell.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class DataClearingTest {
    @Rule
    public WebLayerShellActivityTestRule mActivityTestRule = new WebLayerShellActivityTestRule();

    @Test
    @SmallTest
    public void clearDataWithPersistedProfile_TriggersCallback() throws InterruptedException {
        checkTriggersCallbackOnClearData(new int[] {COOKIES_AND_SITE_DATA}, "Profile");
    }

    @Test
    @SmallTest
    public void clearDataWithInMemoryProfile_TriggersCallback() throws InterruptedException {
        checkTriggersCallbackOnClearData(new int[] {COOKIES_AND_SITE_DATA}, "");
    }

    @Test
    @SmallTest
    public void clearCache_TriggersCallback() throws InterruptedException {
        checkTriggersCallbackOnClearData(new int[] {CACHE}, "Profile");
    }

    @Test
    @SmallTest
    public void clearMultipleTypes_TriggersCallback() throws InterruptedException {
        checkTriggersCallbackOnClearData(new int[] {COOKIES_AND_SITE_DATA, CACHE}, "Profile");
    }

    @Test
    @SmallTest
    public void clearUnknownType_TriggersCallback() throws InterruptedException {
        // This is a forward compatibility test: the older versions of Chrome that don't yet
        // implement clearing some data type should just ignore it and call the callback.
        checkTriggersCallbackOnClearData(new int[] {9999}, "Profile");
    }

    @Test
    @SmallTest
    public void twoSuccesiveRequestsTriggerCallbacks() throws InterruptedException {
        WebLayerShellActivity activity = launchWithProfile("profile");

        CountDownLatch latch = new CountDownLatch(2);
        runOnUiThreadBlocking(() -> {
            Profile profile = activity.getBrowserFragmentController().getProfile();
            profile.clearBrowsingData(new int[] {COOKIES_AND_SITE_DATA}).addCallback(
                    (ignored) -> latch.countDown());
            profile.clearBrowsingData(new int[] {CACHE}).addCallback(
                    (ignored) -> latch.countDown());
        });
        assertTrue(latch.await(3, TimeUnit.SECONDS));
    }

    @Test
    @SmallTest
    public void clearingAgainAfterClearFinished_TriggersCallback() throws InterruptedException {
        WebLayerShellActivity activity = launchWithProfile("profile");

        CountDownLatch latch = new CountDownLatch(1);
        runOnUiThreadBlocking(() -> {
            Profile profile = activity.getBrowserFragmentController().getProfile();
            profile.clearBrowsingData(new int[] {COOKIES_AND_SITE_DATA}).addCallback((v1) -> {
                profile.clearBrowsingData(new int[] {CACHE}).addCallback(
                        (v2) -> latch.countDown());
            });
        });
        assertTrue(latch.await(3, TimeUnit.SECONDS));
    }

    @Test
    @SmallTest
    public void destroyingProfileDuringDataClear_DoesntCrash() throws InterruptedException {
        WebLayerShellActivity activity = launchWithProfile("profile");

        CountDownLatch latch = new CountDownLatch(1);
        runOnUiThreadBlocking(() -> {
            Profile profile = activity.getBrowserFragmentController().getProfile();
            profile.clearBrowsingData(new int[] {COOKIES_AND_SITE_DATA});

            // We need to remove the fragment before calling Profile#destroy().
            FragmentManager fm = activity.getSupportFragmentManager();
            fm.beginTransaction().remove(fm.getFragments().get(0)).commitNow();

            profile.destroy();
            latch.countDown();
        });
        assertTrue(latch.await(3, TimeUnit.SECONDS));
    }

    private void checkTriggersCallbackOnClearData(int[] dataTypes, String profileName)
            throws InterruptedException {
        WebLayerShellActivity activity = launchWithProfile(profileName);
        CountDownLatch latch = new CountDownLatch(1);
        runOnUiThreadBlocking(() -> activity.getBrowserFragmentController().getProfile()
                .clearBrowsingData(dataTypes).addCallback((ignored) -> latch.countDown()));
        assertTrue(latch.await(3, TimeUnit.SECONDS));
    }

    private WebLayerShellActivity launchWithProfile(String profileName) {
        Bundle extras = new Bundle();
        extras.putString(WebLayerShellActivity.EXTRA_PROFILE_NAME, profileName);
        String url = "data:text,foo";
        WebLayerShellActivity activity = mActivityTestRule.launchShellWithUrl(url, extras);
        return activity;
    }

}

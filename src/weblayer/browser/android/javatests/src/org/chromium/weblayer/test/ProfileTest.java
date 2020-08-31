// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;
import android.support.test.filters.SmallTest;
import android.webkit.ValueCallback;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Callback;
import org.chromium.weblayer.CookieManager;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.WebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.Arrays;
import java.util.Collection;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Tests that Profile works as expected.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class ProfileTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @Test
    @SmallTest
    public void testCreateAndGetAllProfiles() {
        WebLayer weblayer = mActivityTestRule.getWebLayer();
        {
            // Start with empty profile.
            Collection<Profile> profiles = getAllProfiles();
            Assert.assertTrue(profiles.isEmpty());
        }

        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        Profile firstProfile = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> activity.getBrowser().getProfile());
        {
            // Launching an activity with a fragment creates one profile.
            Collection<Profile> profiles = getAllProfiles();
            Assert.assertEquals(1, profiles.size());
            Assert.assertTrue(profiles.contains(firstProfile));
        }

        Profile secondProfile = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return weblayer.getProfile("second_test"); });

        {
            Collection<Profile> profiles = getAllProfiles();
            Assert.assertEquals(2, profiles.size());
            Assert.assertTrue(profiles.contains(firstProfile));
            Assert.assertTrue(profiles.contains(secondProfile));
        }
    }

    @Test
    @SmallTest
    public void testDestroyAndDeleteDataFromDiskWhenInUse() {
        WebLayer weblayer = mActivityTestRule.getWebLayer();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        final Profile profile = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> activity.getBrowser().getProfile());

        try {
            Callable<Void> c = () -> {
                profile.destroyAndDeleteDataFromDisk(null);
                return null;
            };
            TestThreadUtils.runOnUiThreadBlocking(c);
            Assert.fail();
        } catch (ExecutionException e) {
            // Expected.
        }
    }

    @Test
    @SmallTest
    public void testDestroyAndDeleteDataFromDisk() throws Exception {
        doTestDestroyAndDeleteDataFromDiskIncognito("testDestroyAndDeleteDataFromDisk");
    }

    @Test
    @SmallTest
    public void testDestroyAndDeleteDataFromDiskIncognito() throws Exception {
        doTestDestroyAndDeleteDataFromDiskIncognito(null);
    }

    private void doTestDestroyAndDeleteDataFromDiskIncognito(final String name) throws Exception {
        WebLayer weblayer = mActivityTestRule.getWebLayer();
        final Profile profile =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> weblayer.getProfile(name));

        {
            Collection<Profile> profiles = getAllProfiles();
            Assert.assertTrue(profiles.contains(profile));
        }

        final CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> profile.destroyAndDeleteDataFromDisk(callbackHelper::notifyCalled));
        {
            Collection<Profile> profiles = getAllProfiles();
            Assert.assertFalse(profiles.contains(profile));
        }
        callbackHelper.waitForFirst();
    }

    @Test
    @SmallTest
    public void testEnumerateAllProfileNames() throws Exception {
        final String profileName = "TestEnumerateAllProfileNames";
        final WebLayer weblayer = mActivityTestRule.getWebLayer();
        final InstrumentationActivity activity = mActivityTestRule.launchWithProfile(profileName);
        final Profile profile = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> activity.getBrowser().getProfile());

        Assert.assertTrue(Arrays.asList(enumerateAllProfileNames(weblayer)).contains(profileName));

        TestThreadUtils.runOnUiThreadBlocking(() -> activity.finish());
        CriteriaHelper.pollUiThread(activity::isDestroyed);
        final CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> profile.destroyAndDeleteDataFromDisk(callbackHelper::notifyCalled));
        callbackHelper.waitForFirst();

        Assert.assertFalse(Arrays.asList(enumerateAllProfileNames(weblayer)).contains(profileName));
    }

    private Profile launchAndDestroyActivity(
            String profileName, ValueCallback<InstrumentationActivity> callback) {
        final InstrumentationActivity activity = mActivityTestRule.launchWithProfile(profileName);
        final Profile profile = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> activity.getBrowser().getProfile());

        callback.onReceiveValue(activity);

        TestThreadUtils.runOnUiThreadBlocking(() -> activity.finish());
        CriteriaHelper.pollUiThread(activity::isDestroyed);
        return profile;
    }

    private void destroyAndDeleteDataFromDisk(Profile profile) throws Exception {
        final CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> profile.destroyAndDeleteDataFromDisk(callbackHelper::notifyCalled));
        callbackHelper.waitForFirst();
    }

    @Test
    @SmallTest
    public void testReuseProfile() throws Exception {
        final String profileName = "ReusedProfile";
        final Uri uri = Uri.parse("https://foo.bar");
        final String expectedCookie = "foo=bar";
        {
            // Create profile and activity and set a cookie.
            launchAndDestroyActivity(profileName, (InstrumentationActivity activity) -> {
                CookieManager cookieManager = TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> { return activity.getBrowser().getProfile().getCookieManager(); });
                try {
                    boolean result =
                            mActivityTestRule.setCookie(cookieManager, uri, expectedCookie);
                    Assert.assertTrue(result);
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            });
        }

        {
            // Without deleting proifle data, create profile and activity again, and check the
            // cookie is there.
            Profile profile =
                    launchAndDestroyActivity(profileName, (InstrumentationActivity activity) -> {
                        CookieManager cookieManager =
                                TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
                                    return activity.getBrowser().getProfile().getCookieManager();
                                });
                        try {
                            String cookie = mActivityTestRule.getCookie(cookieManager, uri);
                            Assert.assertEquals(expectedCookie, cookie);
                        } catch (Exception e) {
                            throw new RuntimeException(e);
                        }
                    });

            destroyAndDeleteDataFromDisk(profile);
        }

        {
            // After deleting profile data, create profile and activity again, and check the cookie
            // is deleted as well.
            launchAndDestroyActivity(profileName, (InstrumentationActivity activity) -> {
                CookieManager cookieManager = TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> { return activity.getBrowser().getProfile().getCookieManager(); });
                try {
                    String cookie = mActivityTestRule.getCookie(cookieManager, uri);
                    Assert.assertEquals("", cookie);
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            });
        }
    }

    private static String[] enumerateAllProfileNames(final WebLayer weblayer) throws Exception {
        final String[][] holder = new String[1][];
        final CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Callback<String[]> callback = new Callback<String[]>() {
                @Override
                public void onResult(String[] names) {
                    holder[0] = names;
                    callbackHelper.notifyCalled();
                }
            };
            weblayer.enumerateAllProfileNames(callback);
        });
        callbackHelper.waitForFirst();
        return holder[0];
    }

    private static Collection<Profile> getAllProfiles() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> Profile.getAllProfiles());
    }
}

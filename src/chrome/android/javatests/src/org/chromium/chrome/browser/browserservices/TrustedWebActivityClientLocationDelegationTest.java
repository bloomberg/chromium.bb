// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.content.ComponentName;
import android.net.Uri;
import android.os.Bundle;
import android.os.RemoteException;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.browser.trusted.TrustedWebActivityCallback;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.dependency_injection.ChromeAppComponent;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.concurrent.TimeoutException;

/**
 * Tests TrustedWebActivityClient location delegation.
 *
 * The control flow in these tests is a bit complicated since attempting to connect to a
 * test TrustedWebActivityService in the chrome_public_test results in a ClassLoader error (see
 * https://crbug.com/841178#c1). Therefore we must put the test TrustedWebActivityService in
 * chrome_public_test_support.
 *
 * The general flow of these tests is as follows:
 * 1. Call a method on TrustedWebActivityClient.
 * 2. This calls through to TestTrustedWebActivityService.
 * 3. TestTrustedWebActivityService notify the result with TrustedWebActivityCallback.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class TrustedWebActivityClientLocationDelegationTest {
    private static final Uri SCOPE = Uri.parse("https://www.example.com/notifications");
    private static final Origin ORIGIN = Origin.create(SCOPE);

    private static final String TEST_SUPPORT_PACKAGE = "org.chromium.chrome.tests.support";

    private static final String EXTRA_NEW_LOCATION_AVAILABLE_CALLBACK = "onNewLocationAvailable";
    private static final String EXTRA_NEW_LOCATION_ERROR_CALLBACK = "onNewLocationError";

    private TrustedWebActivityClient mClient;

    @Before
    public void setUp() throws TimeoutException, RemoteException {
        ChromeAppComponent component = ChromeApplication.getComponent();
        mClient = component.resolveTrustedWebActivityClient();

        // TestTrustedWebActivityService is in the test support apk.
        component.resolveTwaPermissionManager().addDelegateApp(ORIGIN, TEST_SUPPORT_PACKAGE);
    }

    /**
     * Tests {@link TrustedWebActivityClient#checkLocationPermission}
     */
    @Test
    @SmallTest
    public void testCheckLocationPermission() throws TimeoutException {
        CallbackHelper locationPermission = new CallbackHelper();

        TrustedWebActivityClient.PermissionCheckCallback callback =
                new TrustedWebActivityClient.PermissionCheckCallback() {
                    @Override
                    public void onPermissionCheck(ComponentName answeringApp, boolean enabled) {
                        locationPermission.notifyCalled();
                    }
                };

        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> mClient.checkLocationPermission(ORIGIN, callback));
        locationPermission.waitForFirst();
    }

    /**
     * Tests {@link TrustedWebActivityClient#startListeningLocationUpdates}
     */
    @Test
    @SmallTest
    public void testStartListeningLocationUpdates() throws TimeoutException {
        CallbackHelper locationUpdate = new CallbackHelper();

        TrustedWebActivityCallback locationUpdateCallback = new TrustedWebActivityCallback() {
            @Override
            public void onExtraCallback(String callbackName, @Nullable Bundle bundle) {
                if (TextUtils.equals(callbackName, EXTRA_NEW_LOCATION_AVAILABLE_CALLBACK)) {
                    Assert.assertNotNull(bundle);
                    Assert.assertTrue(bundle.containsKey("latitude"));
                    Assert.assertTrue(bundle.containsKey("longitude"));
                    Assert.assertTrue(bundle.containsKey("timeStamp"));
                    locationUpdate.notifyCalled();
                }
            }
        };
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                ()
                        -> mClient.startListeningLocationUpdates(
                                ORIGIN, false /* highAccuracy */, locationUpdateCallback));
        locationUpdate.waitForFirst();
    }

    /**
     * Tests {@link TrustedWebActivityClient#startListeningLocationUpdates}
     */
    @Test
    @SmallTest
    public void testStartLocationUpdatesNoConnection() throws TimeoutException {
        Origin otherOrigin = Origin.createOrThrow("https://www.websitewithouttwa.com/");
        CallbackHelper locationError = new CallbackHelper();

        TrustedWebActivityCallback locationUpdateCallback = new TrustedWebActivityCallback() {
            @Override
            public void onExtraCallback(String callbackName, @Nullable Bundle bundle) {
                if (TextUtils.equals(callbackName, EXTRA_NEW_LOCATION_ERROR_CALLBACK)) {
                    Assert.assertNotNull(bundle);
                    Assert.assertTrue(bundle.containsKey("message"));
                    locationError.notifyCalled();
                }
            }
        };
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                ()
                        -> mClient.startListeningLocationUpdates(
                                otherOrigin, false /* highAccuracy */, locationUpdateCallback));
        locationError.waitForFirst();
    }
}

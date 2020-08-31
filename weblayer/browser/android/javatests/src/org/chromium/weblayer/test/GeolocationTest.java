// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.Manifest;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.support.test.filters.MediumTest;

import androidx.core.app.ActivityCompat;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.InMemorySharedPreferencesContext;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that Geolocation Web API works as expected.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public final class GeolocationTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;
    private TestWebLayer mTestWebLayer;
    private TestWebServer mTestServer;
    private int mLocationPermission = PackageManager.PERMISSION_GRANTED;
    private String mTestUrl;

    private static final String RAW_JAVASCRIPT =
            "var positionCount = 0;"
            + "var errorCount = 0;"
            + "function gotPos(position) {"
            + "  positionCount++;"
            + "}"
            + "function errorCallback(error){"
            + "  errorCount++;"
            + "}"
            + "function initiate_getCurrentPosition() {"
            + "  navigator.geolocation.getCurrentPosition("
            + "      gotPos, errorCallback, {});"
            + "}"
            + "function initiate_watchPosition() {"
            + "  navigator.geolocation.watchPosition("
            + "      gotPos, errorCallback, {});"
            + "}";

    private static final String RAW_HTML =
            "<!doctype html>"
            + "<html>"
            + "  <head>"
            + "    <title>Geolocation</title>"
            + "    <script>" + RAW_JAVASCRIPT + "</script>"
            + "  </head>"
            + "  <body>"
            + "  </body>"
            + "</html>";

    @TargetApi(Build.VERSION_CODES.M)
    private class PermissionCompatDelegate implements ActivityCompat.PermissionCompatDelegate {
        private CallbackHelper mCallbackHelper = new CallbackHelper();

        @Override
        public boolean requestPermissions(
                Activity activity, String[] permissions, int requestCode) {
            mCallbackHelper.notifyCalled();
            return false;
        }

        @Override
        public boolean onActivityResult(
                Activity activity, int requestCode, int resultCode, Intent data) {
            return false;
        }

        public void waitForPermissionsRequest() throws Exception {
            mCallbackHelper.waitForFirst();
        }
    }

    @Before
    public void setUp() throws Throwable {
        Bundle extras = new Bundle();
        // We need to override the context with which to create WebLayer.
        extras.putBoolean(InstrumentationActivity.EXTRA_CREATE_WEBLAYER, false);
        mActivity = mActivityTestRule.launchShell(extras);
        Assert.assertNotNull(mActivity);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.loadWebLayerSync(new InMemorySharedPreferencesContext(
                    mActivity.getApplication()) {
                @Override
                public int checkPermission(String permission, int pid, int uid) {
                    if (permission.equals(Manifest.permission.ACCESS_FINE_LOCATION)
                            || permission.equals(Manifest.permission.ACCESS_COARSE_LOCATION)) {
                        return mLocationPermission;
                    }
                    return getBaseContext().checkPermission(permission, pid, uid);
                }
            });
        });
        mActivityTestRule.navigateAndWait("about:blank");

        mTestWebLayer = TestWebLayer.getTestWebLayer(mActivity.getApplicationContext());
        mTestWebLayer.setSystemLocationSettingEnabled(true);
        mTestWebLayer.setMockLocationProvider(true /* enable */);

        mTestServer = TestWebServer.start();
        mTestUrl = mTestServer.setResponse("/geolocation.html", RAW_HTML, null);

        mActivityTestRule.navigateAndWait(mTestUrl);
        ensureGeolocationIsRunning(false);
    }

    @After
    public void tearDown() throws Throwable {
        mTestWebLayer.setMockLocationProvider(false /* enable */);
        ensureGeolocationIsRunning(false);
    }

    /**
     * Test for navigator.getCurrentPosition.
     */
    @Test
    @MediumTest
    public void testGeolocation_getPosition() throws Throwable {
        mActivityTestRule.executeScriptSync("initiate_getCurrentPosition();", false);
        waitForDialog();
        mTestWebLayer.clickPermissionDialogButton(true);
        waitForCountEqual("positionCount", 1);
        mActivityTestRule.executeScriptSync("initiate_getCurrentPosition();", false);
        waitForCountEqual("positionCount", 2);
        Assert.assertEquals(0, getCountFromJS("errorCount"));
    }

    /**
     * Test for navigator.watchPosition, should receive more than one position.
     */
    @Test
    @MediumTest
    public void testGeolocation_watchPosition() throws Throwable {
        mActivityTestRule.executeScriptSync("initiate_watchPosition();", false);
        waitForDialog();
        mTestWebLayer.clickPermissionDialogButton(true);
        waitForCountGreaterThan("positionCount", 1);
        ensureGeolocationIsRunning(true);
        Assert.assertEquals(0, getCountFromJS("errorCount"));
    }

    /**
     * Test that destroying a tab stops geolocation provider.
     */
    @Test
    @MediumTest
    public void testGeolocation_destroyTabStopsGeolocation() throws Throwable {
        mActivityTestRule.executeScriptSync("initiate_watchPosition();", false);
        waitForDialog();
        mTestWebLayer.clickPermissionDialogButton(true);
        ensureGeolocationIsRunning(true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Browser browser = mActivity.getBrowser();
            browser.destroyTab(mActivity.getBrowser().getActiveTab());
            Assert.assertEquals(0, browser.getTabs().size());
        });
        ensureGeolocationIsRunning(false);
    }

    /**
     * Test geolocation denied on insecure origins (e.g. javascript).
     */
    @Test
    @MediumTest
    public void testGeolocation_denyOnInsecureOrigins() throws Throwable {
        mActivityTestRule.navigateAndWait("about:blank");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivity.getTab().getNavigationController().navigate(
                    Uri.parse("javascript:" + RAW_JAVASCRIPT + "initiate_getCurrentPosition();"));
        });
        waitForCountEqual("errorCount", 1);
        Assert.assertEquals(0, getCountFromJS("positionCount"));
    }

    @Test
    @MediumTest
    public void testGeolocation_denyFromPrompt() throws Throwable {
        mActivityTestRule.executeScriptSync("initiate_watchPosition();", false);
        waitForDialog();
        mTestWebLayer.clickPermissionDialogButton(false);
        waitForCountEqual("errorCount", 1);
        Assert.assertEquals(0, getCountFromJS("positionCount"));
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    public void testRequestSystemPermission() throws Throwable {
        mActivityTestRule.executeScriptSync("initiate_watchPosition();", false);
        waitForDialog();
        mTestWebLayer.clickPermissionDialogButton(true);

        // Reload and deny the system permission, so it is prompted on the next call to geolocation.
        mActivityTestRule.navigateAndWait(mTestUrl);

        PermissionCompatDelegate delegate = new PermissionCompatDelegate();
        ActivityCompat.setPermissionCompatDelegate(delegate);
        mLocationPermission = PackageManager.PERMISSION_DENIED;
        mActivityTestRule.executeScriptSync("initiate_watchPosition();", false);

        delegate.waitForPermissionsRequest();
    }

    // helper methods

    private void waitForCountEqual(String variableName, int count) {
        CriteriaHelper.pollInstrumentationThread(
                () -> { return getCountFromJS(variableName) == count; });
    }

    private void waitForDialog() throws Exception {
        // Make sure the current permission state is "prompt" before waiting for the dialog.
        mActivityTestRule.executeScriptSync("var queryResult;"
                        + "navigator.permissions.query({name: 'geolocation'}).then("
                        + "function(result) { queryResult = result.state; })",
                false);
        CriteriaHelper.pollInstrumentationThread(() -> {
            return !mActivityTestRule.executeScriptSync("queryResult || ''", false)
                            .getString(Tab.SCRIPT_RESULT_KEY)
                            .equals("");
        });
        Assert.assertEquals("prompt",
                mActivityTestRule.executeScriptSync("queryResult", false)
                        .getString(Tab.SCRIPT_RESULT_KEY));
        CriteriaHelper.pollInstrumentationThread(
                () -> { return mTestWebLayer.isPermissionDialogShown(); });
    }

    private void waitForCountGreaterThan(String variableName, int count) {
        CriteriaHelper.pollInstrumentationThread(
                () -> { return getCountFromJS(variableName) > count; });
    }

    private void ensureGeolocationIsRunning(boolean running) {
        CriteriaHelper.pollInstrumentationThread(
                () -> { return mTestWebLayer.isMockLocationProviderRunning() == running; });
    }

    private int getCountFromJS(String variableName) {
        int result = -1;
        try {
            result = mActivityTestRule.executeScriptSync(variableName, false)
                             .getInt(Tab.SCRIPT_RESULT_KEY);
        } catch (Exception e) {
            Assert.fail("Unable to get " + variableName);
        }
        return result;
    }
}

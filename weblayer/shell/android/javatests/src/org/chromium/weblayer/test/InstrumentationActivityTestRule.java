// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.app.Activity;
import android.app.Instrumentation.ActivityMonitor;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Navigation;
import org.chromium.weblayer.NavigationCallback;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.WebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.lang.reflect.Field;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * ActivityTestRule for InstrumentationActivity.
 *
 * Test can use this ActivityTestRule to launch or get InstrumentationActivity.
 */
public class InstrumentationActivityTestRule extends ActivityTestRule<InstrumentationActivity> {
    private static final class NavigationWaiter {
        private String mUrl;
        private Tab mTab;
        private boolean mNavigationComplete;
        private boolean mDoneLoading;
        private boolean mContentfulPaint;
        private CallbackHelper mCallbackHelper = new CallbackHelper();

        private NavigationCallback mNavigationCallback = new NavigationCallback() {
            @Override
            public void onNavigationCompleted(Navigation navigation) {
                if (navigation.getUri().toString().equals(mUrl)) {
                    mNavigationComplete = true;
                    checkComplete();
                }
            }

            @Override
            public void onLoadStateChanged(boolean isLoading, boolean toDifferentDocument) {
                mDoneLoading = !isLoading;
                checkComplete();
            }

            @Override
            public void onFirstContentfulPaint() {
                mContentfulPaint = true;
                checkComplete();
            }
        };

        // |waitForPaint| should generally be set to true, unless there is a specific reason for
        // onFirstContentfulPaint to not fire.
        public NavigationWaiter(String url, Tab controller, boolean waitForPaint) {
            mUrl = url;
            mTab = controller;
            if (!waitForPaint) mContentfulPaint = true;
        }

        public void navigateAndWait() {
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                mTab.getNavigationController().registerNavigationCallback(mNavigationCallback);
                mTab.getNavigationController().navigate(Uri.parse(mUrl));
            });
            try {
                mCallbackHelper.waitForCallback(
                        0, 1, CallbackHelper.WAIT_TIMEOUT_SECONDS * 2, TimeUnit.SECONDS);
            } catch (TimeoutException e) {
                throw new RuntimeException(e);
            }
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                mTab.getNavigationController().unregisterNavigationCallback(mNavigationCallback);
            });
        }

        private void checkComplete() {
            if (mNavigationComplete && mDoneLoading && mContentfulPaint) {
                mCallbackHelper.notifyCalled();
            }
        }
    }

    private static final class JSONCallbackHelper extends CallbackHelper {
        private JSONObject mResult;

        public JSONObject getResult() {
            return mResult;
        }

        public void notifyCalled(JSONObject result) {
            mResult = result;
            notifyCalled();
        }
    }

    public InstrumentationActivityTestRule() {
        super(InstrumentationActivity.class, false, false);
    }

    public WebLayer getWebLayer() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return WebLayer
                    .create(InstrumentationRegistry.getTargetContext().getApplicationContext())
                    .get();
        });
    }

    /**
     * Starts the WebLayer activity with the given extras Bundle. This does not create and load
     * WebLayer.
     */
    public InstrumentationActivity launchShell(Bundle extras) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.putExtras(extras);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setComponent(
                new ComponentName(InstrumentationRegistry.getInstrumentation().getTargetContext(),
                        InstrumentationActivity.class));
        return launchActivity(intent);
    }

    /**
     * Starts the WebLayer activity with the given extras Bundle and completely loads the given URL
     * (this calls navigateAndWait()).
     */
    public InstrumentationActivity launchShellWithUrl(String url, Bundle extras) {
        InstrumentationActivity activity = launchShell(extras);
        Assert.assertNotNull(activity);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.createWebLayer(activity.getApplication(), null).get(); });
        if (url != null) navigateAndWait(url);
        return activity;
    }

    /**
     * Starts the WebLayer activity and completely loads the given URL (this calls
     * navigateAndWait()).
     */
    public InstrumentationActivity launchShellWithUrl(String url) {
        return launchShellWithUrl(url, new Bundle());
    }

    /**
     * Loads the given URL in the shell.
     */
    public void navigateAndWait(String url) {
        navigateAndWait(getActivity().getTab(), url, true /* waitForPaint */);
    }

    public void navigateAndWait(Tab controller, String url, boolean waitForPaint) {
        NavigationWaiter waiter = new NavigationWaiter(url, controller, waitForPaint);
        waiter.navigateAndWait();
    }

    /**
     * Recreates the Activity, blocking until finished.
     * After calling this, getActivity() returns the new Activity.
     */
    public void recreateActivity() {
        Activity activity = getActivity();

        ActivityMonitor monitor =
                new ActivityMonitor(InstrumentationActivity.class.getName(), null, false);
        InstrumentationRegistry.getInstrumentation().addMonitor(monitor);

        TestThreadUtils.runOnUiThreadBlocking(activity::recreate);

        CriteriaHelper.pollUiThread(
                () -> monitor.getLastActivity() != null && monitor.getLastActivity() != activity);
        InstrumentationRegistry.getInstrumentation().removeMonitor(monitor);

        // There is no way to rotate the activity using ActivityTestRule or even notify it.
        // So we have to hack...
        try {
            Field field = ActivityTestRule.class.getDeclaredField("mActivity");
            field.setAccessible(true);
            field.set(this, monitor.getLastActivity());
        } catch (ReflectiveOperationException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Executes the script passed in and waits for the result.
     */
    public JSONObject executeScriptSync(String script, boolean useSeparateIsolate) {
        JSONCallbackHelper callbackHelper = new JSONCallbackHelper();
        int count = callbackHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getActivity().getTab().executeScript(script, useSeparateIsolate,
                    (JSONObject result) -> { callbackHelper.notifyCalled(result); });
        });
        try {
            callbackHelper.waitForCallback(count);
        } catch (TimeoutException e) {
            throw new RuntimeException(e);
        }
        return callbackHelper.getResult();
    }

    public int executeScriptAndExtractInt(String script) {
        try {
            return executeScriptSync(script, true /* useSeparateIsolate */)
                    .getInt(Tab.SCRIPT_RESULT_KEY);
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
    }

    public String executeScriptAndExtractString(String script) {
        try {
            return executeScriptSync(script, true /* useSeparateIsolate */)
                    .getString(Tab.SCRIPT_RESULT_KEY);
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
    }

    public boolean executeScriptAndExtractBoolean(String script) {
        try {
            return executeScriptSync(script, true /* useSeparateIsolate */)
                    .getBoolean(Tab.SCRIPT_RESULT_KEY);
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
    }

    public InstrumentationActivity launchWithProfile(String profileName) {
        Bundle extras = new Bundle();
        extras.putString(InstrumentationActivity.EXTRA_PROFILE_NAME, profileName);
        String url = "data:text,foo";
        return launchShellWithUrl(url, extras);
    }
}

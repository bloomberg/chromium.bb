// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.app.Activity;
import android.app.DialogFragment;
import android.app.Fragment;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.ActivityOptionsCompat;

import org.junit.Assert;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.util.Locale;
import java.util.concurrent.Callable;

/**
 * Collection of activity utilities.
 */
public class ActivityUtils {
    private static final String TAG = "cr_ActivityUtils";

    private static final long ACTIVITY_START_TIMEOUT_MS = ScalableTimeout.scaleTimeout(3000);
    private static final long CONDITION_POLL_INTERVAL_MS = 100;

    /**
     * Waits for a particular fragment to be present on a given activity.
     */
    private static class FragmentPresentCriteria extends Criteria {

        private final Activity mActivity;
        private final String mFragmentTag;

        public FragmentPresentCriteria(Activity activity, String fragmentTag) {
            super(String.format("Could not locate the fragment with tag '%s'", fragmentTag));
            mActivity = activity;
            mFragmentTag = fragmentTag;
        }

        @Override
        public boolean isSatisfied() {
            Fragment fragment = mActivity.getFragmentManager().findFragmentByTag(mFragmentTag);
            if (fragment == null) return false;
            if (fragment instanceof DialogFragment) {
                DialogFragment dialogFragment = (DialogFragment) fragment;
                return dialogFragment.getDialog() != null
                        && dialogFragment.getDialog().isShowing();
            }
            return fragment.getView() != null;
        }
    }

    /**
     * Captures an activity of a particular type by launching an intent explicitly targeting the
     * activity.
     *
     * @param <T> The type of activity to wait for.
     * @param activityType The class type of the activity.
     * @return The spawned activity.
     */
    public static <T> T waitForActivity(
            final Instrumentation instrumentation, final Class<T> activityType) {
        Runnable intentTrigger = new Runnable() {
            @Override
            public void run() {
                Context context = instrumentation.getTargetContext().getApplicationContext();
                Intent activityIntent = new Intent();
                activityIntent.setClass(context, activityType);
                activityIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                activityIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);

                Bundle optionsBundle =
                        ActivityOptionsCompat
                                .makeCustomAnimation(context, R.anim.activity_open_enter, 0)
                                .toBundle();
                IntentUtils.safeStartActivity(context, activityIntent, optionsBundle);
            }
        };
        return waitForActivity(instrumentation, activityType, intentTrigger);
    }

    /**
     * Captures an activity of a particular type that is triggered from some action.
     *
     * @param <T> The type of activity to wait for.
     * @param activityType The class type of the activity.
     * @param activityTrigger The action that will trigger the new activity (run in this thread).
     * @return The spawned activity.
     */
    public static <T> T waitForActivity(Instrumentation instrumentation, Class<T> activityType,
            Runnable activityTrigger) {
        Callable<Void> callableWrapper = new Callable<Void>() {
            @Override
            public Void call() {
                activityTrigger.run();
                return null;
            }
        };

        try {
            return waitForActivityWithTimeout(
                    instrumentation, activityType, callableWrapper, ACTIVITY_START_TIMEOUT_MS);
        } catch (Exception e) {
            // We just ignore checked exceptions here since Runnables can't throw them.
        }
        return null;
    }

    /**
     * Captures an activity of a particular type that is triggered from some action.
     *
     * @param <T> The type of activity to wait for.
     * @param activityType The class type of the activity.
     * @param activityTrigger The action that will trigger the new activity (run in this thread).
     * @return The spawned activity.
     */
    public static <T> T waitForActivity(Instrumentation instrumentation, Class<T> activityType,
            Callable<Void> activityTrigger) throws Exception {
        return waitForActivityWithTimeout(instrumentation, activityType, activityTrigger,
                ACTIVITY_START_TIMEOUT_MS);
    }

    /**
     * Captures an activity of a particular type that is triggered from some action.
     *
     * @param activityType The class type of the activity.
     * @param activityTrigger The action that will trigger the new activity (run in this thread).
     * @param timeOut The maximum time to wait for activity creation
     * @return The spawned activity.
     */
    public static <T> T waitForActivityWithTimeout(Instrumentation instrumentation,
            Class<T> activityType, Callable<Void> activityTrigger, long timeOut) throws Exception {
        ActivityMonitor monitor =
                instrumentation.addMonitor(activityType.getCanonicalName(), null, false);

        activityTrigger.call();
        instrumentation.waitForIdleSync();
        Activity activity = monitor.getLastActivity();
        if (activity == null) {
            activity = monitor.waitForActivityWithTimeout(timeOut);
            if (activity == null) logRunningChromeActivities();
        }
        Assert.assertNotNull(activityType.getName() + " did not start in: " + timeOut, activity);

        return activityType.cast(activity);
    }

    private static void logRunningChromeActivities() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            StringBuilder builder = new StringBuilder("Running Chrome Activities: ");
            for (Activity activity : ApplicationStatus.getRunningActivities()) {
                builder.append(String.format(Locale.US, "\n   %s : %d",
                        activity.getClass().getSimpleName(),
                        ApplicationStatus.getStateForActivity(activity)));
            }
            Log.i(TAG, builder.toString());
        });
    }

    /**
     * Waits for a fragment to be registered by the specified activity.
     *
     * @param activity The activity that owns the fragment.
     * @param fragmentTag The tag of the fragment to be loaded.
     */
    @SuppressWarnings({"unchecked", "TypeParameterUnusedInFormals"})
    public static <T> T waitForFragment(Activity activity, String fragmentTag) {
        CriteriaHelper.pollInstrumentationThread(new FragmentPresentCriteria(activity, fragmentTag),
                ACTIVITY_START_TIMEOUT_MS, CONDITION_POLL_INTERVAL_MS);
        return (T) activity.getFragmentManager().findFragmentByTag(fragmentTag);
    }

    /**
     * Waits until the specified fragment has been attached to the specified activity. Note that
     * we don't guarantee that the fragment is visible. Some UI operations can happen too
     * quickly and we can miss the time that a fragment is visible. This method allows you to get a
     * reference to any fragment that was attached to the activity at any point.
     *
     * @param <T> A subclass of android.app.Fragment
     * @param activity An instance or subclass of Preferences
     * @param fragmentClass The class object for T
     * @return A reference to the requested fragment or null.
     */
    @SuppressWarnings("unchecked")
    public static <T extends Fragment> T waitForFragmentToAttach(
            final Preferences activity, final Class<T> fragmentClass) {
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Could not find fragment " + fragmentClass) {
                    @Override
                    public boolean isSatisfied() {
                        return fragmentClass.isInstance(activity.getFragmentForTest());
                    }
                }, ACTIVITY_START_TIMEOUT_MS, CONDITION_POLL_INTERVAL_MS);
        return (T) activity.getFragmentForTest();
    }
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityOptionsCompat;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.Fragment;

import org.junit.Assert;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.test.util.TimeoutTimer;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Locale;
import java.util.concurrent.Callable;

/**
 * Collection of activity utilities.
 */
public class ActivityUtils {
    private static final String TAG = "ActivityUtils";

    private static final long ACTIVITY_START_TIMEOUT_MS = 3000L;
    private static final long CONDITION_POLL_INTERVAL_MS = 100;

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
        TimeoutTimer timer = new TimeoutTimer(timeOut);
        ActivityMonitor monitor =
                instrumentation.addMonitor(activityType.getCanonicalName(), null, false);

        activityTrigger.call();
        instrumentation.waitForIdleSync();
        Activity activity = monitor.getLastActivity();
        while (activity == null && !timer.isTimedOut()) {
            activity = monitor.waitForActivityWithTimeout(timer.getRemainingMs());
        }
        if (activity == null) logRunningChromeActivities();
        Assert.assertNotNull(activityType.getName() + " did not start in: " + timeOut, activity);

        return activityType.cast(activity);
    }

    private static void logRunningChromeActivities() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
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
    @SuppressWarnings("unchecked")
    public static <T extends Fragment> T waitForFragment(
            AppCompatActivity activity, String fragmentTag) {
        String failureReason =
                String.format("Could not locate the fragment with tag '%s'", fragmentTag);
        CriteriaHelper.pollInstrumentationThread(new Criteria(failureReason) {
            @Override
            public boolean isSatisfied() {
                Fragment fragment =
                        activity.getSupportFragmentManager().findFragmentByTag(fragmentTag);
                if (fragment == null) return false;
                if (fragment instanceof DialogFragment) {
                    DialogFragment dialogFragment = (DialogFragment) fragment;
                    return dialogFragment.getDialog() != null
                            && dialogFragment.getDialog().isShowing();
                }
                return fragment.getView() != null;
            }
        }, ACTIVITY_START_TIMEOUT_MS, CONDITION_POLL_INTERVAL_MS);
        return (T) activity.getSupportFragmentManager().findFragmentByTag(fragmentTag);
    }

    /**
     * Waits until the specified fragment has been attached to the specified activity. Note that
     * we don't guarantee that the fragment is visible. Some UI operations can happen too
     * quickly and we can miss the time that a fragment is visible. This method allows you to get a
     * reference to any fragment that was attached to the activity at any point.
     *
     * @param <T> A subclass of {@link Fragment}.
     * @param activity An instance or subclass of {@link SettingsActivity}.
     * @param fragmentClass The class object for {@link T}.
     * @return A reference to the requested fragment or null.
     */
    @SuppressWarnings("unchecked")
    public static <T extends Fragment> T waitForFragmentToAttach(
            final SettingsActivity activity, final Class<T> fragmentClass) {
        String failureReason = String.format(
                "Could not find fragment of type %s", fragmentClass.getCanonicalName());
        CriteriaHelper.pollInstrumentationThread(new Criteria(failureReason) {
            @Override
            public boolean isSatisfied() {
                return fragmentClass.isInstance(activity.getMainFragment());
            }
        }, ACTIVITY_START_TIMEOUT_MS, CONDITION_POLL_INTERVAL_MS);
        return (T) activity.getMainFragment();
    }
}

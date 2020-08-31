// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.Intent;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.metrics.WebApkSplashscreenMetrics;
import org.chromium.chrome.browser.metrics.WebApkUma;

import javax.inject.Inject;

/**
 * Handles recording user metrics for WebAPK activities.
 */
@ActivityScope
public class WebApkActivityLifecycleUmaTracker
        implements ActivityStateListener, InflationObserver, PauseResumeWithNativeObserver {
    @VisibleForTesting
    public static final String STARTUP_UMA_HISTOGRAM_SUFFIX = ".WebApk";

    private final ChromeActivity mActivity;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final SplashController mSplashController;

    /** The start time that the activity becomes focused in milliseconds since boot. */
    private long mStartTime;

    @Inject
    public WebApkActivityLifecycleUmaTracker(ChromeActivity<?> activity,
            BrowserServicesIntentDataProvider intentDataProvider, SplashController splashController,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            WebappDeferredStartupWithStorageHandler deferredStartupWithStorageHandler) {
        mActivity = activity;
        mIntentDataProvider = intentDataProvider;
        mSplashController = splashController;

        lifecycleDispatcher.register(this);
        ApplicationStatus.registerStateListenerForActivity(this, mActivity);

        // Add UMA recording task at the front of the deferred startup queue as it has a higher
        // priority than other deferred startup tasks like checking for a WebAPK update.
        deferredStartupWithStorageHandler.addTaskToFront((storage, didCreateStorage) -> {
            if (activity.isActivityFinishingOrDestroyed()) return;

            WebApkExtras webApkExtras = mIntentDataProvider.getWebApkExtras();
            WebApkUma.recordShellApkVersion(webApkExtras.shellApkVersion, webApkExtras.distributor);
        });
    }

    @Override
    public void onActivityStateChange(Activity activity, @ActivityState int newState) {
        if (newState == ActivityState.RESUMED) {
            mStartTime = SystemClock.elapsedRealtime();
        }
    }

    @Override
    public void onPreInflationStartup() {
        // Decide whether to record startup UMA histograms. This is a similar check to the one done
        // in ChromeTabbedActivity.performPreInflationStartup refer to the comment there for why.
        if (!LibraryLoader.getInstance().isInitialized()) {
            mActivity.getActivityTabStartupMetricsTracker().trackStartupMetrics(
                    STARTUP_UMA_HISTOGRAM_SUFFIX);
            // If there is a saved instance state, then the intent (and its stored timestamp) might
            // be stale (Android replays intents if there is a recents entry for the activity).
            if (mActivity.getSavedInstanceState() == null) {
                Intent intent = mActivity.getIntent();
                // Splash observers are removed once the splash screen is hidden.
                mSplashController.addObserver(new WebApkSplashscreenMetrics(
                        WebappIntentUtils.getWebApkShellLaunchTime(intent),
                        WebappIntentUtils.getNewStyleWebApkSplashShownTime(intent)));
            }
        }
    }

    @Override
    public void onPostInflationStartup() {}

    @Override
    public void onResumeWithNative() {}

    @Override
    public void onPauseWithNative() {
        WebApkExtras webApkExtras = mIntentDataProvider.getWebApkExtras();
        long sessionDuration = SystemClock.elapsedRealtime() - mStartTime;
        WebApkUma.recordWebApkSessionDuration(webApkExtras.distributor, sessionDuration);
        WebApkUkmRecorder.recordWebApkSessionDuration(webApkExtras.manifestUrl,
                webApkExtras.distributor, webApkExtras.webApkVersionCode, sessionDuration);
    }
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import android.content.ComponentName;
import android.content.Context;

import org.chromium.webapk.shell_apk.HostBrowserLauncher;
import org.chromium.webapk.shell_apk.HostBrowserLauncherParams;
import org.chromium.webapk.shell_apk.TransparentLauncherActivity;

/**
 * UI-less activity which launches host browser. Relaunches itself if the android.intent.action.MAIN
 * intent handler needs to be switched.
 */
public class H2OTransparentLauncherActivity extends TransparentLauncherActivity {
    @Override
    protected void onHostBrowserSelected(HostBrowserLauncherParams params) {
        if (params == null) {
            finish();
            return;
        }

        boolean shouldLaunchSplash = H2OLauncher.shouldIntentLaunchSplashActivity(params);
        if (relaunchIfNeeded(params, shouldLaunchSplash)) {
            finish();
            return;
        }

        if (shouldLaunchSplash) {
            // Launch {@link SplashActivity} first instead of directly launching the host
            // browser so that for a WebAPK launched via
            // {@link H2OTransparentHostBrowserLauncherActivity}, tapping the app icon
            // brings the WebAPK activity stack to the foreground and does not create a
            // new activity stack.
            Context appContext = getApplicationContext();
            H2OLauncher.copyIntentExtrasAndLaunch(appContext, getIntent(),
                    params.getSelectedShareTargetActivityClassName(),
                    new ComponentName(appContext, SplashActivity.class));
            finish();
            return;
        }

        HostBrowserLauncher.launch(getApplicationContext(), params);
        finish();
    }

    /**
     * Launches the android.intent.action.MAIN intent handler if the activity which currently
     * handles the android.intent.action.MAIN intent needs to be switched. Launching the main intent
     * handler is done to minimize the number of places which enable/disable components.
     */
    private boolean relaunchIfNeeded(HostBrowserLauncherParams params, boolean shouldLaunchSplash) {
        Context appContext = getApplicationContext();

        // {@link H2OLauncher#changeEnabledComponentsAndKillShellApk()} enables one
        // component, THEN disables the other. Relaunch if the wrong component is disabled (vs
        // if the wrong component is enabled) to handle the case where both components are enabled.
        ComponentName relaunchComponent = null;
        if (shouldLaunchSplash) {
            // Relaunch if SplashActivity is disabled.
            if (!SplashActivity.checkComponentEnabled(appContext)) {
                relaunchComponent = new ComponentName(appContext, H2OMainActivity.class);
            }
        } else {
            // Relaunch if H2OMainActivity is disabled.
            if (!H2OMainActivity.checkComponentEnabled(appContext)) {
                relaunchComponent = new ComponentName(appContext, SplashActivity.class);
            }
        }

        if (relaunchComponent == null) {
            return false;
        }

        H2OLauncher.copyIntentExtrasAndLaunch(getApplicationContext(), getIntent(),
                params.getSelectedShareTargetActivityClassName(), relaunchComponent);
        return true;
    }
}

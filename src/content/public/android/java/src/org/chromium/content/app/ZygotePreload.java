// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.app;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.pm.ApplicationInfo;
import android.os.Build;
import android.os.Process;
import android.os.SystemClock;

import org.chromium.base.JNIUtils;
import org.chromium.base.Log;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.process_launcher.ChildProcessService;

/**
 * Class used in android:zygotePreloadName attribute of manifest.
 * Code in this class runs in the zygote. It runs in a limited environment
 * (eg no application) and cannot communicate with any other app process,
 * so care must be taken when writing code in this class. Eg it should not
 * create any thread.
 */
@TargetApi(Build.VERSION_CODES.Q)
public class ZygotePreload implements android.app.ZygotePreload {
    private static final String TAG = "ZygotePreload";

    @SuppressLint("Override")
    @Override
    public void doPreload(ApplicationInfo appInfo) {
        try {
            // The current thread time is the best approximation we have of the zygote start time
            // since Process.getStartUptimeMillis() is not reliable in the zygote process. This will
            // be the total CPU time the current thread has been running, and is reset on fork so
            // should give an accurate measurement of zygote process startup time.
            ChildProcessService.setZygoteInfo(
                    Process.myPid(), SystemClock.currentThreadTimeMillis());
            JNIUtils.enableSelectiveJniRegistration();
            LibraryLoader.getInstance().loadNowInZygote(appInfo);
        } catch (Throwable e) {
            // Ignore any exception. Child service can continue loading.
            Log.w(TAG, "Exception in zygote", e);
        }
    }
}

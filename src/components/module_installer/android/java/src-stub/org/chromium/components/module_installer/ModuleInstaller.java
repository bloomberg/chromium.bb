// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer;

import android.content.Context;

import org.chromium.base.VisibleForTesting;

/** Dummy fallback of ModuleInstaller for APK builds. */
public class ModuleInstaller {
    public static void init() {}

    public static void initActivity(Context context) {}

    public static void recordModuleAvailability() {}

    public static void updateCrashKeys(){};

    public static void install(
            String moduleName, OnModuleInstallFinishedListener onFinishedListener) {
        throw new UnsupportedOperationException("Cannot install module if APK");
    }

    public static void installDeferred(String moduleName) {
        throw new UnsupportedOperationException("Cannot deferred install module if APK");
    }

    @VisibleForTesting
    public static boolean didRequestDeferred(String moduleName) {
        throw new UnsupportedOperationException();
    }

    private ModuleInstaller() {}
}

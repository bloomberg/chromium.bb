// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.support.annotation.Nullable;

import org.chromium.base.BundleUtils;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.modules.ModuleInstallUi;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.module_installer.ModuleInstaller;

/**
 * Manages the loading of autofill assistant DFM, and provides implementation of
 * AutofillAssistantModuleEntry.
 */
public class AutofillAssistantModuleEntryProvider {
    private static final String TAG = "AutofillAssistant";

    /**
     * The real singleton instances. Using an instance instead of static methods allows modifying
     * method implementation during tests. Outside of tests, this is the only instance that should
     * exist.
     */
    static final AutofillAssistantModuleEntryProvider INSTANCE =
            new AutofillAssistantModuleEntryProvider();

    /* Returns the AA module entry, if it is already installed. */
    @Nullable
    /* package */ AutofillAssistantModuleEntry getModuleEntryIfInstalled(Context context) {
        // Required to access resources in DFM using this activity as context.
        ModuleInstaller.getInstance().initActivity(context);
        if (AutofillAssistantModule.isInstalled()) {
            return AutofillAssistantModule.getImpl();
        }
        return null;
    }

    /** Gets the AA module entry, installing it if necessary. */
    /* package */ void getModuleEntry(
            Context context, Tab tab, Callback<AutofillAssistantModuleEntry> callback) {
        AutofillAssistantModuleEntry entry = getModuleEntryIfInstalled(context);
        if (entry != null) {
            callback.onResult(entry);
            return;
        }
        loadDynamicModuleWithUi(context, tab, callback);
    }

    /**
     * Maybe trigger a deferred install of the module.
     *
     * <p>This is public so that it can be used in the Chrome upgrade package. The conditions for
     * eligibility are:
     *
     * <ul>
     *   <li>This is a Bundle build.
     *   <li>The autofill_assistant DFM is not installed yet.
     *   <li>The Android version is L+.
     *   <li>The device has high disk capacity.
     * </ul>
     */
    public static void maybeInstallDeferred() {
        boolean isNotBundle = !BundleUtils.isBundle();
        boolean isInstalled = AutofillAssistantModule.isInstalled();
        boolean isVersionBeforeLollipop =
                android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.LOLLIPOP;
        boolean isNotHighEndDiskDevice = !SysUtils.isHighEndDiskDevice();
        if (isNotBundle || isInstalled || isVersionBeforeLollipop || isNotHighEndDiskDevice) {
            Log.v(TAG,
                    "Deferred install not triggered: not_bundle=" + isNotBundle
                            + ", already_installed=" + isInstalled
                            + ", before_lollipop=" + isVersionBeforeLollipop
                            + ", not_high_end_device=" + isNotHighEndDiskDevice);
            return;
        }
        Log.v(TAG, "Deferred install triggered.");
        AutofillAssistantModule.installDeferred();
    }

    private static void loadDynamicModuleWithUi(
            Context activity, Tab tab, Callback<AutofillAssistantModuleEntry> callback) {
        ModuleInstallUi ui = new ModuleInstallUi(tab, R.string.autofill_assistant_module_title,
                new ModuleInstallUi.FailureUiListener() {
                    @Override
                    public void onRetry() {
                        loadDynamicModuleWithUi(activity, tab, callback);
                    }

                    @Override
                    public void onCancel() {
                        callback.onResult(null);
                    }
                });
        // Shows toast informing user about install start.
        ui.showInstallStartUi();
        ModuleInstaller.getInstance().install("autofill_assistant", (success) -> {
            if (success) {
                // Clean install of chrome will have issues here without initializing
                // after installation of DFM.
                ModuleInstaller.getInstance().initActivity(activity);
                // Don't show success UI from DFM, transition to autobot UI directly.
                callback.onResult(AutofillAssistantModule.getImpl());
                return;
            }
            // Show inforbar to ask user if they want to retry or cancel.
            ui.showInstallFailureUi();
        });
    }
}

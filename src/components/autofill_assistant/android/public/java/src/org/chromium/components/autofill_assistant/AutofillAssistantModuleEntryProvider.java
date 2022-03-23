// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import androidx.annotation.Nullable;

import org.chromium.base.BundleUtils;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.SysUtils;
import org.chromium.components.autofill_assistant.metrics.FeatureModuleInstallation;

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
    public static final AutofillAssistantModuleEntryProvider INSTANCE =
            new AutofillAssistantModuleEntryProvider();

    boolean isInstalled() {
        return AutofillAssistantModule.isInstalled();
    }

    /* Returns the AA module entry, if it is already installed. */
    @Nullable
    /* package */
    public AutofillAssistantModuleEntry getModuleEntryIfInstalled() {
        if (AutofillAssistantModule.isInstalled()) {
            return AutofillAssistantModule.getImpl();
        }
        return null;
    }

    /** Gets the AA module entry, installing it if necessary. */
    /* package */
    public void getModuleEntry(Callback<AutofillAssistantModuleEntry> callback,
            AssistantModuleInstallUi.Provider moduleInstallUiProvider, boolean showUi) {
        AutofillAssistantModuleEntry entry = getModuleEntryIfInstalled();
        if (entry != null) {
            callback.onResult(entry);
            return;
        }
        loadDynamicModule(callback, moduleInstallUiProvider, showUi);
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
        boolean isNotHighEndDiskDevice = !SysUtils.isHighEndDiskDevice();
        if (isNotBundle || isInstalled || isNotHighEndDiskDevice) {
            Log.v(TAG,
                    "Deferred install not triggered: not_bundle=" + isNotBundle
                            + ", already_installed=" + isInstalled
                            + ", not_high_end_device=" + isNotHighEndDiskDevice);
            return;
        }
        Log.v(TAG, "Deferred install triggered.");
        AutofillAssistantMetrics.recordFeatureModuleInstallation(
                FeatureModuleInstallation.DFM_BACKGROUND_INSTALLATION_REQUESTED);
        AutofillAssistantModule.installDeferred();
    }

    private static void loadDynamicModule(Callback<AutofillAssistantModuleEntry> callback,
            AssistantModuleInstallUi.Provider moduleInstallUiProvider, boolean showUi) {
        AssistantModuleInstallUi ui = moduleInstallUiProvider.create((Boolean retry) -> {
            if (retry) {
                loadDynamicModule(callback, moduleInstallUiProvider, showUi);
            } else {
                callback.onResult(null);
            }
        });

        if (showUi) {
            // Shows toast informing user about install start.
            ui.showInstallStartUi();
        }

        AutofillAssistantModule.install((success) -> {
            if (success) {
                // Don't show success UI from DFM, transition to Autofill Assistant UI directly.
                callback.onResult(AutofillAssistantModule.getImpl());
                return;
            } else if (showUi) {
                // Show inforbar to ask user if they want to retry or cancel.
                ui.showInstallFailureUi();
            } else {
                callback.onResult(null);
            }
        });
    }
}

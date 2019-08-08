// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.PendingIntent;
import android.content.Intent;
import android.os.Bundle;
import android.support.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.metrics.DropOutReason;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.modules.ModuleInstallUi;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.module_installer.ModuleInstaller;

import java.net.URLDecoder;
import java.util.HashMap;
import java.util.Map;

/** Facade for starting Autofill Assistant on a custom tab. */
public class AutofillAssistantFacade {
    /**
     * Prefix for Intent extras relevant to this feature.
     *
     * <p>Intent starting with this prefix are reported to the controller as parameters, except for
     * the ones starting with {@code INTENT_SPECIAL_PREFIX}.
     */
    private static final String INTENT_EXTRA_PREFIX =
            "org.chromium.chrome.browser.autofill_assistant.";

    /** Prefix for intent extras which are not parameters. */
    private static final String INTENT_SPECIAL_PREFIX = INTENT_EXTRA_PREFIX + "special.";

    /** Special parameter that enables the feature. */
    private static final String PARAMETER_ENABLED = "ENABLED";

    /**
     * Identifier used by parameters/or special intent that indicates experiments passed from
     * the caller.
     */
    private static final String EXPERIMENT_IDS_IDENTIFIER = "EXPERIMENT_IDS";

    /**
     * Boolean parameter that trusted apps can use to declare that the user has agreed to Terms and
     * Conditions that cover the use of Autofill Assistant in Chrome for that specific invocation.
     */
    private static final String AGREED_TO_TC = "AGREED_TO_TC";

    /** Pending intent sent by first-party apps. */
    private static final String PENDING_INTENT_NAME = INTENT_SPECIAL_PREFIX + "PENDING_INTENT";

    /** Intent extra name for csv list of experiment ids. */
    private static final String EXPERIMENT_IDS_NAME =
            INTENT_SPECIAL_PREFIX + EXPERIMENT_IDS_IDENTIFIER;

    /** Package names of trusted first-party apps, from the pending intent. */
    private static final String[] TRUSTED_CALLER_PACKAGES = {
            "com.google.android.googlequicksearchbox", // GSA
    };

    /**
     * Synthetic field trial names and group names should match those specified in
     * google3/analysis/uma/dashboards/
     * .../variations/generate_server_hashes.py and
     * .../website/components/variations_dash/variations_histogram_entry.js.
     */
    private static final String SYNTHETIC_TRIAL = "AutofillAssistantTriggered";
    private static final String ENABLED_GROUP = "Enabled";

    /** Returns true if conditions are satisfied to attempt to start Autofill Assistant. */
    public static boolean isConfigured(@Nullable Bundle intentExtras) {
        return getBooleanParameter(intentExtras, PARAMETER_ENABLED);
    }

    /** Starts Autofill Assistant on the given {@code activity}. */
    public static void start(ChromeActivity activity) {
        // Register synthetic trial as soon as possible.
        UmaSessionStats.registerSyntheticFieldTrial(SYNTHETIC_TRIAL, ENABLED_GROUP);

        // Early exit if autofill assistant should not be triggered.
        if (!canStart(activity.getInitialIntent())
                && !AutofillAssistantPreferencesUtil.getShowOnboarding()) {
            return;
        }

        // Have an "attempted starts" baseline for the drop out histogram.
        AutofillAssistantMetrics.recordDropOut(DropOutReason.AA_START);
        checkAndLoadDynamicModuleIfNeeded(activity, (success) -> {
            if (success) {
                if (canStart(activity.getInitialIntent())) {
                    getTab(activity, tab -> startNow(activity, tab));
                    return;
                }
                if (AutofillAssistantPreferencesUtil.getShowOnboarding()) {
                    getTab(activity, tab -> {
                        AutofillAssistantClient client =
                                AutofillAssistantClient.fromWebContents(tab.getWebContents());
                        client.showOnboarding(
                                getExperimentIds(activity.getInitialIntent().getExtras()),
                                () -> startNow(activity, tab));
                    });
                    return;
                }
            } else {
                AutofillAssistantMetrics.recordDropOut(DropOutReason.DFM_CANCELLED);
            }
        });
    }

    /**
     * In M74 experiment ids might come from parameters. This function merges both exp ids from
     * special intent and parameters.
     * @return Comma-separated list of active experiment ids.
     */
    private static String getExperimentIds(@Nullable Bundle bundleExtras) {
        if (bundleExtras == null) {
            return "";
        }

        StringBuilder experiments = new StringBuilder();
        Map<String, String> parameters = extractParameters(bundleExtras);
        if (parameters.containsKey(EXPERIMENT_IDS_IDENTIFIER)) {
            experiments.append(parameters.get(EXPERIMENT_IDS_IDENTIFIER));
        }

        String experimentsFromIntent = IntentUtils.safeGetString(bundleExtras, EXPERIMENT_IDS_NAME);
        if (experimentsFromIntent != null) {
            if (experiments.length() > 0 && !experiments.toString().endsWith(",")) {
                experiments.append(",");
            }
            experiments.append(experimentsFromIntent);
        }
        return experiments.toString();
    }

    /**
     * Checks if classes from DFM can be loaded, if not try loading it with default UI provided
     * by DFM.
     * @param callback is called with 'true' when DFM is already loaded or loading DFM was
     *        successful, it is called with 'false' if DFM cannot be loaded and aborted by user.
     */
    private static void checkAndLoadDynamicModuleIfNeeded(
            ChromeActivity activity, Callback<Boolean> callback) {
        if (AutofillAssistantModule.isInstalled()) {
            callback.onResult(true);
            return;
        }
        getTab(activity, tab -> { loadDynamicModuleWithUi(tab, callback); });
    }

    private static void loadDynamicModuleWithUi(Tab tab, Callback<Boolean> callback) {
        ModuleInstallUi ui = new ModuleInstallUi(tab, R.string.autofill_assistant_module_title,
                new ModuleInstallUi.FailureUiListener() {
                    @Override
                    public void onRetry() {
                        loadDynamicModuleWithUi(tab, callback);
                    }

                    @Override
                    public void onCancel() {
                        callback.onResult(false);
                    }
                });
        // Shows toast informing user about install start.
        ui.showInstallStartUi();
        ModuleInstaller.install("autofill_assistant", (success) -> {
            if (success) {
                // Don't show success UI from DFM, transition to autobot UI directly.
                callback.onResult(true);
                return;
            }
            // Show inforbar to ask user if they want to retry or cancel.
            ui.showInstallFailureUi();
        });
    }

    private static void startNow(ChromeActivity activity, Tab tab) {
        Bundle bundleExtras = activity.getInitialIntent().getExtras();
        Map<String, String> parameters = extractParameters(bundleExtras);
        parameters.remove(PARAMETER_ENABLED);
        String initialUrl = activity.getInitialIntent().getDataString();

        AutofillAssistantClient client =
                AutofillAssistantClient.fromWebContents(tab.getWebContents());

        client.start(initialUrl, parameters, getExperimentIds(bundleExtras),
                activity.getInitialIntent().getExtras());
    }

    private static void getTab(ChromeActivity activity, Callback<Tab> callback) {
        if (activity.getActivityTab() != null
                && activity.getActivityTab().getWebContents() != null) {
            callback.onResult(activity.getActivityTab());
            return;
        }

        // The tab is not yet available. We need to register as listener and wait for it.
        activity.getActivityTabProvider().addObserverAndTrigger(
                new ActivityTabProvider.HintlessActivityTabObserver() {
                    @Override
                    public void onActivityTabChanged(Tab tab) {
                        if (tab == null) return;
                        activity.getActivityTabProvider().removeObserver(this);
                        assert tab.getWebContents() != null;
                        callback.onResult(tab);
                    }
                });
    }

    /** Return the value if the given boolean parameter from the extras. */
    private static boolean getBooleanParameter(@Nullable Bundle extras, String parameterName) {
        return extras != null
                && IntentUtils.safeGetBoolean(extras, INTENT_EXTRA_PREFIX + parameterName, false);
    }

    /** Returns a map containing the extras starting with {@link #INTENT_EXTRA_PREFIX}. */
    private static Map<String, String> extractParameters(@Nullable Bundle extras) {
        Map<String, String> result = new HashMap<>();
        if (extras != null) {
            for (String key : extras.keySet()) {
                try {
                    if (key.startsWith(INTENT_EXTRA_PREFIX)
                            && !key.startsWith(INTENT_SPECIAL_PREFIX)) {
                        result.put(key.substring(INTENT_EXTRA_PREFIX.length()),
                                URLDecoder.decode(extras.get(key).toString(), "UTF-8"));
                    }
                } catch (java.io.UnsupportedEncodingException e) {
                    throw new IllegalStateException("UTF-8 encoding not available.", e);
                }
            }
        }
        return result;
    }

    /** Returns {@code true} if we can start right away. */
    private static boolean canStart(Intent intent) {
        return (AutofillAssistantPreferencesUtil.isAutofillAssistantSwitchOn()
                       && !AutofillAssistantPreferencesUtil.getShowOnboarding())
                || hasAgreedToTc(intent);
    }

    /**
     * Returns {@code true} if the user has already agreed to specific terms and conditions for the
     * current task, that cover the use of autofill assistant. There's no need to show the generic
     * first-time screen for that call.
     */
    private static boolean hasAgreedToTc(Intent intent) {
        return getBooleanParameter(intent.getExtras(), AGREED_TO_TC)
                && callerIsOnWhitelist(intent, TRUSTED_CALLER_PACKAGES);
    }

    /** Returns {@code true} if the caller is on the given whitelist. */
    private static boolean callerIsOnWhitelist(Intent intent, String[] whitelist) {
        PendingIntent pendingIntent =
                IntentUtils.safeGetParcelableExtra(intent, PENDING_INTENT_NAME);
        if (pendingIntent == null) {
            return false;
        }
        String packageName = pendingIntent.getCreatorPackage();
        for (String whitelistedPackage : whitelist) {
            if (whitelistedPackage.equals(packageName)) {
                return true;
            }
        }
        return false;
    }
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.metrics.DropOutReason;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.directactions.DirectActionHandler;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;

/** Facade for starting Autofill Assistant on a custom tab. */
public class AutofillAssistantFacade {
    /**
     * Synthetic field trial names and group names should match those specified in
     * google3/analysis/uma/dashboards/
     * .../variations/generate_server_hashes.py and
     * .../website/components/variations_dash/variations_histogram_entry.js.
     */
    private static final String TRIGGERED_SYNTHETIC_TRIAL = "AutofillAssistantTriggered";
    private static final String ENABLED_GROUP = "Enabled";

    private static final String EXPERIMENTS_SYNTHETIC_TRIAL = "AutofillAssistantExperimentsTrial";

    /** Returns true if conditions are satisfied to attempt to start Autofill Assistant. */
    private static boolean isConfigured(AutofillAssistantArguments arguments) {
        return arguments.isEnabled();
    }

    /**
     * Starts Autofill Assistant.
     * @param activity {@link ChromeActivity} the activity on which the Autofill Assistant is being
     *         started. This must be a launch activity holding the correct intent for starting.
     */
    public static void start(ChromeActivity activity) {
        start(activity,
                AutofillAssistantArguments.newBuilder()
                        .fromBundle(activity.getInitialIntent().getExtras())
                        .withInitialUrl(activity.getInitialIntent().getDataString())
                        .build());
    }

    /**
     * Starts Autofill Assistant.
     * @param activity {@link ChromeActivity} the activity on which the Autofill Assistant is being
     *         started.
     * @param bundleExtras {@link Bundle} the extras which were used to start the Autofill
     *         Assistant.
     * @param initialUrl the initial URL the Autofill Assistant should be started on.
     */
    public static void start(
            ChromeActivity activity, @Nullable Bundle bundleExtras, String initialUrl) {
        start(activity,
                AutofillAssistantArguments.newBuilder()
                        .fromBundle(bundleExtras)
                        .withInitialUrl(initialUrl)
                        .build());
    }

    /**
     * Starts Autofill Assistant.
     * @param activity {@link ChromeActivity} the activity on which the Autofill Assistant is being
     *         started.
     * @param arguments {@link AutofillAssistantArguments} the arguments which were used to start
     *          the Autofill Assistant.
     */
    public static void start(ChromeActivity activity, AutofillAssistantArguments arguments) {
        // Register synthetic trial as soon as possible.
        UmaSessionStats.registerSyntheticFieldTrial(TRIGGERED_SYNTHETIC_TRIAL, ENABLED_GROUP);
        // Synthetic trial for experiments.
        String experimentIds = arguments.getExperimentIds();
        if (!experimentIds.isEmpty()) {
            for (String experimentId : experimentIds.split(",")) {
                UmaSessionStats.registerSyntheticFieldTrial(
                        EXPERIMENTS_SYNTHETIC_TRIAL, experimentId);
            }
        }

        // Have an "attempted starts" baseline for the drop out histogram.
        AutofillAssistantMetrics.recordDropOut(DropOutReason.AA_START);
        waitForTabWithWebContents(activity, tab -> {
            AutofillAssistantModuleEntryProvider.INSTANCE.getModuleEntry(
                    tab, (moduleEntry) -> {
                        if (moduleEntry == null) {
                            AutofillAssistantMetrics.recordDropOut(
                                    DropOutReason.DFM_INSTALL_FAILED);
                            return;
                        }

                        moduleEntry.start(activity.getBottomSheetController(),
                                activity.getFullscreenManager(), activity.getCompositorViewHolder(),
                                activity.getScrim(), activity, tab.getWebContents(),
                                !AutofillAssistantPreferencesUtil.getShowOnboarding(),
                                activity instanceof CustomTabActivity, arguments.getInitialUrl(),
                                arguments.getParameters(), arguments.getExperimentIds(),
                                arguments.getCallerAccount(), arguments.getUserName());
                    });
        });
    }

    /**
     * Checks whether direct actions provided by Autofill Assistant should be available - assuming
     * that direct actions are available at all.
     */
    public static boolean areDirectActionsAvailable(@ActivityType int activityType) {
        return BuildInfo.isAtLeastQ()
                && (activityType == ActivityType.CUSTOM_TAB || activityType == ActivityType.TABBED)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ASSISTANT_DIRECT_ACTIONS)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ASSISTANT);
    }

    /**
     * Returns a {@link DirectActionHandler} for making dynamic actions available under Android Q.
     *
     * <p>This should only be called if {@link #areDirectActionsAvailable} returns true. This method
     * can also return null if autofill assistant is not available for some other reasons.
     */
    public static DirectActionHandler createDirectActionHandler(Context context,
            BottomSheetController bottomSheetController, ChromeFullscreenManager fullscreenManager,
            CompositorViewHolder compositorViewHolder, ActivityTabProvider activityTabProvider,
            ScrimView scrimView) {
        return new AutofillAssistantDirectActionHandler(context, bottomSheetController,
                fullscreenManager, compositorViewHolder, activityTabProvider, scrimView,
                AutofillAssistantModuleEntryProvider.INSTANCE);
    }

    /** Provides the callback with a tab that has a web contents, waits if necessary. */
    private static void waitForTabWithWebContents(ChromeActivity activity, Callback<Tab> callback) {
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

    public static boolean isAutofillAssistantEnabled(Intent intent) {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ASSISTANT)
                && AutofillAssistantFacade.isConfigured(AutofillAssistantArguments.newBuilder()
                                                                .fromBundle(intent.getExtras())
                                                                .build());
    }

    public static boolean isAutofillAssistantByIntentTriggeringEnabled(Intent intent) {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ASSISTANT_CHROME_ENTRY)
                && AutofillAssistantFacade.isAutofillAssistantEnabled(intent);
    }
}

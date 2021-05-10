// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.autofill_assistant.onboarding.AssistantOnboardingResult;
import org.chromium.chrome.browser.autofill_assistant.onboarding.BaseOnboardingCoordinator;
import org.chromium.chrome.browser.autofill_assistant.onboarding.OnboardingCoordinatorFactory;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.content_public.browser.WebContents;

import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A handler that provides Autofill Assistant actions for a specific activity.
 */
class AutofillAssistantActionHandlerImpl implements AutofillAssistantActionHandler {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final BrowserControlsStateProvider mBrowserControls;
    private final CompositorViewHolder mCompositorViewHolder;
    private final ActivityTabProvider mActivityTabProvider;
    private final ScrimCoordinator mScrim;

    AutofillAssistantActionHandlerImpl(Context context, BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, CompositorViewHolder compositorViewHolder,
            ActivityTabProvider activityTabProvider, ScrimCoordinator scrim) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mBrowserControls = browserControls;
        mCompositorViewHolder = compositorViewHolder;
        mActivityTabProvider = activityTabProvider;
        mScrim = scrim;
    }

    @Override
    public void fetchWebsiteActions(
            String userName, String experimentIds, Bundle arguments, Callback<Boolean> callback) {
        if (!AutofillAssistantPreferencesUtil.isAutofillOnboardingAccepted()) {
            callback.onResult(false);
            return;
        }
        AutofillAssistantClient client = getOrCreateClient();
        if (client == null) {
            callback.onResult(false);
            return;
        }

        client.fetchWebsiteActions(userName, experimentIds, toArgumentMap(arguments), callback);
    }

    @Override
    public boolean hasRunFirstCheck() {
        if (!AutofillAssistantPreferencesUtil.isAutofillOnboardingAccepted()) return false;
        AutofillAssistantClient client = getOrCreateClient();
        if (client == null) return false;
        return client.hasRunFirstCheck();
    }

    @Override
    public List<AutofillAssistantDirectAction> getActions() {
        AutofillAssistantClient client = getOrCreateClient();
        if (client == null) {
            return Collections.emptyList();
        }
        return client.getDirectActions();
    }

    @Override
    public void performOnboarding(
            String experimentIds, Bundle arguments, Callback<Boolean> callback) {
        Map<String, String> parameters = toArgumentMap(arguments);
        BaseOnboardingCoordinator coordinator =
                OnboardingCoordinatorFactory.createBottomSheetOnboardingCoordinator(experimentIds,
                        parameters, mContext, mBottomSheetController, mBrowserControls,
                        mCompositorViewHolder);
        coordinator.show(result -> {
            coordinator.hide();
            callback.onResult(result == AssistantOnboardingResult.ACCEPTED);
        });
    }

    @Override
    public void performAction(
            String name, String experimentIds, Bundle arguments, Callback<Boolean> callback) {
        AutofillAssistantClient client = getOrCreateClient();
        if (client == null) {
            callback.onResult(false);
            return;
        }

        Map<String, String> argumentMap = toArgumentMap(arguments);
        Callback<BaseOnboardingCoordinator> afterOnboarding = (onboardingCoordinator) -> {
            callback.onResult(client.performDirectAction(
                    name, experimentIds, argumentMap, onboardingCoordinator));
        };

        if (!AutofillAssistantPreferencesUtil.isAutofillOnboardingAccepted()) {
            BaseOnboardingCoordinator coordinator =
                    OnboardingCoordinatorFactory.createBottomSheetOnboardingCoordinator(
                            experimentIds, argumentMap, mContext, mBottomSheetController,
                            mBrowserControls, mCompositorViewHolder);
            coordinator.show(result -> {
                if (result != AssistantOnboardingResult.ACCEPTED) {
                    coordinator.hide();
                    callback.onResult(false);
                    return;
                }
                afterOnboarding.onResult(coordinator);
            });
            return;
        }
        afterOnboarding.onResult(null);
    }

    private WebContents getWebContents() {
        Tab tab = mActivityTabProvider.get();
        if (tab == null) {
            return null;
        }
        return tab.getWebContents();
    }

    /**
     * Returns a client for the current tab or {@code null} if there's no current tab or the current
     * tab doesn't have an associated browser content.
     */
    @Nullable
    private AutofillAssistantClient getOrCreateClient() {
        ThreadUtils.assertOnUiThread();
        WebContents webContents = getWebContents();
        if (webContents == null) {
            return null;
        }
        return AutofillAssistantClient.fromWebContents(webContents);
    }

    /** Extracts string arguments from a bundle. */
    private Map<String, String> toArgumentMap(Bundle bundle) {
        Map<String, String> map = new HashMap<>();
        for (String key : bundle.keySet()) {
            String value = bundle.getString(key);
            if (value != null) {
                map.put(key, value);
            }
        }
        return map;
    }
}

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.Callback;

import java.util.Map;

/**
 * Helper interface for showing and hiding the onboarding.
 */
public interface AssistantOnboardingHelper {
    /**
     * Displays the onboarding to the user.
     *
     * @param useDialogOnboarding whether to show the dialog or bottom-sheet onboarding.
     * @param experimentIds the list of active experiment ids.
     * @param parameters the key/value map of script parameters use.
     * @param callback the callback to invoke with the {@code OnboardingResult}.
     */
    void showOnboarding(boolean useDialogOnboarding, String experimentIds,
            Map<String, String> parameters, Callback<Integer> callback);

    /**
     * Hides the onboarding, if currently shown. Does not invoke the callback that was associated
     * with {@code showOnboarding}.
     */
    void hideOnboarding();
}

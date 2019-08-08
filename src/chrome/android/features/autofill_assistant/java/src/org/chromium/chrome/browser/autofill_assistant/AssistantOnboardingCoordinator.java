// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.metrics.OnBoarding;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.Arrays;

/**
 * Coordinator responsible for showing the onboarding screen when the user is using the Autofill
 * Assistant for the first time.
 */
class AssistantOnboardingCoordinator {
    private static final String SMALL_ONBOARDING_EXPERIMENT_ID = "4257013";

    /**
     * Shows the onboarding screen and returns whether we should proceed.
     */
    static View show(
            String experimentIds, Context context, ViewGroup root, Callback<Boolean> callback) {
        AutofillAssistantMetrics.recordOnBoarding(OnBoarding.OB_SHOWN);

        View initView = LayoutInflater.from(context)
                                .inflate(R.layout.autofill_assistant_onboarding, root)
                                .findViewById(R.id.assistant_onboarding);

        TextView termsTextView = initView.findViewById(R.id.google_terms_message);
        String termsString = context.getApplicationContext().getString(
                R.string.autofill_assistant_google_terms_description);

        NoUnderlineClickableSpan termsSpan = new NoUnderlineClickableSpan(context.getResources(),
                (widget)
                        -> CustomTabActivity.showInfoPage(context.getApplicationContext(),
                                context.getApplicationContext().getString(
                                        R.string.autofill_assistant_google_terms_url)));
        SpannableString spannableMessage = SpanApplier.applySpans(
                termsString, new SpanApplier.SpanInfo("<link>", "</link>", termsSpan));
        termsTextView.setText(spannableMessage);
        termsTextView.setMovementMethod(LinkMovementMethod.getInstance());

        // Set focusable for accessibility.
        initView.setFocusable(true);

        initView.findViewById(R.id.button_init_ok)
                .setOnClickListener(unusedView -> onClicked(true, root, initView, callback));
        initView.findViewById(R.id.button_init_not_ok)
                .setOnClickListener(unusedView -> onClicked(false, root, initView, callback));
        initView.announceForAccessibility(
                context.getString(R.string.autofill_assistant_first_run_accessibility));

        // Hide views that should not be displayed when showing the small onboarding.
        if (Arrays.asList(experimentIds.split(",")).contains(SMALL_ONBOARDING_EXPERIMENT_ID)) {
            hide(initView, R.id.onboarding_image);
            hide(initView, R.id.onboarding_subtitle);
            hide(initView, R.id.onboarding_separator);
        }

        return initView;
    }

    private static void hide(View root, int resId) {
        root.findViewById(resId).setVisibility(View.GONE);
    }

    private static void onClicked(
            boolean accept, ViewGroup root, View initView, Callback<Boolean> callback) {
        AutofillAssistantPreferencesUtil.setInitialPreferences(accept);
        root.removeView(initView);
        if (accept) {
            AutofillAssistantMetrics.recordOnBoarding(OnBoarding.OB_ACCEPTED);
        } else {
            AutofillAssistantMetrics.recordOnBoarding(OnBoarding.OB_CANCELLED);
        }
        callback.onResult(accept);
    }
}

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Settings fragment for FLoC settings as part of Privacy Sandbox.
 */
public class FlocSettingsFragment extends PreferenceFragmentCompat
        implements Preference.OnPreferenceChangeListener, Preference.OnPreferenceClickListener {
    public static final String FLOC_REGIONS_DEFAULT_URL =
            "https://www.privacysandbox.com/proposals/floc";
    public static final String EXPERIMENT_URL_PARAM = "floc-website-url";

    public static final String FLOC_DESCRIPTION = "floc_description";
    public static final String FLOC_TOGGLE = "floc_toggle";
    public static final String FLOC_STATUS = "floc_status";
    public static final String FLOC_GROUP = "floc_group";
    public static final String FLOC_UPDATE = "floc_update";
    public static final String RESET_FLOC_EXPLANATION = "reset_floc_explanation";
    public static final String RESET_FLOC_BUTTON = "reset_floc_button";

    private PrivacySandboxHelpers.CustomTabIntentHelper mCustomTabHelper;
    private PrivacySandboxHelpers.TrustedIntentHelper mTrustedIntentHelper;
    private ButtonCompat mResetButton;

    /**
     * Initializes all the objects related to the preferences page.
     */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        // Add all preferences and set the title.
        getActivity().setTitle(R.string.prefs_privacy_sandbox_floc);
        SettingsUtils.addPreferencesFromResource(this, R.xml.floc_preferences);
        findPreference(FLOC_DESCRIPTION)
                .setSummary(SpanApplier.applySpans(PrivacySandboxBridge.getFlocDescriptionString()
                                + " "
                                + getContext().getString(R.string.privacy_sandbox_floc_description),
                        new SpanInfo("<link>", "</link>",
                                new NoUnderlineClickableSpan(getContext().getResources(),
                                        (widget) -> openUrlInCct(getFlocRegionsUrl())))));
        findPreference(RESET_FLOC_EXPLANATION)
                .setSummary(PrivacySandboxBridge.getFlocResetExplanationString());
        // Configure the toggle.
        ChromeSwitchPreference flocToggle = (ChromeSwitchPreference) findPreference(FLOC_TOGGLE);
        flocToggle.setOnPreferenceChangeListener(this);
        flocToggle.setChecked(PrivacySandboxBridge.isFlocEnabled());
        // Configure the reset button.
        Preference resetButton = findPreference(RESET_FLOC_BUTTON);
        resetButton.setOnPreferenceClickListener(this);
        resetButton.setTitle(R.string.privacy_sandbox_floc_reset_button);

        RecordUserAction.record("Settings.PrivacySandbox.FlocSubpageOpened");

        updateInformation();
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        if (!FLOC_TOGGLE.equals(key)) return true;
        boolean enabled = (boolean) newValue;
        PrivacySandboxBridge.setFlocEnabled(enabled);
        updateInformation();
        return true;
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        String key = preference.getKey();
        if (!RESET_FLOC_BUTTON.equals(key)) return true;
        PrivacySandboxBridge.resetFlocId();
        updateInformation();
        return true;
    }

    /**
     * Set the necessary CCT helpers to be able to natively open links. This is needed because the
     * helpers are not modularized.
     */
    public void setCctHelpers(PrivacySandboxHelpers.CustomTabIntentHelper tabHelper,
            PrivacySandboxHelpers.TrustedIntentHelper intentHelper) {
        mCustomTabHelper = tabHelper;
        mTrustedIntentHelper = intentHelper;
    }

    private void updateInformation() {
        findPreference(RESET_FLOC_BUTTON).setEnabled(PrivacySandboxBridge.isFlocIdResettable());

        findPreference(FLOC_STATUS)
                .setSummary(getContext().getString(R.string.privacy_sandbox_floc_status_title)
                        + "\n" + PrivacySandboxBridge.getFlocStatusString());
        findPreference(FLOC_GROUP)
                .setSummary(getContext().getString(R.string.privacy_sandbox_floc_group_title) + "\n"
                        + PrivacySandboxBridge.getFlocGroupString());
        findPreference(FLOC_UPDATE)
                .setSummary(getContext().getString(R.string.privacy_sandbox_floc_update_title)
                        + "\n" + PrivacySandboxBridge.getFlocUpdateString());
    }

    private String getFlocRegionsUrl() {
        // Get the URL from Finch, if defined.
        String url = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_2, EXPERIMENT_URL_PARAM);
        if (url == null || url.isEmpty()) return FLOC_REGIONS_DEFAULT_URL;
        return url;
    }

    private void openUrlInCct(String url) {
        assert (mCustomTabHelper != null)
                && (mTrustedIntentHelper != null)
            : "CCT helpers must be set on PrivacySandboxSettingsFragment before opening a link.";
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder().setShowTitle(true).build();
        customTabIntent.intent.setData(Uri.parse(url));
        Intent intent = mCustomTabHelper.createCustomTabActivityIntent(
                getContext(), customTabIntent.intent);
        intent.setPackage(getContext().getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, getContext().getPackageName());
        mTrustedIntentHelper.addTrustedIntentExtras(intent);
        IntentUtils.safeStartActivity(getContext(), intent);
    }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceGroup;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.password_manager.settings.PasswordUIView;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.signin.UnifiedConsentServiceBridge;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Settings fragment to enable Sync and other services that communicate with Google.
 */
public class GoogleServicesSettings
        extends PreferenceFragmentCompat implements Preference.OnPreferenceChangeListener {
    private static final String PREF_SEARCH_SUGGESTIONS = "search_suggestions";
    private static final String PREF_NAVIGATION_ERROR = "navigation_error";
    private static final String PREF_SAFE_BROWSING = "safe_browsing";
    private static final String PREF_PASSWORD_LEAK_DETECTION = "password_leak_detection";
    private static final String PREF_SAFE_BROWSING_SCOUT_REPORTING =
            "safe_browsing_scout_reporting";
    private static final String PREF_USAGE_AND_CRASH_REPORTING = "usage_and_crash_reports";
    private static final String PREF_URL_KEYED_ANONYMIZED_DATA = "url_keyed_anonymized_data";
    private static final String PREF_CONTEXTUAL_SEARCH = "contextual_search";
    private static final String PREF_AUTOFILL_ASSISTANT = "autofill_assistant";

    private final PrefServiceBridge mPrefServiceBridge = PrefServiceBridge.getInstance();
    private final PrivacyPreferencesManager mPrivacyPrefManager =
            PrivacyPreferencesManager.getInstance();
    private final ManagedPreferenceDelegate mManagedPreferenceDelegate =
            createManagedPreferenceDelegate();
    private final SharedPreferencesManager mSharedPreferencesManager =
            SharedPreferencesManager.getInstance();

    private ChromeSwitchPreference mSearchSuggestions;
    private ChromeSwitchPreference mNavigationError;
    private ChromeSwitchPreference mSafeBrowsing;
    private ChromeSwitchPreference mPasswordLeakDetection;
    private ChromeSwitchPreference mSafeBrowsingReporting;
    private ChromeSwitchPreference mUsageAndCrashReporting;
    private ChromeSwitchPreference mUrlKeyedAnonymizedData;
    private @Nullable ChromeSwitchPreference mAutofillAssistant;
    private @Nullable Preference mContextualSearch;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, String rootKey) {
        mPrivacyPrefManager.migrateNetworkPredictionPreferences();

        getActivity().setTitle(R.string.prefs_google_services);
        setHasOptionsMenu(true);

        SettingsUtils.addPreferencesFromResource(this, R.xml.google_services_preferences);

        mSearchSuggestions = (ChromeSwitchPreference) findPreference(PREF_SEARCH_SUGGESTIONS);
        mSearchSuggestions.setOnPreferenceChangeListener(this);
        mSearchSuggestions.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mNavigationError = (ChromeSwitchPreference) findPreference(PREF_NAVIGATION_ERROR);
        mNavigationError.setOnPreferenceChangeListener(this);
        mNavigationError.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mSafeBrowsing = (ChromeSwitchPreference) findPreference(PREF_SAFE_BROWSING);
        mSafeBrowsing.setOnPreferenceChangeListener(this);
        mSafeBrowsing.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mPasswordLeakDetection =
                (ChromeSwitchPreference) findPreference(PREF_PASSWORD_LEAK_DETECTION);
        mPasswordLeakDetection.setOnPreferenceChangeListener(this);
        mPasswordLeakDetection.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mSafeBrowsingReporting =
                (ChromeSwitchPreference) findPreference(PREF_SAFE_BROWSING_SCOUT_REPORTING);
        mSafeBrowsingReporting.setOnPreferenceChangeListener(this);
        mSafeBrowsingReporting.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mUsageAndCrashReporting =
                (ChromeSwitchPreference) findPreference(PREF_USAGE_AND_CRASH_REPORTING);
        mUsageAndCrashReporting.setOnPreferenceChangeListener(this);
        mUsageAndCrashReporting.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mUrlKeyedAnonymizedData =
                (ChromeSwitchPreference) findPreference(PREF_URL_KEYED_ANONYMIZED_DATA);
        mUrlKeyedAnonymizedData.setOnPreferenceChangeListener(this);
        mUrlKeyedAnonymizedData.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        mAutofillAssistant = (ChromeSwitchPreference) findPreference(PREF_AUTOFILL_ASSISTANT);
        if (shouldShowAutofillAssistantPreference()) {
            mAutofillAssistant.setOnPreferenceChangeListener(this);
            mAutofillAssistant.setManagedPreferenceDelegate(mManagedPreferenceDelegate);
        } else {
            removePreference(getPreferenceScreen(), mAutofillAssistant);
            mAutofillAssistant = null;
        }

        mContextualSearch = findPreference(PREF_CONTEXTUAL_SEARCH);
        if (!ContextualSearchFieldTrial.isEnabled()) {
            removePreference(getPreferenceScreen(), mContextualSearch);
            mContextualSearch = null;
        }
        updatePreferences();
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(R.drawable.ic_help_and_feedback);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            HelpAndFeedback.getInstance().show(getActivity(),
                    getString(R.string.help_context_sync_and_services),
                    Profile.getLastUsedRegularProfile(), null);
            return true;
        }
        return false;
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferences();
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        if (PREF_SEARCH_SUGGESTIONS.equals(key)) {
            mPrefServiceBridge.setBoolean(Pref.SEARCH_SUGGEST_ENABLED, (boolean) newValue);
        } else if (PREF_SAFE_BROWSING.equals(key)) {
            mPrefServiceBridge.setBoolean(Pref.SAFE_BROWSING_ENABLED, (boolean) newValue);
            // Toggling the safe browsing preference impacts the leak detection and the
            // safe browsing reporting preferences as well.
            PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                    this::updateLeakDetectionAndSafeBrowsingReportingPreferences);
        } else if (PREF_PASSWORD_LEAK_DETECTION.equals(key)) {
            mPrefServiceBridge.setBoolean(
                    Pref.PASSWORD_MANAGER_LEAK_DETECTION_ENABLED, (boolean) newValue);
        } else if (PREF_SAFE_BROWSING_SCOUT_REPORTING.equals(key)) {
            SafeBrowsingBridge.setSafeBrowsingExtendedReportingEnabled((boolean) newValue);
        } else if (PREF_NAVIGATION_ERROR.equals(key)) {
            mPrefServiceBridge.setBoolean(Pref.ALTERNATE_ERROR_PAGES_ENABLED, (boolean) newValue);
        } else if (PREF_USAGE_AND_CRASH_REPORTING.equals(key)) {
            UmaSessionStats.changeMetricsReportingConsent((boolean) newValue);
        } else if (PREF_URL_KEYED_ANONYMIZED_DATA.equals(key)) {
            UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                    Profile.getLastUsedRegularProfile(), (boolean) newValue);
        } else if (PREF_AUTOFILL_ASSISTANT.equals(key)) {
            setAutofillAssistantSwitchValue((boolean) newValue);
        }
        return true;
    }

    private static void removePreference(PreferenceGroup from, Preference preference) {
        boolean found = from.removePreference(preference);
        assert found : "Don't have such preference! Preference key: " + preference.getKey();
    }

    private void updatePreferences() {
        mSearchSuggestions.setChecked(mPrefServiceBridge.getBoolean(Pref.SEARCH_SUGGEST_ENABLED));
        mNavigationError.setChecked(
                mPrefServiceBridge.getBoolean(Pref.ALTERNATE_ERROR_PAGES_ENABLED));
        mSafeBrowsing.setChecked(mPrefServiceBridge.getBoolean(Pref.SAFE_BROWSING_ENABLED));

        updateLeakDetectionAndSafeBrowsingReportingPreferences();

        mUsageAndCrashReporting.setChecked(
                mPrivacyPrefManager.isUsageAndCrashReportingPermittedByUser());
        mUrlKeyedAnonymizedData.setChecked(
                UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(
                        Profile.getLastUsedRegularProfile()));

        if (mAutofillAssistant != null) {
            mAutofillAssistant.setChecked(isAutofillAssistantSwitchOn());
        }
        if (mContextualSearch != null) {
            boolean isContextualSearchEnabled =
                    !ContextualSearchManager.isContextualSearchDisabled();
            mContextualSearch.setSummary(
                    isContextualSearchEnabled ? R.string.text_on : R.string.text_off);
        }
    }

    /**
     * If password leak detection is off and cannot be toggled while safe browsing is disabled, so
     * its appearance needs to be updated. The same goes for safe browsing reporting.
     */
    private void updateLeakDetectionAndSafeBrowsingReportingPreferences() {
        boolean safe_browsing_enabled = mPrefServiceBridge.getBoolean(Pref.SAFE_BROWSING_ENABLED);
        mSafeBrowsingReporting.setEnabled(safe_browsing_enabled);
        mSafeBrowsingReporting.setChecked(safe_browsing_enabled
                && SafeBrowsingBridge.isSafeBrowsingExtendedReportingEnabled());

        boolean has_token_for_leak_check = PasswordUIView.hasAccountForLeakCheckRequest();
        boolean leak_detection_enabled =
                mPrefServiceBridge.getBoolean(Pref.PASSWORD_MANAGER_LEAK_DETECTION_ENABLED);
        boolean toggle_enabled = safe_browsing_enabled && has_token_for_leak_check;

        mPasswordLeakDetection.setEnabled(toggle_enabled);
        mPasswordLeakDetection.setChecked(toggle_enabled && leak_detection_enabled);

        if (!safe_browsing_enabled || !leak_detection_enabled || has_token_for_leak_check) {
            mPasswordLeakDetection.setSummary(null);
            return;
        }
        mPasswordLeakDetection.setSummary(
                R.string.passwords_leak_detection_switch_signed_out_enable_description);
    }

    private ChromeManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return preference -> {
            String key = preference.getKey();
            if (PREF_NAVIGATION_ERROR.equals(key)) {
                return mPrefServiceBridge.isManagedPreference(Pref.ALTERNATE_ERROR_PAGES_ENABLED);
            }
            if (PREF_SEARCH_SUGGESTIONS.equals(key)) {
                return mPrefServiceBridge.isManagedPreference(Pref.SEARCH_SUGGEST_ENABLED);
            }
            if (PREF_SAFE_BROWSING_SCOUT_REPORTING.equals(key)) {
                return SafeBrowsingBridge.isSafeBrowsingExtendedReportingManaged();
            }
            if (PREF_SAFE_BROWSING.equals(key)) {
                return mPrefServiceBridge.isManagedPreference(Pref.SAFE_BROWSING_ENABLED);
            }
            if (PREF_PASSWORD_LEAK_DETECTION.equals(key)) {
                return mPrefServiceBridge.isManagedPreference(
                        Pref.PASSWORD_MANAGER_LEAK_DETECTION_ENABLED);
            }
            if (PREF_USAGE_AND_CRASH_REPORTING.equals(key)) {
                return PrivacyPreferencesManager.getInstance().isMetricsReportingManaged();
            }
            if (PREF_URL_KEYED_ANONYMIZED_DATA.equals(key)) {
                return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionManaged(
                        Profile.getLastUsedRegularProfile());
            }
            return false;
        };
    }

    /**
     *  This checks whether Autofill Assistant is enabled and was shown at least once (only then
     *  will the AA switch be assigned a value).
     */
    private boolean shouldShowAutofillAssistantPreference() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ASSISTANT)
                && mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED);
    }

    public boolean isAutofillAssistantSwitchOn() {
        return mSharedPreferencesManager.readBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, false);
    }

    public void setAutofillAssistantSwitchValue(boolean newValue) {
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, newValue);
    }
}

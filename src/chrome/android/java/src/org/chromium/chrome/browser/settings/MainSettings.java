// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.provider.Settings;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.datareduction.settings.DataReductionPreferenceFragment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.offlinepages.prefetch.PrefetchConfiguration;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.settings.SignInPreference;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.tracing.settings.DeveloperSettings;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;

import java.util.HashMap;
import java.util.Map;

/**
 * The main settings screen, shown when the user first opens Settings.
 */
public class MainSettings extends PreferenceFragmentCompat
        implements TemplateUrlService.LoadListener, ProfileSyncService.SyncStateChangedListener,
                   SigninManager.SignInStateObserver {
    public static final String PREF_ACCOUNT_SECTION = "account_section";
    public static final String PREF_SIGN_IN = "sign_in";
    public static final String PREF_SYNC_AND_SERVICES = "sync_and_services";
    public static final String PREF_SEARCH_ENGINE = "search_engine";
    public static final String PREF_PASSWORDS = "passwords";
    public static final String PREF_HOMEPAGE = "homepage";
    public static final String PREF_UI_THEME = "ui_theme";
    public static final String PREF_DATA_REDUCTION = "data_reduction";
    public static final String PREF_NOTIFICATIONS = "notifications";
    public static final String PREF_DOWNLOADS = "downloads";
    public static final String PREF_DEVELOPER = "developer";

    private final ManagedPreferenceDelegate mManagedPreferenceDelegate;
    private final Map<String, Preference> mAllPreferences = new HashMap<>();
    private SignInPreference mSignInPreference;

    public MainSettings() {
        setHasOptionsMenu(true);
        mManagedPreferenceDelegate = createManagedPreferenceDelegate();
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        createPreferences();
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        // Disable animations of preference changes.
        getListView().setItemAnimator(null);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mSignInPreference.onPreferenceFragmentDestroyed();
    }

    @Override
    public void onStart() {
        super.onStart();
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager();
        if (signinManager.isSigninSupported()) {
            signinManager.addSignInStateObserver(this);
            mSignInPreference.registerForUpdates();
        }
        ProfileSyncService syncService = ProfileSyncService.get();
        if (syncService != null) {
            syncService.addSyncStateChangedListener(this);
        }
    }

    @Override
    public void onStop() {
        super.onStop();
        SigninManager signinManager = IdentityServicesProvider.get().getSigninManager();
        if (signinManager.isSigninSupported()) {
            signinManager.removeSignInStateObserver(this);
            mSignInPreference.unregisterForUpdates();
        }
        ProfileSyncService syncService = ProfileSyncService.get();
        if (syncService != null) {
            syncService.removeSyncStateChangedListener(this);
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferences();
    }

    private void createPreferences() {
        SettingsUtils.addPreferencesFromResource(this, R.xml.main_preferences);
        cachePreferences();

        mSignInPreference.setOnStateChangedCallback(this::onSignInPreferenceStateChanged);

        updatePasswordsPreference();

        setManagedPreferenceDelegateForPreference(PREF_SEARCH_ENGINE);
        setManagedPreferenceDelegateForPreference(PREF_DATA_REDUCTION);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            // If we are on Android O+ the Notifications preference should lead to the Android
            // Settings notifications page, not to Chrome's notifications settings page.
            Preference notifications = findPreference(PREF_NOTIFICATIONS);
            notifications.setOnPreferenceClickListener(preference -> {
                Intent intent = new Intent();
                intent.setAction(Settings.ACTION_APP_NOTIFICATION_SETTINGS);
                intent.putExtra(Settings.EXTRA_APP_PACKAGE,
                        ContextUtils.getApplicationContext().getPackageName());
                startActivity(intent);
                // We handle the click so the default action (opening NotificationsPreference)
                // isn't triggered.
                return true;
            });
        } else if (!PrefetchConfiguration.isPrefetchingFlagEnabled()) {
            // The Notifications Preferences page currently contains the Content Suggestions
            // Notifications setting (used only by the Offline Prefetch feature) and an entry to the
            // per-website notification settings page. The latter can be accessed from Site
            // Settings, so we only show the entry to the Notifications Preferences page if the
            // Prefetching feature flag is enabled.
            getPreferenceScreen().removePreference(findPreference(PREF_NOTIFICATIONS));
        }

        if (!TemplateUrlServiceFactory.get().isLoaded()) {
            TemplateUrlServiceFactory.get().registerLoadListener(this);
            TemplateUrlServiceFactory.get().load();
        }

        // This checks whether the flag for Downloads Preferences is enabled.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE)) {
            getPreferenceScreen().removePreference(findPreference(PREF_DOWNLOADS));
        }
    }

    /**
     * Stores all preferences in memory so that, if they needed to be added/removed from the
     * PreferenceScreen, there would be no need to reload them from 'main_preferences.xml'.
     */
    private void cachePreferences() {
        int preferenceCount = getPreferenceScreen().getPreferenceCount();
        for (int index = 0; index < preferenceCount; index++) {
            Preference preference = getPreferenceScreen().getPreference(index);
            mAllPreferences.put(preference.getKey(), preference);
        }
        mSignInPreference = (SignInPreference) mAllPreferences.get(PREF_SIGN_IN);
    }

    private void setManagedPreferenceDelegateForPreference(String key) {
        ChromeBasePreference chromeBasePreference = (ChromeBasePreference) mAllPreferences.get(key);
        chromeBasePreference.setManagedPreferenceDelegate(mManagedPreferenceDelegate);
    }

    private void updatePreferences() {
        if (IdentityServicesProvider.get().getSigninManager().isSigninSupported()) {
            addPreferenceIfAbsent(PREF_SIGN_IN);
        } else {
            removePreferenceIfPresent(PREF_SIGN_IN);
        }

        updateSyncAndServicesPreference();
        updateSearchEnginePreference();

        Preference homepagePref = addPreferenceIfAbsent(PREF_HOMEPAGE);
        setOnOffSummary(homepagePref, HomepageManager.isHomepageEnabled());

        if (NightModeUtils.isNightModeSupported()) {
            addPreferenceIfAbsent(PREF_UI_THEME);
        } else {
            removePreferenceIfPresent(PREF_UI_THEME);
        }

        if (DeveloperSettings.shouldShowDeveloperSettings()) {
            addPreferenceIfAbsent(PREF_DEVELOPER);
        } else {
            removePreferenceIfPresent(PREF_DEVELOPER);
        }

        ChromeBasePreference dataReduction =
                (ChromeBasePreference) findPreference(PREF_DATA_REDUCTION);
        dataReduction.setSummary(DataReductionPreferenceFragment.generateSummary(getResources()));
    }

    private Preference addPreferenceIfAbsent(String key) {
        Preference preference = getPreferenceScreen().findPreference(key);
        if (preference == null) getPreferenceScreen().addPreference(mAllPreferences.get(key));
        return mAllPreferences.get(key);
    }

    private void removePreferenceIfPresent(String key) {
        Preference preference = getPreferenceScreen().findPreference(key);
        if (preference != null) getPreferenceScreen().removePreference(preference);
    }

    private void updateSyncAndServicesPreference() {
        ChromeBasePreference syncAndServices =
                (ChromeBasePreference) findPreference(PREF_SYNC_AND_SERVICES);
        syncAndServices.setIcon(SyncSettingsUtils.getSyncStatusIcon(getActivity()));
        syncAndServices.setSummary(SyncSettingsUtils.getSyncStatusSummary(getActivity()));
    }

    private void updateSearchEnginePreference() {
        if (!TemplateUrlServiceFactory.get().isLoaded()) {
            ChromeBasePreference searchEnginePref =
                    (ChromeBasePreference) findPreference(PREF_SEARCH_ENGINE);
            searchEnginePref.setEnabled(false);
            return;
        }

        String defaultSearchEngineName = null;
        TemplateUrl dseTemplateUrl =
                TemplateUrlServiceFactory.get().getDefaultSearchEngineTemplateUrl();
        if (dseTemplateUrl != null) defaultSearchEngineName = dseTemplateUrl.getShortName();

        Preference searchEnginePreference = findPreference(PREF_SEARCH_ENGINE);
        searchEnginePreference.setEnabled(true);
        searchEnginePreference.setSummary(defaultSearchEngineName);
    }

    private void updatePasswordsPreference() {
        Preference passwordsPreference = findPreference(PREF_PASSWORDS);
        passwordsPreference.setOnPreferenceClickListener(preference -> {
            PasswordManagerLauncher.showPasswordSettings(
                    getActivity(), ManagePasswordsReferrer.CHROME_SETTINGS);
            return true;
        });
    }

    private void setOnOffSummary(Preference pref, boolean isOn) {
        pref.setSummary(isOn ? R.string.text_on : R.string.text_off);
    }

    // SigninManager.SignInStateObserver implementation.
    @Override
    public void onSignedIn() {
        // After signing in or out of a managed account, preferences may change or become enabled
        // or disabled.
        new Handler().post(() -> updatePreferences());
    }

    @Override
    public void onSignedOut() {
        updatePreferences();
    }

    private void onSignInPreferenceStateChanged() {
        // Remove "Account" section header if the personalized sign-in promo is shown.
        if (mSignInPreference.getState() == SignInPreference.State.PERSONALIZED_PROMO) {
            removePreferenceIfPresent(PREF_ACCOUNT_SECTION);
        } else {
            addPreferenceIfAbsent(PREF_ACCOUNT_SECTION);
        }
    }

    // TemplateUrlService.LoadListener implementation.
    @Override
    public void onTemplateUrlServiceLoaded() {
        TemplateUrlServiceFactory.get().unregisterLoadListener(this);
        updateSearchEnginePreference();
    }

    @Override
    public void syncStateChanged() {
        updateSyncAndServicesPreference();
    }

    @VisibleForTesting
    public ManagedPreferenceDelegate getManagedPreferenceDelegateForTest() {
        return mManagedPreferenceDelegate;
    }

    private ManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return new ChromeManagedPreferenceDelegate() {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                if (PREF_DATA_REDUCTION.equals(preference.getKey())) {
                    return DataReductionProxySettings.getInstance().isDataReductionProxyManaged();
                }
                if (PREF_SEARCH_ENGINE.equals(preference.getKey())) {
                    return TemplateUrlServiceFactory.get().isDefaultSearchManaged();
                }
                return false;
            }

            @Override
            public boolean isPreferenceClickDisabledByPolicy(Preference preference) {
                if (PREF_DATA_REDUCTION.equals(preference.getKey())) {
                    DataReductionProxySettings settings = DataReductionProxySettings.getInstance();
                    return settings.isDataReductionProxyManaged()
                            && !settings.isDataReductionProxyEnabled();
                }
                if (PREF_SEARCH_ENGINE.equals(preference.getKey())) {
                    return TemplateUrlServiceFactory.get().isDefaultSearchManaged();
                }
                return isPreferenceControlledByPolicy(preference)
                        || isPreferenceControlledByCustodian(preference);
            }
        };
    }
}

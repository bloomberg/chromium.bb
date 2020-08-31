// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility.settings;

import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.accessibility.FontSizePrefs;
import org.chromium.chrome.browser.accessibility.FontSizePrefs.FontSizePrefsObserver;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.components.browser_ui.settings.ChromeBaseCheckBoxPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Fragment to keep track of all the accessibility related preferences.
 */
public class AccessibilitySettings
        extends PreferenceFragmentCompat implements Preference.OnPreferenceChangeListener {
    static final String PREF_TEXT_SCALE = "text_scale";
    static final String PREF_FORCE_ENABLE_ZOOM = "force_enable_zoom";
    static final String PREF_READER_FOR_ACCESSIBILITY = "reader_for_accessibility";
    static final String PREF_CAPTIONS = "captions";

    private TextScalePreference mTextScalePref;
    private ChromeBaseCheckBoxPreference mForceEnableZoomPref;

    private FontSizePrefs mFontSizePrefs = FontSizePrefs.getInstance();
    private FontSizePrefsObserver mFontSizePrefsObserver = new FontSizePrefsObserver() {
        @Override
        public void onFontScaleFactorChanged(float fontScaleFactor, float userFontScaleFactor) {
            mTextScalePref.updateFontScaleFactors(fontScaleFactor, userFontScaleFactor, true);
        }

        @Override
        public void onForceEnableZoomChanged(boolean enabled) {
            mForceEnableZoomPref.setChecked(enabled);
        }
    };

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        getActivity().setTitle(R.string.prefs_accessibility);
        setDivider(null);
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.accessibility_preferences);

        mTextScalePref = (TextScalePreference) findPreference(PREF_TEXT_SCALE);
        mTextScalePref.setOnPreferenceChangeListener(this);
        mTextScalePref.updateFontScaleFactors(mFontSizePrefs.getFontScaleFactor(),
                mFontSizePrefs.getUserFontScaleFactor(), false);

        mForceEnableZoomPref =
                (ChromeBaseCheckBoxPreference) findPreference(PREF_FORCE_ENABLE_ZOOM);
        mForceEnableZoomPref.setOnPreferenceChangeListener(this);
        mForceEnableZoomPref.setChecked(mFontSizePrefs.getForceEnableZoom());

        ChromeBaseCheckBoxPreference readerForAccessibilityPref =
                (ChromeBaseCheckBoxPreference) findPreference(PREF_READER_FOR_ACCESSIBILITY);
        readerForAccessibilityPref.setChecked(
                PrefServiceBridge.getInstance().getBoolean(Pref.READER_FOR_ACCESSIBILITY_ENABLED));
        readerForAccessibilityPref.setOnPreferenceChangeListener(this);

        ChromeBaseCheckBoxPreference mAccessibilityTabSwitcherPref =
                (ChromeBaseCheckBoxPreference) findPreference(
                        ChromePreferenceKeys.ACCESSIBILITY_TAB_SWITCHER);
        if (AccessibilityUtil.isAccessibilityEnabled()) {
            mAccessibilityTabSwitcherPref.setChecked(
                    SharedPreferencesManager.getInstance().readBoolean(
                            ChromePreferenceKeys.ACCESSIBILITY_TAB_SWITCHER, true));
        } else {
            getPreferenceScreen().removePreference(mAccessibilityTabSwitcherPref);
        }

        Preference captions = findPreference(PREF_CAPTIONS);
        captions.setOnPreferenceClickListener(preference -> {
            Intent intent = new Intent(Settings.ACTION_CAPTIONING_SETTINGS);

            // Open the activity in a new task because the back button on the caption
            // settings page navigates to the previous settings page instead of Chrome.
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(intent);

            return true;
        });
    }

    @Override
    public void onStart() {
        super.onStart();
        mFontSizePrefs.addObserver(mFontSizePrefsObserver);
    }

    @Override
    public void onStop() {
        mFontSizePrefs.removeObserver(mFontSizePrefsObserver);
        super.onStop();
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (PREF_TEXT_SCALE.equals(preference.getKey())) {
            mFontSizePrefs.setUserFontScaleFactor((Float) newValue);
        } else if (PREF_FORCE_ENABLE_ZOOM.equals(preference.getKey())) {
            mFontSizePrefs.setForceEnableZoomFromUser((Boolean) newValue);
        } else if (PREF_READER_FOR_ACCESSIBILITY.equals(preference.getKey())) {
            PrefServiceBridge.getInstance().setBoolean(
                    Pref.READER_FOR_ACCESSIBILITY_ENABLED, (Boolean) newValue);
        }
        return true;
    }
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.developer;

import android.os.Bundle;
import android.support.v7.preference.ListPreference;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceFragmentCompat;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.SettingsUtils;
import org.chromium.chrome.browser.tracing.TracingController;
import org.chromium.chrome.browser.tracing.TracingNotificationManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;

/**
 * Settings fragment that shows options for recording a performance trace.
 */
public class TracingSettings
        extends PreferenceFragmentCompat implements TracingController.Observer {
    static final String NON_DEFAULT_CATEGORY_PREFIX = "disabled-by-default-";

    @VisibleForTesting
    static final String UI_PREF_DEFAULT_CATEGORIES = "default_categories";
    @VisibleForTesting
    static final String UI_PREF_NON_DEFAULT_CATEGORIES = "non_default_categories";
    @VisibleForTesting
    static final String UI_PREF_MODE = "mode";
    @VisibleForTesting
    static final String UI_PREF_START_RECORDING = "start_recording";
    @VisibleForTesting
    static final String UI_PREF_TRACING_STATUS = "tracing_status";

    private static final String PREF_TRACING_CATEGORIES = "tracing_categories";
    private static final String PREF_TRACING_MODE = "tracing_mode";

    // Non-translated strings:
    private static final String MSG_TRACING_TITLE = "Tracing";
    private static final String MSG_PRIVACY_NOTICE =
            "Traces may contain user or site data related to the active browsing session, "
            + "including incognito tabs.";
    private static final String MSG_ACTIVE_SUMMARY =
            "A trace is being recorded. Use the notification to stop and share the result.";
    @VisibleForTesting
    static final String MSG_START = "Record trace";
    @VisibleForTesting
    static final String MSG_ACTIVE = "Recording…";
    private static final String MSG_CATEGORIES_SUMMARY = "%s out of %s enabled";
    private static final String MSG_MODE_RECORD_UNTIL_FULL = "Record until full";
    private static final String MSG_MODE_RECORD_AS_MUCH_AS_POSSIBLE =
            "Record until full (large buffer)";
    private static final String MSG_MODE_RECORD_CONTINUOUSLY = "Record continuously";
    @VisibleForTesting
    static final String MSG_NOTIFICATIONS_DISABLED =
            "Please enable Chrome browser notifications to record a trace.";

    // Ordered map that maps tracing mode string to resource id for its description.
    private static final Map<String, String> TRACING_MODES = createTracingModesMap();

    private Preference mPrefDefaultCategories;
    private Preference mPrefNondefaultCategories;
    private ListPreference mPrefMode;
    private Preference mPrefStartRecording;
    private Preference mPrefTracingStatus;

    /**
     * Type of a tracing category indicating whether it is enabled by default or not.
     */
    @IntDef({CategoryType.DEFAULT, CategoryType.NON_DEFAULT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CategoryType {
        int DEFAULT = 0;
        int NON_DEFAULT = 1;
    }

    private static Map<String, String> createTracingModesMap() {
        Map<String, String> map = new LinkedHashMap<>();
        map.put("record-until-full", MSG_MODE_RECORD_UNTIL_FULL);
        map.put("record-as-much-as-possible", MSG_MODE_RECORD_AS_MUCH_AS_POSSIBLE);
        map.put("record-continuously", MSG_MODE_RECORD_CONTINUOUSLY);
        return map;
    }

    /**
     * @return the current set of all enabled categories, irrespective of their type.
     */
    public static Set<String> getEnabledCategories() {
        Set<String> enabled =
                ContextUtils.getAppSharedPreferences().getStringSet(PREF_TRACING_CATEGORIES, null);
        if (enabled == null) {
            enabled = new HashSet<>();
            // By default, enable all default categories.
            for (String category : TracingController.getInstance().getKnownCategories()) {
                if (getCategoryType(category) == CategoryType.DEFAULT) enabled.add(category);
            }
        }
        return enabled;
    }

    /**
     * Get the set of enabled categories of a given category type.
     *
     * @param type the category type.
     * @return the current set of enabled categories with |type|.
     */
    public static Set<String> getEnabledCategories(@CategoryType int type) {
        Set<String> enabled = new HashSet<>();
        for (String category : getEnabledCategories()) {
            if (type == getCategoryType(category)) {
                enabled.add(category);
            }
        }
        return enabled;
    }

    /**
     * Set the enabled categories of a given category type. The set of enabled categories with
     * different types will not be changed.
     *
     * @param type the category type.
     * @param enabledOfType the set of enabled categories with the given |type|.
     */
    public static void setEnabledCategories(@CategoryType int type, Set<String> enabledOfType) {
        Set<String> enabled = new HashSet<>(enabledOfType);
        for (String category : getEnabledCategories()) {
            if (type != getCategoryType(category)) {
                enabled.add(category);
            }
        }
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putStringSet(PREF_TRACING_CATEGORIES, enabled)
                .apply();
    }

    /**
     * Get the type of a category derived from its name.
     * @param category the name of the category.
     * @return the type of the category.
     */
    public static @CategoryType int getCategoryType(String category) {
        return category.startsWith(NON_DEFAULT_CATEGORY_PREFIX) ? CategoryType.NON_DEFAULT
                                                                : CategoryType.DEFAULT;
    }

    /**
     * @return the current tracing mode stored in the preferences. Either "record-until-full",
     *     "record-as-much-as-possible", or "record-continuously".
     */
    public static String getSelectedTracingMode() {
        return ContextUtils.getAppSharedPreferences().getString(
                PREF_TRACING_MODE, TRACING_MODES.keySet().iterator().next());
    }

    /**
     * Select and store a new tracing mode in the preferences.
     *
     * @param tracingMode the new tracing mode, should be either "record-until-full",
     *     "record-as-much-as-possible", or "record-continuously".
     */
    public static void setSelectedTracingMode(String tracingMode) {
        assert TRACING_MODES.containsKey(tracingMode);
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putString(PREF_TRACING_MODE, tracingMode)
                .apply();
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(MSG_TRACING_TITLE);
        SettingsUtils.addPreferencesFromResource(this, R.xml.tracing_preferences);

        mPrefDefaultCategories = findPreference(UI_PREF_DEFAULT_CATEGORIES);
        mPrefNondefaultCategories = findPreference(UI_PREF_NON_DEFAULT_CATEGORIES);
        mPrefMode = (ListPreference) findPreference(UI_PREF_MODE);
        mPrefStartRecording = findPreference(UI_PREF_START_RECORDING);
        mPrefTracingStatus = findPreference(UI_PREF_TRACING_STATUS);

        mPrefDefaultCategories.getExtras().putInt(
                TracingCategoriesSettings.EXTRA_CATEGORY_TYPE, CategoryType.DEFAULT);

        mPrefNondefaultCategories.getExtras().putInt(
                TracingCategoriesSettings.EXTRA_CATEGORY_TYPE, CategoryType.NON_DEFAULT);

        mPrefMode.setEntryValues(TRACING_MODES.keySet().toArray(new String[TRACING_MODES.size()]));
        String[] descriptions =
                TRACING_MODES.values().toArray(new String[TRACING_MODES.values().size()]);
        mPrefMode.setEntries(descriptions);
        mPrefMode.setOnPreferenceChangeListener((preference, newValue) -> {
            setSelectedTracingMode((String) newValue);
            updatePreferences();
            return true;
        });

        mPrefStartRecording.setOnPreferenceClickListener(preference -> {
            TracingController.getInstance().startRecording();
            updatePreferences();
            return true;
        });
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferences();
        TracingController.getInstance().addObserver(this);
    }

    @Override
    public void onPause() {
        super.onPause();
        TracingController.getInstance().removeObserver(this);
    }

    @Override
    public void onTracingStateChanged(@TracingController.State int state) {
        updatePreferences();
    }

    private void updatePreferences() {
        @TracingController.State
        int state = TracingController.getInstance().getState();
        boolean initialized = state != TracingController.State.INITIALIZING;
        boolean idle = state == TracingController.State.IDLE || !initialized;
        boolean notificationsEnabled = TracingNotificationManager.browserNotificationsEnabled();

        mPrefDefaultCategories.setEnabled(initialized);
        mPrefNondefaultCategories.setEnabled(initialized);
        mPrefMode.setEnabled(initialized);
        mPrefStartRecording.setEnabled(idle && initialized && notificationsEnabled);

        if (initialized) {
            int defaultTotal = 0;
            int nondefaultTotal = 0;
            for (String category : TracingController.getInstance().getKnownCategories()) {
                if (getCategoryType(category) == CategoryType.DEFAULT) {
                    defaultTotal++;
                } else {
                    nondefaultTotal++;
                }
            }

            int defaultEnabled = getEnabledCategories(CategoryType.DEFAULT).size();
            int nondefaultEnabled = getEnabledCategories(CategoryType.NON_DEFAULT).size();

            mPrefDefaultCategories.setSummary(
                    String.format(MSG_CATEGORIES_SUMMARY, defaultEnabled, defaultTotal));
            mPrefNondefaultCategories.setSummary(
                    String.format(MSG_CATEGORIES_SUMMARY, nondefaultEnabled, nondefaultTotal));

            mPrefMode.setValue(getSelectedTracingMode());
            mPrefMode.setSummary(TRACING_MODES.get(getSelectedTracingMode()));
        }

        if (!notificationsEnabled) {
            mPrefStartRecording.setTitle(MSG_START);
            mPrefTracingStatus.setTitle(MSG_NOTIFICATIONS_DISABLED);
        } else if (idle) {
            mPrefStartRecording.setTitle(MSG_START);
            mPrefTracingStatus.setTitle(MSG_PRIVACY_NOTICE);
        } else {
            mPrefStartRecording.setTitle(MSG_ACTIVE);
            mPrefTracingStatus.setTitle(MSG_ACTIVE_SUMMARY);
        }
    }
}

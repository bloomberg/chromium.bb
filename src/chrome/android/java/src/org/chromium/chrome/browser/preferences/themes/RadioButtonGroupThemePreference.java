// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.themes;

import android.content.Context;
import android.preference.Preference;
import android.util.AttributeSet;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.night_mode.NightModeMetrics;
import org.chromium.chrome.browser.preferences.themes.ThemePreferences.ThemeSetting;
import org.chromium.chrome.browser.widget.RadioButtonWithDescription;

import java.util.ArrayList;
import java.util.Collections;

/**
 * A radio button group Preference used for Themes. Currently, it has 3 options: System default,
 * Light, and Dark.
 */
public class RadioButtonGroupThemePreference
        extends Preference implements RadioButtonWithDescription.OnCheckedChangeListener {
    private @ThemeSetting int mSetting;
    private ArrayList<RadioButtonWithDescription> mButtons;

    public RadioButtonGroupThemePreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        // Inflating from XML.
        setLayoutResource(R.layout.radio_button_group_theme_preference);

        // Initialize entries with null objects so that calling ArrayList#set() would not throw
        // java.lang.IndexOutOfBoundsException.
        mButtons = new ArrayList<>(Collections.nCopies(ThemeSetting.NUM_ENTRIES, null));
    }

    /**
     * @param setting The initial setting for this Preference
     */
    public void initialize(@ThemeSetting int setting) {
        mSetting = setting;
    }

    @Override
    protected void onBindView(View view) {
        super.onBindView(view);

        assert ThemeSetting.NUM_ENTRIES == 3;
        mButtons.set(ThemeSetting.SYSTEM_DEFAULT, view.findViewById(R.id.system_default));
        mButtons.set(ThemeSetting.LIGHT, view.findViewById(R.id.light));
        mButtons.set(ThemeSetting.DARK, view.findViewById(R.id.dark));

        for (int i = 0; i < ThemeSetting.NUM_ENTRIES; i++) {
            mButtons.get(i).setRadioButtonGroup(mButtons);
            mButtons.get(i).setOnCheckedChangeListener(this);
        }

        mButtons.get(mSetting).setChecked(true);
    }

    @Override
    public void onCheckedChanged() {
        for (int i = 0; i < ThemeSetting.NUM_ENTRIES; i++) {
            if (mButtons.get(i).isChecked()) {
                mSetting = i;
                break;
            }
        }
        callChangeListener(mSetting);
        NightModeMetrics.recordThemePreferencesChanged(mSetting);
    }
}

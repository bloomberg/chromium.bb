// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode.settings;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.UI_THEME_DARKEN_WEBSITES_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.UI_THEME_SETTING;

import android.support.test.filters.SmallTest;
import android.view.View;
import android.widget.LinearLayout;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.BuildInfo;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.ThemeType;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionLayout;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

import java.util.Arrays;
import java.util.List;

/**
 * Tests for ThemeSettingsFragment.
 */
// clang-format off
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class ThemeSettingsFragmentTest extends DummyUiActivityTestCase {
    // clang-format on
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(new ParameterSet().value(false).name("DefaultLightDisabled"),
                    new ParameterSet().value(true).name("DefaultLightEnabled"));

    @Rule
    public SettingsActivityTestRule<ThemeSettingsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(ThemeSettingsFragment.class);

    private boolean mDefaultToLight;
    private ThemeSettingsFragment mFragment;
    private RadioButtonGroupThemePreference mPreference;

    public ThemeSettingsFragmentTest(boolean defaultToLight) {
        mDefaultToLight = defaultToLight;
        NightModeUtils.setNightModeDefaultToLightForTesting(defaultToLight);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        SharedPreferencesManager.getInstance().removeKey(UI_THEME_SETTING);
        SharedPreferencesManager.getInstance().removeKey(UI_THEME_DARKEN_WEBSITES_ENABLED);
        mSettingsActivityTestRule.startSettingsActivity();
        mFragment = mSettingsActivityTestRule.getFragment();
        mPreference = (RadioButtonGroupThemePreference) mFragment.findPreference(
                ThemeSettingsFragment.PREF_UI_THEME_PREF);
    }

    @Override
    public void tearDownTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SharedPreferencesManager.getInstance().removeKey(UI_THEME_SETTING);
            SharedPreferencesManager.getInstance().removeKey(UI_THEME_DARKEN_WEBSITES_ENABLED);
        });

        NightModeUtils.setNightModeDefaultToLightForTesting(null);
        super.tearDownTest();
    }

    @Test
    @SmallTest
    @Feature({"Themes"})
    public void testSelectThemes() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Default to light parameter is only applicable pre-Q.
            if (mDefaultToLight && BuildInfo.isAtLeastQ()) {
                Assert.assertFalse("Q should not default to light.",
                        NightModeUtils.isNightModeDefaultToLight());
                return;
            }

            int expectedDefaultTheme = mDefaultToLight ? ThemeType.LIGHT : ThemeType.SYSTEM_DEFAULT;
            Assert.assertEquals("Incorrect default theme setting.", expectedDefaultTheme,
                    NightModeUtils.getThemeSetting());
            assertButtonCheckedCorrectly(
                    mDefaultToLight ? "Light" : "System default", expectedDefaultTheme);

            // Select System default
            Assert.assertEquals(R.id.system_default, getButton(0).getId());
            selectButton(0);
            assertButtonCheckedCorrectly("System default", 0);
            Assert.assertEquals(ThemeType.SYSTEM_DEFAULT, mPreference.getSetting());
            Assert.assertEquals(mPreference.getSetting(),
                    SharedPreferencesManager.getInstance().readInt(UI_THEME_SETTING));

            // Select Light
            Assert.assertEquals(R.id.light, getButton(1).getId());
            selectButton(1);
            assertButtonCheckedCorrectly("Light", 1);
            Assert.assertEquals(ThemeType.LIGHT, mPreference.getSetting());
            Assert.assertEquals(mPreference.getSetting(),
                    SharedPreferencesManager.getInstance().readInt(UI_THEME_SETTING));

            // Select Dark
            Assert.assertEquals(R.id.dark, getButton(2).getId());
            selectButton(2);
            assertButtonCheckedCorrectly("Dark", 2);
            Assert.assertEquals(ThemeType.DARK, mPreference.getSetting());
            Assert.assertEquals(mPreference.getSetting(),
                    SharedPreferencesManager.getInstance().readInt(UI_THEME_SETTING));
        });
    }

    @Test
    @SmallTest
    @Feature({"Themes"})
    @Features.EnableFeatures(DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)
    public void testDarkenWebsiteButton() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Default to light parameter is only applicable pre-Q.
            if (mDefaultToLight && BuildInfo.isAtLeastQ()) {
                Assert.assertFalse("Q should not default to light.",
                        NightModeUtils.isNightModeDefaultToLight());
                return;
            }

            LinearLayout checkboxContainer = mPreference.getCheckboxContainerForTesting();
            RadioButtonWithDescriptionLayout group = mPreference.getGroupForTesting();

            int expectedDefaultTheme = mDefaultToLight ? ThemeType.LIGHT : ThemeType.SYSTEM_DEFAULT;
            Assert.assertEquals("Incorrect default theme setting.", expectedDefaultTheme,
                    NightModeUtils.getThemeSetting());
            assertButtonCheckedCorrectly(
                    mDefaultToLight ? "Light" : "System default", expectedDefaultTheme);

            // Select System default
            selectButton(0);
            Assert.assertTrue(
                    "Darken website button should be visible when system default is checked",
                    checkboxContainer.getVisibility() == View.VISIBLE);
            Assert.assertEquals(
                    "Darken website button should be below the system default option when system"
                            + " default is checked",
                    1, group.indexOfChild(checkboxContainer));

            // Select Light
            selectButton(1);
            Assert.assertTrue(
                    "Darken website button should be invisible when light theme is checked",
                    checkboxContainer.getVisibility() != View.VISIBLE);

            // Select Dark
            selectButton(2);
            Assert.assertTrue("Darken website button should be visible when dark theme is checked",
                    checkboxContainer.getVisibility() == View.VISIBLE);
            Assert.assertEquals("Darken website button should be below the dark theme option"
                            + " when dark theme is checked",
                    3, group.indexOfChild(checkboxContainer));

            // Check darken website button
            checkboxContainer.performClick();
            SharedPreferencesManager sharedPreferencesManager =
                    SharedPreferencesManager.getInstance();
            Assert.assertTrue("Darken website feature should be enabled when darken website button"
                            + " is checked",
                    mPreference.isDarkenWebsitesEnabled());
            Assert.assertTrue("Darken website feature should be enabled when darken website button"
                            + " is checked",
                    sharedPreferencesManager.readBoolean(UI_THEME_DARKEN_WEBSITES_ENABLED, false));

            // Check system default, darken website button should stay checked
            selectButton(1);
            Assert.assertTrue(
                    "Darken website button should stay its state when changing theme preference",
                    mPreference.isDarkenWebsitesEnabled());
            Assert.assertTrue(
                    "Darken website button should stay its state when changing theme preference",
                    sharedPreferencesManager.readBoolean(UI_THEME_DARKEN_WEBSITES_ENABLED, false));
        });
    }

    private RadioButtonWithDescription getButton(int index) {
        return (RadioButtonWithDescription) mPreference.getButtonsForTesting().get(index);
    }

    private void selectButton(int index) {
        getButton(index).onClick(null);
    }

    private boolean isRestUnchecked(int selectedIndex) {
        for (int i = 0; i < ThemeType.NUM_ENTRIES; i++) {
            if (i != selectedIndex && getButton(i).isChecked()) {
                return false;
            }
        }
        return true;
    }

    private void assertButtonCheckedCorrectly(String buttonTitle, int index) {
        Assert.assertTrue(buttonTitle + " button should be checked.", getButton(index).isChecked());
        Assert.assertTrue(
                "Buttons except " + buttonTitle + " should be unchecked.", isRestUnchecked(index));
    }
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import static org.chromium.chrome.browser.ChromeFeatureList.SEARCH_ENGINE_PROMO_EXISTING_DEVICE;

import android.support.annotation.Nullable;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.contextual_suggestions.EmptyEnabledStateMonitor;
import org.chromium.chrome.browser.contextual_suggestions.EnabledStateMonitor;
import org.chromium.chrome.browser.dependency_injection.ChromeAppModule;
import org.chromium.chrome.browser.dependency_injection.ModuleFactoryOverrides;
import org.chromium.chrome.browser.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;

/**
 * Tests for ContextualSuggestionsPreference.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.DisableFeatures(SEARCH_ENGINE_PROMO_EXISTING_DEVICE)
public class ContextualSuggestionsPreferenceTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private ContextualSuggestionsPreference mFragment;

    private FakeEnabledStateMonitor mEnabledStateMonitor = new FakeEnabledStateMonitor();

    private ChromeSwitchPreference mSwitch;
    private boolean mInitialSwitchState;

    private class FakeEnabledStateMonitor extends EmptyEnabledStateMonitor {
        @Nullable
        private EnabledStateMonitor.Observer mObserver;

        private boolean mEnabledInSettings;

        @Override
        public void addObserver(Observer observer) {
            mObserver = observer;
        }

        @Override
        public void removeObserver(Observer observer) {
            mObserver = null;
        }

        @Override
        public boolean getSettingsEnabled() {
            return mEnabledInSettings;
        }

        @Override
        public boolean getEnabledState() {
            // Mirroring the behavior of EnabledStateMonitorImpl
            return PrefServiceBridge.getInstance().getBoolean(Pref.CONTEXTUAL_SUGGESTIONS_ENABLED)
                    && mEnabledInSettings;
        }

        public void setSettingsEnabled(boolean enabled) {
            mEnabledInSettings = enabled;

            if (mObserver != null) {
                mObserver.onSettingsStateChanged(enabled);
            }
        }
    }

    private class ChromeAppModuleForTest extends ChromeAppModule {
        @Override
        public EnabledStateMonitor provideEnabledStateMonitor() {
            return mEnabledStateMonitor;
        }
    }

    @Before
    public void setUp() {
        ModuleFactoryOverrides.setOverride(
                ChromeAppModule.Factory.class, ChromeAppModuleForTest::new);

        Preferences preferences =
                PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                        ContextualSuggestionsPreference.class.getName());
        mFragment = (ContextualSuggestionsPreference) preferences.getFragmentForTest();
        mSwitch = (ChromeSwitchPreference) mFragment.findPreference(
                ContextualSuggestionsPreference.PREF_CONTEXTUAL_SUGGESTIONS_SWITCH);
        mInitialSwitchState = mSwitch.isChecked();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> setSwitchState(mInitialSwitchState));
        ModuleFactoryOverrides.clearOverrides();
    }

    @Test
    @SmallTest
    @Feature({"ContextualSuggestions"})
    public void testSwitch_Toggle() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mEnabledStateMonitor.setSettingsEnabled(true);

            // Check initial state matches preference.
            setSwitchState(true);
            Assert.assertEquals(mSwitch.isChecked(), true);
            Assert.assertEquals(mSwitch.isChecked(),
                    PrefServiceBridge.getInstance().getBoolean(
                            Pref.CONTEXTUAL_SUGGESTIONS_ENABLED));

            // Toggle and check if preference matches.
            PreferencesTest.clickPreference(mFragment, mSwitch);
            Assert.assertEquals(mSwitch.isChecked(), false);
            Assert.assertEquals(mSwitch.isChecked(),
                    PrefServiceBridge.getInstance().getBoolean(
                            Pref.CONTEXTUAL_SUGGESTIONS_ENABLED));

            // Toggle again check if preference matches.
            PreferencesTest.clickPreference(mFragment, mSwitch);
            Assert.assertEquals(mSwitch.isChecked(), true);
            Assert.assertEquals(mSwitch.isChecked(),
                    PrefServiceBridge.getInstance().getBoolean(
                            Pref.CONTEXTUAL_SUGGESTIONS_ENABLED));
        });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSuggestions"})
    public void testSwitch_SettingsStateChanged() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Make sure switch is checked.
            mEnabledStateMonitor.setSettingsEnabled(true);
            setSwitchState(true);
            Assert.assertTrue(mSwitch.isEnabled());
            Assert.assertTrue(mSwitch.isChecked());

            // Simulate settings are disabled.
            mEnabledStateMonitor.setSettingsEnabled(false);
            Assert.assertFalse(mSwitch.isEnabled());
            Assert.assertFalse(mSwitch.isChecked());
        });
    }

    private void setSwitchState(boolean checked) {
        if (mSwitch.isChecked() != checked) PreferencesTest.clickPreference(mFragment, mSwitch);
    }
}

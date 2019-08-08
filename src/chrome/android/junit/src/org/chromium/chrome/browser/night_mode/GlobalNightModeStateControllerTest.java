// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.base.ApplicationState.HAS_RUNNING_ACTIVITIES;
import static org.chromium.base.ApplicationState.HAS_STOPPED_ACTIVITIES;
import static org.chromium.chrome.browser.preferences.ChromePreferenceManager.UI_THEME_SETTING_KEY;

import android.content.Context;
import android.content.Intent;
import android.os.PowerManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPowerManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.themes.ThemePreferences;

/**
 * Unit tests for {@link GlobalNightModeStateController}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GlobalNightModeStateControllerTest {
    @Mock
    private NightModeStateProvider.Observer mObserver;

    private GlobalNightModeStateController mGlobalNightModeStateController;

    private SystemNightModeMonitor mSystemNightModeMonitor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mSystemNightModeMonitor = spy(SystemNightModeMonitor.getInstance());
        mGlobalNightModeStateController = new GlobalNightModeStateController(
                mSystemNightModeMonitor, ChromePreferenceManager.getInstance());

        mGlobalNightModeStateController.onApplicationStateChange(HAS_RUNNING_ACTIVITIES);

        // Night mode is disabled by default.
        assertFalse(GlobalNightModeStateProviderHolder.getInstance().isInNightMode());
    }

    @After
    public void tearDown() throws Exception {
        ChromePreferenceManager.getInstance().removeKey(UI_THEME_SETTING_KEY);
    }

    @Test
    public void testUpdateNightMode_PowerSaveMode() {
        // Enable power save mode and verify night mode is enabled.
        setIsPowerSaveMode(true);
        assertTrue(mGlobalNightModeStateController.isInNightMode());

        // Disable power save mode and verify night mode is disabled.
        setIsPowerSaveMode(false);
        assertFalse(mGlobalNightModeStateController.isInNightMode());
    }

    @Test
    public void testUpdateNightMode_SystemNightMode() {
        // Enable system night mode and verify night mode is enabled.
        setSystemNightMode(true);
        assertTrue(mGlobalNightModeStateController.isInNightMode());

        // Disable system night mode and verify night mode is disabled.
        setSystemNightMode(false);
        assertFalse(mGlobalNightModeStateController.isInNightMode());
    }

    @Test
    public void testUpdateNightMode_Preference() {
        // Set preference to dark theme and verify night mode is enabled.
        ChromePreferenceManager.getInstance().writeInt(
                UI_THEME_SETTING_KEY, ThemePreferences.ThemeSetting.DARK);
        assertTrue(mGlobalNightModeStateController.isInNightMode());

        // Set preference to light theme and verify night mode is disabled.
        ChromePreferenceManager.getInstance().writeInt(
                UI_THEME_SETTING_KEY, ThemePreferences.ThemeSetting.LIGHT);
        assertFalse(mGlobalNightModeStateController.isInNightMode());

        // Regardless of power save mode and system night mode, night mode is disabled with light
        // theme preference.
        setIsPowerSaveMode(true);
        assertFalse(mGlobalNightModeStateController.isInNightMode());

        setSystemNightMode(true);
        assertFalse(mGlobalNightModeStateController.isInNightMode());
    }

    @Test
    public void testStopAndRestart() {
        // Simulate to stop listening to night mode state changes. Verify that night mode state is
        // not changed.
        mGlobalNightModeStateController.onApplicationStateChange(HAS_STOPPED_ACTIVITIES);
        setIsPowerSaveMode(true);
        assertFalse(mGlobalNightModeStateController.isInNightMode());

        setSystemNightMode(true);
        assertFalse(mGlobalNightModeStateController.isInNightMode());

        ChromePreferenceManager.getInstance().writeInt(
                UI_THEME_SETTING_KEY, ThemePreferences.ThemeSetting.DARK);
        assertFalse(mGlobalNightModeStateController.isInNightMode());

        // Simulate to start listening to night mode state changes. Verify that
        //   1. Night mode state is updated after #start().
        //   2. Night mode state is updated on power save mode, system night mode, or preference
        //      changes.
        mGlobalNightModeStateController.onApplicationStateChange(HAS_RUNNING_ACTIVITIES);
        assertTrue(mGlobalNightModeStateController.isInNightMode());

        ChromePreferenceManager.getInstance().writeInt(
                UI_THEME_SETTING_KEY, ThemePreferences.ThemeSetting.SYSTEM_DEFAULT);
        assertTrue(mGlobalNightModeStateController.isInNightMode());

        setIsPowerSaveMode(false);
        assertTrue(mGlobalNightModeStateController.isInNightMode());

        setSystemNightMode(false);
        assertFalse(mGlobalNightModeStateController.isInNightMode());
    }

    @Test
    public void testObserver() {
        mGlobalNightModeStateController.addObserver(mObserver);

        // Verify that observer is called on night mode state changed from false to true.
        ChromePreferenceManager.getInstance().writeInt(
                UI_THEME_SETTING_KEY, ThemePreferences.ThemeSetting.DARK);
        assertTrue(mGlobalNightModeStateController.isInNightMode());
        verify(mObserver, times(1)).onNightModeStateChanged();

        // Verify that observer is not called when night mode state is not changed.
        setIsPowerSaveMode(true);
        assertTrue(mGlobalNightModeStateController.isInNightMode());
        verify(mObserver, times(1)).onNightModeStateChanged();

        // Verify that observer is not called when night mode state is not changed.
        setIsPowerSaveMode(false);
        assertTrue(mGlobalNightModeStateController.isInNightMode());
        verify(mObserver, times(1)).onNightModeStateChanged();

        // Verify that observer is called when set to light theme.
        ChromePreferenceManager.getInstance().writeInt(
                UI_THEME_SETTING_KEY, ThemePreferences.ThemeSetting.LIGHT);
        assertFalse(mGlobalNightModeStateController.isInNightMode());
        verify(mObserver, times(2)).onNightModeStateChanged();

        // Verify that observer is not called after it is removed.
        mGlobalNightModeStateController.removeObserver(mObserver);
        ChromePreferenceManager.getInstance().writeInt(
                UI_THEME_SETTING_KEY, ThemePreferences.ThemeSetting.DARK);
        assertTrue(mGlobalNightModeStateController.isInNightMode());
        verify(mObserver, times(2)).onNightModeStateChanged();
    }

    /**
     * Simulates setting power save mode, and notifies the change.
     * @param isPowerSaveMode Whether power save mode is enabled or not.
     */
    private void setIsPowerSaveMode(boolean isPowerSaveMode) {
        PowerManager powerManager = (PowerManager) RuntimeEnvironment.application.getSystemService(
                Context.POWER_SERVICE);
        ShadowPowerManager shadowPowerManager = shadowOf(powerManager);
        shadowPowerManager.setIsPowerSaveMode(isPowerSaveMode);
        ContextUtils.getApplicationContext().sendBroadcast(
                new Intent(PowerManager.ACTION_POWER_SAVE_MODE_CHANGED));
    }

    /**
     * Simulates setting system night mode, and notifies the change.
     * @param isSystemNightModeOn Whether system night mode is enabled or not.
     */
    private void setSystemNightMode(boolean isSystemNightModeOn) {
        when(mSystemNightModeMonitor.isSystemNightModeOn()).thenReturn(isSystemNightModeOn);
        mSystemNightModeMonitor.notifyObserversForTesting();
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import static org.chromium.chrome.browser.preferences.ChromePreferenceManager.UI_THEME_SETTING_KEY;

import android.annotation.TargetApi;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.os.PowerManager;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatDelegate;
import android.text.TextUtils;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.themes.ThemePreferences;

/**
 * Maintains and provides the night mode state for the entire application.
 */
class GlobalNightModeStateController implements NightModeStateProvider,
                                                SystemNightModeMonitor.Observer,
                                                ApplicationStatus.ApplicationStateListener {
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final SystemNightModeMonitor mSystemNightModeMonitor;
    private final ChromePreferenceManager mChromePreferenceManager;

    /**
     * Whether night mode is enabled throughout the entire app. If null, night mode is not
     * initialized yet.
     */
    private Boolean mNightModeOn;
    private ChromePreferenceManager.Observer mPreferenceObserver;

    /** Whether power save mode is on. This is always false on pre-L. */
    private boolean mPowerSaveModeOn;
    private @Nullable PowerManager mPowerManager;
    private @Nullable BroadcastReceiver mPowerModeReceiver;

    /** Whether this class has started listening to relevant states for night mode. */
    private boolean mIsStarted;

    /**
     * Should not directly instantiate unless for testing purpose. Use
     * {@link GlobalNightModeStateProviderHolder#getInstance()} instead.
     * @param systemNightModeMonitor The {@link SystemNightModeMonitor} that maintains the system
     *                               night mode state.
     * @param chromePreferenceManager The {@link ChromePreferenceManager} that maintains shared
     *                                preferences.
     */
    GlobalNightModeStateController(@NonNull SystemNightModeMonitor systemNightModeMonitor,
            @NonNull ChromePreferenceManager chromePreferenceManager) {
        mSystemNightModeMonitor = systemNightModeMonitor;
        mChromePreferenceManager = chromePreferenceManager;

        mPreferenceObserver = key -> {
            if (TextUtils.equals(key, UI_THEME_SETTING_KEY)) updateNightMode();
        };

        initializeForPowerSaveMode();
        updateNightMode();

        // It is unlikely that this is called after an activity is stopped or destroyed, but
        // checking here just to be safe.
        final int applicationState = ApplicationStatus.getStateForApplication();
        if (applicationState == ApplicationState.HAS_RUNNING_ACTIVITIES
                || applicationState == ApplicationState.HAS_PAUSED_ACTIVITIES) {
            start();
        }
        ApplicationStatus.registerApplicationStateListener(this);
    }

    // NightModeStateProvider implementation.
    @Override
    public boolean isInNightMode() {
        return mNightModeOn != null ? mNightModeOn : false;
    }

    @Override
    public void addObserver(@NonNull Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(@NonNull Observer observer) {
        mObservers.removeObserver(observer);
    }

    // SystemNightModeMonitor.Observer implementation.
    @Override
    public void onSystemNightModeChanged() {
        updateNightMode();
    }

    // ApplicationStatus.ApplicationStateListener implementation.
    @Override
    public void onApplicationStateChange(int newState) {
        if (newState == ApplicationState.HAS_RUNNING_ACTIVITIES) {
            start();
        } else if (newState == ApplicationState.HAS_STOPPED_ACTIVITIES) {
            stop();
        }
    }

    /** Starts listening to states relevant to night mode. */
    private void start() {
        if (mIsStarted) return;
        mIsStarted = true;

        if (mPowerModeReceiver != null) {
            updatePowerSaveMode();
            ContextUtils.getApplicationContext().registerReceiver(mPowerModeReceiver,
                    new IntentFilter(PowerManager.ACTION_POWER_SAVE_MODE_CHANGED));
        }
        mSystemNightModeMonitor.addObserver(this);
        mChromePreferenceManager.addObserver(mPreferenceObserver);
        updateNightMode();
    }

    /** Stops listening to states relevant to night mode. */
    private void stop() {
        if (!mIsStarted) return;
        mIsStarted = false;

        if (mPowerModeReceiver != null) {
            ContextUtils.getApplicationContext().unregisterReceiver(mPowerModeReceiver);
        }
        mSystemNightModeMonitor.removeObserver(this);
        mChromePreferenceManager.removeObserver(mPreferenceObserver);
    }

    private void updateNightMode() {
        final int themeSetting = mChromePreferenceManager.readInt(UI_THEME_SETTING_KEY);
        final boolean newNightModeOn = themeSetting == ThemePreferences.ThemeSetting.SYSTEM_DEFAULT
                        && (mPowerSaveModeOn || mSystemNightModeMonitor.isSystemNightModeOn())
                || themeSetting == ThemePreferences.ThemeSetting.DARK;
        if (mNightModeOn != null && newNightModeOn == mNightModeOn) return;

        mNightModeOn = newNightModeOn;
        AppCompatDelegate.setDefaultNightMode(
                mNightModeOn ? AppCompatDelegate.MODE_NIGHT_YES : AppCompatDelegate.MODE_NIGHT_NO);
        for (Observer observer : mObservers) observer.onNightModeStateChanged();

        NightModeMetrics.recordNightModeState(mNightModeOn);
        NightModeMetrics.recordThemePreferencesState(themeSetting);
        if (mNightModeOn) {
            NightModeMetrics.recordNightModeEnabledReason(themeSetting, mPowerSaveModeOn);
        }
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void initializeForPowerSaveMode() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        mPowerManager = (PowerManager) ContextUtils.getApplicationContext().getSystemService(
                Context.POWER_SERVICE);
        updatePowerSaveMode();
        mPowerModeReceiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                updatePowerSaveMode();
                updateNightMode();
            }
        };
    }

    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void updatePowerSaveMode() {
        mPowerSaveModeOn = mPowerManager.isPowerSaveMode();
    }
}

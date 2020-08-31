// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.UI_THEME_SETTING;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.os.Build;
import android.view.ContextThemeWrapper;

import androidx.annotation.Nullable;
import androidx.annotation.StyleRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Helper methods for supporting night mode.
 */
public class NightModeUtils {
    private static Boolean sNightModeSupportedForTest;
    private static Boolean sNightModeDefaultToLightForTesting;

    /**
     * Due to Lemon issues on resources access, night mode is disabled on Kitkat until the issue is
     * resolved. See https://crbug.com/957286 for details.
     * @return Whether night mode is supported.
     */
    public static boolean isNightModeSupported() {
        if (sNightModeSupportedForTest != null) return sNightModeSupportedForTest;
        return Build.VERSION.SDK_INT > Build.VERSION_CODES.KITKAT;
    }

    /**
     * If the {@link Context} is a ChromeBaseAppCompatActivity, this method will get the
     * {@link NightModeStateProvider} from the activity. Otherwise, the
     * {@link GlobalNightModeStateProviderHolder} will be used.
     * @param context The {@link Context} to get the NightModeStateProvider.
     * @return Whether or not the night mode is enabled.
     */
    public static boolean isInNightMode(Context context) {
        if (context instanceof ChromeBaseAppCompatActivity) {
            return ((ChromeBaseAppCompatActivity) context)
                    .getNightModeStateProvider()
                    .isInNightMode();
        }
        return GlobalNightModeStateProviderHolder.getInstance().isInNightMode();
    }

    /**
     * Updates configuration for night mode to ensure night mode settings are applied properly.
     * Should be called anytime the Activity's configuration changes (e.g. from
     * {@link Activity#onConfigurationChanged(Configuration)}) if uiMode was not overridden on
     * the configuration during activity initialization
     * (see {@link #applyOverridesForNightMode(NightModeStateProvider, Configuration)}).
     * @param activity The {@link ChromeBaseAppCompatActivity} that needs to be updated.
     * @param newConfig The new {@link Configuration} from
     *                  {@link Activity#onConfigurationChanged(Configuration)}.
     * @param themeResId The {@link StyleRes} for the theme of the specified activity.
     */
    public static void updateConfigurationForNightMode(ChromeBaseAppCompatActivity activity,
            Configuration newConfig, @StyleRes int themeResId) {
        final int uiNightMode = activity.getNightModeStateProvider().isInNightMode()
                ? Configuration.UI_MODE_NIGHT_YES
                : Configuration.UI_MODE_NIGHT_NO;

        if (uiNightMode == (newConfig.uiMode & Configuration.UI_MODE_NIGHT_MASK)) return;

        // This is to fix styles on floating action bar when the new configuration UI mode doesn't
        // match the actual UI mode we need, and NightModeStateProvider#shouldOverrideConfiguration
        // returns false. May check if this is needed on newer version of support library.
        // See https://crbug.com/935731.
        if (themeResId != 0) {
            // Re-apply theme so that the correct configuration is used.
            activity.setTheme(themeResId);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                // On M+ setTheme only applies if the themeResId actually changes. Force applying
                // the styles so that the correct styles are used.
                activity.getTheme().applyStyle(themeResId, true);
            }
        }
    }

    /**
     * @param provider The {@link NightModeStateProvider} that provides the night mode state.
     * @param config The {@link Configuration} on which UI night mode should be overridden if
     *               necessary.
     * @return True if UI night mode is overridden on the provided {@code config}, and false
     *         otherwise.
     */
    public static boolean applyOverridesForNightMode(
            NightModeStateProvider provider, Configuration config) {
        if (!provider.shouldOverrideConfiguration()) return false;

        // Override uiMode so that UIs created by the DecorView (e.g. context menu, floating
        // action bar) get the correct theme. May check if this is needed on newer version
        // of support library. See https://crbug.com/935731.
        final int nightMode = provider.isInNightMode() ? Configuration.UI_MODE_NIGHT_YES
                                                       : Configuration.UI_MODE_NIGHT_NO;
        config.uiMode = nightMode | (config.uiMode & ~Configuration.UI_MODE_NIGHT_MASK);
        return true;
    }

    /**
     * Wraps a {@link Context} into one having a resource configuration with the given night mode
     * setting.
     * @param context {@link Context} to wrap.
     * @param themeResId Theme resource to use with {@link ContextThemeWrapper}.
     * @param nightMode Whether to apply night mode.
     * @return Wrapped {@link Context}.
     */
    public static Context wrapContextWithNightModeConfig(Context context, @StyleRes int themeResId,
            boolean nightMode) {
        ContextThemeWrapper wrapper = new ContextThemeWrapper(context, themeResId);
        Configuration config = new Configuration();
        // Pre-Android O, fontScale gets initialized to 1 in the constructor. Set it to 0 so
        // that applyOverrideConfiguration() does not interpret it as an overridden value.
        config.fontScale = 0;
        int nightModeFlag = nightMode ? Configuration.UI_MODE_NIGHT_YES
                : Configuration.UI_MODE_NIGHT_NO;
        config.uiMode = nightModeFlag | (config.uiMode & ~Configuration.UI_MODE_NIGHT_MASK);
        wrapper.applyOverrideConfiguration(config);
        return wrapper;
    }

    /**
     * The current theme setting, reflecting either the user setting or the default if the user has
     * not explicitly set a preference.
     * @return The current theme setting. See {@link ThemeType}.
     */
    public static @ThemeType int getThemeSetting() {
        int userSetting = SharedPreferencesManager.getInstance().readInt(UI_THEME_SETTING, -1);
        if (userSetting == -1) {
            return isNightModeDefaultToLight() ? ThemeType.LIGHT : ThemeType.SYSTEM_DEFAULT;
        } else {
            return userSetting;
        }
    }

    @VisibleForTesting
    public static void setNightModeSupportedForTesting(@Nullable Boolean nightModeSupported) {
        sNightModeSupportedForTest = nightModeSupported;
    }

    /**
     * @return Whether or not to default to the light theme when the night mode feature is enabled.
     */
    public static boolean isNightModeDefaultToLight() {
        if (sNightModeDefaultToLightForTesting != null) {
            return sNightModeDefaultToLightForTesting;
        }
        return !BuildInfo.isAtLeastQ();
    }

    /**
     * Toggles whether the night mode experiment is enabled for testing. Should be reset back to
     * null after the test has finished.
     */
    @VisibleForTesting
    public static void setNightModeDefaultToLightForTesting(@Nullable Boolean available) {
        sNightModeDefaultToLightForTesting = available;
    }
}

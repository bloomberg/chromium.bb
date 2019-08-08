// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.app.Fragment;
import android.os.Build;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.browser.preferences.website.ContentSettingsResources;
import org.chromium.chrome.browser.preferences.website.SingleCategoryPreferences;
import org.chromium.chrome.browser.preferences.website.SiteSettingsCategory;
import org.chromium.chrome.browser.test.ChromeBrowserTestRule;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for the NotificationsPreferences.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class NotificationsPreferencesTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();
    private Preferences mActivity;

    @Rule
    public ScreenShooter mScreenShooter = new ScreenShooter();

    @Before
    public void setUp() {
        mActivity = PreferencesTest.startPreferences(InstrumentationRegistry.getInstrumentation(),
                NotificationsPreferences.class.getName());
    }

    // TODO(https://crbug.com/894334): Remove format suppression once formatting bug is fixed.
    // clang-format off
    @Test
    @SmallTest
    @Feature({"Preferences", "UiCatalogue"})
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.N_MR1)
    @CommandLineFlags.Add("enable-features=ContentSuggestionsNotifications")
    @Features.DisableFeatures(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)
    public void testContentSuggestionsToggle() {
        // clang-format on

        final PreferenceFragment fragment = (PreferenceFragment) mActivity.getFragmentForTest();
        final ChromeSwitchPreference toggle = (ChromeSwitchPreference) fragment.findPreference(
                NotificationsPreferences.PREF_SUGGESTIONS);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Make sure the toggle reflects the state correctly.
            boolean initiallyChecked = toggle.isChecked();
            Assert.assertEquals(toggle.isChecked(),
                    PrefServiceBridge.getInstance().getBoolean(
                            Pref.CONTENT_SUGGESTIONS_NOTIFICATIONS_ENABLED));

            // Make sure we can change the state.
            PreferencesTest.clickPreference(fragment, toggle);
            Assert.assertEquals(toggle.isChecked(), !initiallyChecked);
            Assert.assertEquals(toggle.isChecked(),
                    PrefServiceBridge.getInstance().getBoolean(
                            Pref.CONTENT_SUGGESTIONS_NOTIFICATIONS_ENABLED));

            // Make sure we can change it back.
            PreferencesTest.clickPreference(fragment, toggle);
            Assert.assertEquals(toggle.isChecked(), initiallyChecked);
            Assert.assertEquals(toggle.isChecked(),
                    PrefServiceBridge.getInstance().getBoolean(
                            Pref.CONTENT_SUGGESTIONS_NOTIFICATIONS_ENABLED));

            // Click it one last time so we're in a toggled state for the UI Capture.
            PreferencesTest.clickPreference(fragment, toggle);
        });

        mScreenShooter.shoot("ContentSuggestionsToggle");
    }

    // TODO(https://crbug.com/894334): Remove format suppression once formatting bug is fixed.
    // clang-format off
    @Test
    @SmallTest
    @Feature({"Preferences", "UiCatalogue"})
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.N_MR1)
    @CommandLineFlags.Add("disable-features=NTPArticleSuggestions")
    @Features.DisableFeatures(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)
    public void testToggleDisabledWhenSuggestionsDisabled() {
        // clang-format on

        PreferenceFragment fragment = (PreferenceFragment) mActivity.getFragmentForTest();
        ChromeSwitchPreference toggle = (ChromeSwitchPreference) fragment.findPreference(
                NotificationsPreferences.PREF_SUGGESTIONS);

        Assert.assertFalse(toggle.isEnabled());
        Assert.assertFalse(toggle.isChecked());

        mScreenShooter.shoot("ToggleDisabledWhenSuggestionsDisabled");
    }

    // TODO(https://crbug.com/894334): Remove format suppression once formatting bug is fixed.
    // clang-format off
    @Test
    @SmallTest
    @Feature({"Preferences", "UiCatalogue"})
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.N_MR1)
    @Features.DisableFeatures(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)
    public void testLinkToWebsiteNotifications() {
        // clang-format on

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PreferenceFragment fragment = (PreferenceFragment) mActivity.getFragmentForTest();
            Preference fromWebsites =
                    fragment.findPreference(NotificationsPreferences.PREF_FROM_WEBSITES);

            PreferencesTest.clickPreference(fragment, fromWebsites);
        });

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return getTopFragment() instanceof SingleCategoryPreferences;
            }
        });

        SingleCategoryPreferences fragment = (SingleCategoryPreferences) getTopFragment();
        Assert.assertTrue(
                fragment.getCategoryForTest().showSites(SiteSettingsCategory.Type.NOTIFICATIONS));

        mScreenShooter.shoot("LinkToWebsiteNotifications");
    }

    /** Gets the fragment of the top Activity. Assumes the top Activity is a Preferences. */
    private static Fragment getTopFragment() {
        Preferences preferences = (Preferences) ApplicationStatus.getLastTrackedFocusedActivity();
        return preferences.getFragmentForTest();
    }

    // TODO(https://crbug.com/894334): Remove format suppression once formatting bug is fixed.
    // clang-format off
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.N_MR1)
    @Features.DisableFeatures(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)
    public void testWebsiteNotificationsSummary() {
        // clang-format on

        final PreferenceFragment fragment = (PreferenceFragment) mActivity.getFragmentForTest();
        final Preference fromWebsites =
                fragment.findPreference(NotificationsPreferences.PREF_FROM_WEBSITES);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PrefServiceBridge.getInstance().setCategoryEnabled(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS, false);
            fragment.onResume();
            Assert.assertEquals(fromWebsites.getSummary(), getNotificationsSummary(false));

            PrefServiceBridge.getInstance().setCategoryEnabled(
                    ContentSettingsType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS, true);
            fragment.onResume();
            Assert.assertEquals(fromWebsites.getSummary(), getNotificationsSummary(true));
        });
    }

    // TODO(https://crbug.com/894334): Remove format suppression once formatting bug is fixed.
    // clang-format off
    @Test
    @SmallTest
    @Feature({"Preferences"})
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.N_MR1)
    @Features.EnableFeatures(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)
    public void prefHiddenWhenOnFeed() {
        // clang-format on

        final PreferenceFragment fragment = (PreferenceFragment) mActivity.getFragmentForTest();
        Assert.assertNull(fragment.findPreference(NotificationsPreferences.PREF_SUGGESTIONS));
    }

    /** Gets the summary text that should be used for site specific notifications. */
    private String getNotificationsSummary(boolean enabled) {
        return mActivity.getResources().getString(ContentSettingsResources.getCategorySummary(
                ContentSettingsType.CONTENT_SETTINGS_TYPE_NOTIFICATIONS, enabled));
    }
}

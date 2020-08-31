// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.site_settings.SiteSettingsTestUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.ExpandablePreferenceGroup;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.site_settings.WebsiteAddress;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for TrustedWebActivity functionality under Settings > Site Settings.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
@CommandLineFlags.Add({
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
})
public class TrustedWebActivityPreferencesUiTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private String mPackage;
    private TrustedWebActivityPermissionManager mPermissionMananger;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        mPackage = InstrumentationRegistry.getTargetContext().getPackageName();
        mPermissionMananger = ChromeApplication.getComponent().resolveTwaPermissionManager();
    }

    /**
     * Tests that the 'Managed by' section appears correctly and that it contains our registered
     * website.
     * @throws Exception
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testSingleCategoryManagedBy() throws Exception {
        final String site = "http://example.com";
        final Origin origin = Origin.create(site);

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mPermissionMananger.updatePermission(
                                origin, mPackage, ContentSettingsType.NOTIFICATIONS, true));

        SettingsActivity settingsActivity = SiteSettingsTestUtils.startSiteSettingsCategory(
                SiteSettingsCategory.Type.NOTIFICATIONS);
        final String groupName = "managed_group";

        final SingleCategorySettings websitePreferences =
                TestThreadUtils.runOnUiThreadBlocking(() -> {
                    final SingleCategorySettings preferences =
                            (SingleCategorySettings) settingsActivity.getMainFragment();
                    final ExpandablePreferenceGroup group =
                            (ExpandablePreferenceGroup) preferences.findPreference(groupName);
                    preferences.onPreferenceClick(group);
                    return preferences;
                });

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                // The preference group gets recreated in onPreferenceClick, so we need to find it
                // again.
                final ExpandablePreferenceGroup group =
                        (ExpandablePreferenceGroup) websitePreferences.findPreference(groupName);
                return group.isExpanded();
            }
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final ExpandablePreferenceGroup group =
                    (ExpandablePreferenceGroup) websitePreferences.findPreference(groupName);
            Assert.assertEquals(1, group.getPreferenceCount());
            androidx.preference.Preference preference = group.getPreference(0);
            CharSequence title = preference.getTitle();
            Assert.assertEquals("example.com", title.toString());
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> mPermissionMananger.unregister(origin));

        settingsActivity.finish();
    }

    /**
     * Tests that registered sites show 'Managed by' in the title when viewing the details for a
     * single website.
     */
    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testWebsitePreferencesManagedBy() {
        final String site = "http://example.com";
        final Origin origin = Origin.create(site);

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mPermissionMananger.updatePermission(
                                origin, mPackage, ContentSettingsType.NOTIFICATIONS, true));

        WebsiteAddress address = WebsiteAddress.create(site);
        Website website = new Website(address, address);
        final SettingsActivity settingsActivity =
                SiteSettingsTestUtils.startSingleWebsitePreferences(website);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final SingleWebsiteSettings websitePreferences =
                    (SingleWebsiteSettings) settingsActivity.getMainFragment();
            final ChromeImageViewPreference notificationPreference =
                    (ChromeImageViewPreference) websitePreferences.findPreference(
                            "push_notifications_list");
            CharSequence summary = notificationPreference.getSummary();
            Assert.assertTrue(summary.toString().startsWith("Managed by "));
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> mPermissionMananger.unregister(origin));

        settingsActivity.finish();
    }
}

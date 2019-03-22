// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertThat;

import android.content.Context;
import android.preference.CheckBoxPreference;
import android.preference.PreferenceScreen;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CollectionUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Preferences;
import org.chromium.chrome.browser.preferences.privacy.ClearBrowsingDataPreferences.DialogOption;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.test.util.MockSyncContentResolverDelegate;

import java.util.HashSet;
import java.util.Set;

/**
 * Integration tests for ClearBrowsingDataPreferencesBasic.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ClearBrowsingDataPreferencesBasicTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String GOOGLE_ACCOUNT = "Google Account";
    private static final String OTHER_ACTIVITY = "other forms of browsing history";
    private static final String SIGNED_IN_DEVICES = "signed-in devices";

    @Before
    public void setUp() throws InterruptedException {
        SigninTestUtil.setUpAuthForTest();
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> ProfileSyncService.resetForTests());
    }

    private static class StubProfileSyncService extends ProfileSyncService {
        private final boolean mSyncable;

        StubProfileSyncService(boolean syncable) {
            super();
            mSyncable = syncable;
        }

        @Override
        public Set<Integer> getActiveDataTypes() {
            return mSyncable ? CollectionUtil.newHashSet(ModelType.HISTORY_DELETE_DIRECTIVES)
                             : new HashSet<Integer>();
        }
    }

    private void setSyncable(final boolean syncable) {
        Context context = InstrumentationRegistry.getTargetContext();
        MockSyncContentResolverDelegate delegate = new MockSyncContentResolverDelegate();
        delegate.setMasterSyncAutomatically(syncable);
        AndroidSyncSettings.overrideForTests(delegate, null);
        if (syncable) {
            AndroidSyncSettings.get().enableChromeSync();
        } else {
            AndroidSyncSettings.get().disableChromeSync();
        }

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ProfileSyncService.overrideForTests(new StubProfileSyncService(syncable));
            }
        });
    }

    private String getCheckboxSummary(PreferenceScreen screen, String preference) {
        CheckBoxPreference checkbox = (CheckBoxPreference) screen.findPreference(preference);
        return new StringBuilder(checkbox.getSummary()).toString();
    }

    /**
     * Tests that for users who are not signed in, only the general information is shown.
     */
    @Test
    @SmallTest
    public void testCheckBoxTextNonsigned() throws Exception {
        SigninTestUtil.resetSigninState();

        final Preferences preferences = mActivityTestRule.startPreferences(
                ClearBrowsingDataPreferencesBasic.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ClearBrowsingDataPreferencesBasic fragment =
                        (ClearBrowsingDataPreferencesBasic) preferences.getFragmentForTest();
                PreferenceScreen screen = fragment.getPreferenceScreen();

                String cookiesSummary = getCheckboxSummary(screen,
                        ClearBrowsingDataPreferences.getPreferenceKey(
                                DialogOption.CLEAR_COOKIES_AND_SITE_DATA));
                String historySummary = getCheckboxSummary(screen,
                        ClearBrowsingDataPreferences.getPreferenceKey(DialogOption.CLEAR_HISTORY));

                assertThat(cookiesSummary, not(containsString(GOOGLE_ACCOUNT)));
                assertThat(historySummary, not(containsString(OTHER_ACTIVITY)));
                assertThat(historySummary, not(containsString(SIGNED_IN_DEVICES)));
            }
        });
    }

    /**
     * Tests that for users who are signed in but don't have sync activated,
     * only information about "other activity" is shown in the history summary.
     */
    @Test
    @SmallTest
    public void testCheckBoxTextSigned() throws Exception {
        SigninTestUtil.addAndSignInTestAccount();
        setSyncable(false);

        final Preferences preferences = mActivityTestRule.startPreferences(
                ClearBrowsingDataPreferencesBasic.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ClearBrowsingDataPreferencesBasic fragment =
                        (ClearBrowsingDataPreferencesBasic) preferences.getFragmentForTest();
                PreferenceScreen screen = fragment.getPreferenceScreen();

                String historySummary = getCheckboxSummary(screen,
                        ClearBrowsingDataPreferences.getPreferenceKey(DialogOption.CLEAR_HISTORY));

                assertThat(historySummary, containsString(OTHER_ACTIVITY));
                assertThat(historySummary, not(containsString(SIGNED_IN_DEVICES)));
            }
        });
    }

    /**
     * Tests that users who are signed in, and have sync enabled see information
     * about their "other activity" and history on "signed in devices".
     */
    @Test
    @SmallTest
    public void testCheckBoxTextSignedAndSynced() throws Exception {
        SigninTestUtil.addAndSignInTestAccount();
        setSyncable(true);

        final Preferences preferences = mActivityTestRule.startPreferences(
                ClearBrowsingDataPreferencesBasic.class.getName());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                ClearBrowsingDataPreferencesBasic fragment =
                        (ClearBrowsingDataPreferencesBasic) preferences.getFragmentForTest();
                PreferenceScreen screen = fragment.getPreferenceScreen();

                String historySummary = getCheckboxSummary(screen,
                        ClearBrowsingDataPreferences.getPreferenceKey(DialogOption.CLEAR_HISTORY));

                assertThat(historySummary, containsString(OTHER_ACTIVITY));
                assertThat(historySummary, containsString(SIGNED_IN_DEVICES));
            }
        });
    }
}

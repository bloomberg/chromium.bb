// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.sync.FakeProfileSyncService;
import org.chromium.chrome.browser.sync.GoogleServiceAuthError;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.IOException;

/**
 * Test suite for the SyncErrorInfoBar.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags
        .Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
        @Features.EnableFeatures({ChromeFeatureList.SYNC_ERROR_INFOBAR_ANDROID})
        public class SyncErrorInfoBarTest {
    private FakeProfileSyncService mFakeProfileSyncService;

    @Rule
    public final SyncTestRule mSyncTestRule = new SyncTestRule() {
        @Override
        protected FakeProfileSyncService createProfileSyncService() {
            return new FakeProfileSyncService();
        }
    };

    @Rule
    public final ChromeRenderTestRule mRenderTestRule = new ChromeRenderTestRule();

    @Before
    public void setUp() {
        deleteSyncErrorInfoBarShowTimePref();
        mFakeProfileSyncService = (FakeProfileSyncService) mSyncTestRule.getProfileSyncService();
        mSyncTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @LargeTest
    public void testSyncErrorInfoBarShownForAuthError() throws Exception {
        Assert.assertEquals("InfoBar should not be shown before signing in", 0,
                mSyncTestRule.getInfoBars().size());
        showSyncErrorInfoBarForAuthError();
        Assert.assertEquals("InfoBar should be shown", 1, mSyncTestRule.getInfoBars().size());

        // Resolving the error should not show the infobar again.
        deleteSyncErrorInfoBarShowTimePref();
        mFakeProfileSyncService.setAuthError(GoogleServiceAuthError.State.NONE);
        InfoBarUtil.waitUntilNoInfoBarsExist(mSyncTestRule.getInfoBars());
    }

    @Test
    @LargeTest
    public void testSyncErrorInfoBarShownForSyncSetupIncomplete() {
        Assert.assertEquals("InfoBar should not be shown before signing in", 0,
                mSyncTestRule.getInfoBars().size());
        showSyncErrorInfoBarForSyncSetupIncomplete();
        Assert.assertEquals("InfoBar should be shown", 1, mSyncTestRule.getInfoBars().size());

        // Resolving the error should not show the infobar again.
        deleteSyncErrorInfoBarShowTimePref();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mFakeProfileSyncService.setFirstSetupComplete(SyncFirstSetupCompleteSource.BASIC_FLOW);
        });
        InfoBarUtil.waitUntilNoInfoBarsExist(mSyncTestRule.getInfoBars());
    }

    @Test
    @LargeTest
    public void testSyncErrorInfoBarShownForPassphraseRequired() {
        Assert.assertEquals("InfoBar should not be shown before signing in", 0,
                mSyncTestRule.getInfoBars().size());
        showSyncErrorInfoBarForPassphraseRequired();
        Assert.assertEquals("InfoBar should be shown", 1, mSyncTestRule.getInfoBars().size());

        // Resolving the error should not show the infobar again.
        deleteSyncErrorInfoBarShowTimePref();
        mFakeProfileSyncService.setPassphraseRequiredForPreferredDataTypes(false);
        InfoBarUtil.waitUntilNoInfoBarsExist(mSyncTestRule.getInfoBars());
    }

    @Test
    @LargeTest
    public void testSyncErrorInfoBarNotShownWhenNoError() {
        Assert.assertEquals("InfoBar should not be shown before signing in", 0,
                mSyncTestRule.getInfoBars().size());
        mSyncTestRule.setUpAccountAndSignInForTesting();
        SyncTestUtil.waitForSyncActive();
        mFakeProfileSyncService.setEngineInitialized(true);
        mFakeProfileSyncService.setAuthError(GoogleServiceAuthError.State.NONE);
        mFakeProfileSyncService.setPassphraseRequiredForPreferredDataTypes(false);

        @SyncError
        int syncError = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            mFakeProfileSyncService.setFirstSetupComplete(SyncFirstSetupCompleteSource.BASIC_FLOW);
            return SyncSettingsUtils.getSyncError();
        });
        // syncError should not equal to any of these errors that trigger the infobar.
        Assert.assertTrue(syncError != SyncError.AUTH_ERROR);
        Assert.assertTrue(syncError != SyncError.PASSPHRASE_REQUIRED);
        Assert.assertTrue(syncError != SyncError.SYNC_SETUP_INCOMPLETE);

        Assert.assertEquals("InfoBar should not be shown when there is no error", 0,
                mSyncTestRule.getInfoBars().size());
    }

    @Test
    @LargeTest
    public void testSyncErrorInfoBarIsNotShownBeforeMinimalIntervalPassed() {
        // Initiate auth error to show the infobar.
        Assert.assertEquals("InfoBar should not be shown before signing in", 0,
                mSyncTestRule.getInfoBars().size());
        showSyncErrorInfoBarForAuthError();
        Assert.assertEquals("InfoBar should be shown", 1, mSyncTestRule.getInfoBars().size());

        // Create another new tab.
        mSyncTestRule.loadUrlInNewTab(UrlConstants.CHROME_BLANK_URL);
        Assert.assertEquals("InfoBar should not be shown again before minimum interval passed", 0,
                mSyncTestRule.getInfoBars().size());

        // Override the time of last seen infobar to minimum required time before current time.
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putLong(SyncErrorInfoBar.PREF_SYNC_ERROR_INFOBAR_SHOWN_AT_TIME,
                        System.currentTimeMillis()
                                - SyncErrorInfoBar.MINIMAL_DURATION_BETWEEN_INFOBARS_MS)
                .apply();
        mSyncTestRule.loadUrlInNewTab(UrlConstants.CHROME_BLANK_URL);
        Assert.assertEquals("InfoBar should be shown again after minimum interval passed", 1,
                mSyncTestRule.getInfoBars().size());
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncErrorInfoBarForAuthErrorView() throws IOException {
        showSyncErrorInfoBarForAuthError();
        mRenderTestRule.render(mSyncTestRule.getInfoBarContainer().getContainerViewForTesting(),
                "sync_error_infobar_auth_error");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncErrorInfoBarForSyncSetupIncompleteView() throws IOException {
        showSyncErrorInfoBarForSyncSetupIncomplete();
        mRenderTestRule.render(mSyncTestRule.getInfoBarContainer().getContainerViewForTesting(),
                "sync_error_infobar_sync_setup_incomplete");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncErrorInfoBarForPassphraseRequiredView() throws IOException {
        showSyncErrorInfoBarForPassphraseRequired();
        mRenderTestRule.render(mSyncTestRule.getInfoBarContainer().getContainerViewForTesting(),
                "sync_error_infobar_passphrase_required");
    }

    private void showSyncErrorInfoBarForAuthError() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mFakeProfileSyncService.setAuthError(GoogleServiceAuthError.State.INVALID_GAIA_CREDENTIALS);
        mSyncTestRule.loadUrlInNewTab(UrlConstants.CHROME_BLANK_URL);
    }

    private void showSyncErrorInfoBarForPassphraseRequired() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mFakeProfileSyncService.setEngineInitialized(true);
        mFakeProfileSyncService.setPassphraseRequiredForPreferredDataTypes(true);
        mSyncTestRule.loadUrlInNewTab(UrlConstants.CHROME_BLANK_URL);
    }

    private void showSyncErrorInfoBarForSyncSetupIncomplete() {
        mSyncTestRule.setUpTestAccountAndSignInWithSyncSetupAsIncomplete();
        mSyncTestRule.loadUrlInNewTab(UrlConstants.CHROME_BLANK_URL);
    }

    private void deleteSyncErrorInfoBarShowTimePref() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .remove(SyncErrorInfoBar.PREF_SYNC_ERROR_INFOBAR_SHOWN_AT_TIME)
                .apply();
    }
}

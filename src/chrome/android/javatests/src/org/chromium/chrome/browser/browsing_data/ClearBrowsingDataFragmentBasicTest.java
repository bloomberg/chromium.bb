// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.view.View;

import androidx.annotation.Nullable;
import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;

import org.chromium.base.CollectionUtil;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.sync.ModelType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.IOException;
import java.util.HashSet;

/**
 * Integration tests for ClearBrowsingDataFragmentBasic.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ClearBrowsingDataFragmentBasicTest {
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();
    public final SettingsActivityTestRule<ClearBrowsingDataFragmentBasic>
            mSettingsActivityTestRule =
                    new SettingsActivityTestRule<>(ClearBrowsingDataFragmentBasic.class);

    // SettingsActivity has to be finished before the outer CTA can be finished or trying to finish
    // CTA won't work.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mActivityTestRule).around(mSettingsActivityTestRule);

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Mock
    private SyncService mMockSyncService;

    @Mock
    public TemplateUrlService mMockTemplateUrlService;
    @Mock
    public TemplateUrl mMockSearchEngine;

    private @Nullable TemplateUrlService mActualTemplateUrlService;

    @Before
    public void setUp() throws InterruptedException {
        initMocks(this);
        TestThreadUtils.runOnUiThreadBlocking(() -> SyncService.overrideForTests(mMockSyncService));
        setSyncable(false);
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> SyncService.resetForTests());
        if (mActualTemplateUrlService != null) {
            // Reset the actual service if the mock is used.
            TemplateUrlServiceFactory.setInstanceForTesting(mActualTemplateUrlService);
        }
    }

    private void setSyncable(boolean syncable) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            when(mMockSyncService.isSyncRequested()).thenReturn(syncable);
            when(mMockSyncService.getActiveDataTypes())
                    .thenReturn(syncable
                                    ? CollectionUtil.newHashSet(ModelType.HISTORY_DELETE_DIRECTIVES)
                                    : new HashSet<Integer>());
        });
    }

    private void configureMockSearchEngine() {
        // Cache the actual Url Service, so the test can put it back after tests.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActualTemplateUrlService = TemplateUrlServiceFactory.get(); });

        TemplateUrlServiceFactory.setInstanceForTesting(mMockTemplateUrlService);
        Mockito.doReturn(mMockSearchEngine)
                .when(mMockTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();
    }

    private void waitForOptionsMenu() {
        CriteriaHelper.pollUiThread(() -> {
            return mSettingsActivityTestRule.getActivity().findViewById(R.id.menu_id_general_help)
                    != null;
        });
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.SEARCH_HISTORY_LINK)
    public void testRenderNotSignedIn() throws IOException {
        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        View view = mSettingsActivityTestRule.getActivity()
                            .findViewById(android.R.id.content)
                            .getRootView();
        mRenderTestRule.render(view, "clear_browsing_data_basic_signed_out");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.SEARCH_HISTORY_LINK)
    public void testRenderSignedInNotSyncing() throws IOException {
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();
        // Simulate that Sync was stopped but the primary account remained.
        setSyncable(false);
        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        View view = mSettingsActivityTestRule.getActivity()
                            .findViewById(android.R.id.content)
                            .getRootView();
        mRenderTestRule.render(view, "clear_browsing_data_basic_signed_in_no_sync");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.SEARCH_HISTORY_LINK)
    public void testRenderSignedInAndSyncing() throws IOException {
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();
        setSyncable(true);
        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        View view = mSettingsActivityTestRule.getActivity()
                            .findViewById(android.R.id.content)
                            .getRootView();
        mRenderTestRule.render(view, "clear_browsing_data_basic_signed_in_sync");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.SEARCH_HISTORY_LINK)
    public void testRenderSearchHistoryLinkSignedOutGoogleDSE() throws IOException {
        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        View view = mSettingsActivityTestRule.getActivity()
                            .findViewById(android.R.id.content)
                            .getRootView();
        mRenderTestRule.render(view, "clear_browsing_data_basic_shl_google_signed_out");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.SEARCH_HISTORY_LINK)
    public void testRenderSearchHistoryLinkSignedInGoogleDSE() throws IOException {
        mAccountManagerTestRule.addTestAccountThenSignin();
        setSyncable(false);
        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        View view = mSettingsActivityTestRule.getActivity()
                            .findViewById(android.R.id.content)
                            .getRootView();
        mRenderTestRule.render(view, "clear_browsing_data_basic_shl_google_signed_in");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.SEARCH_HISTORY_LINK)
    public void testRenderSearchHistoryLinkSignedInKnownNonGoogleDSE() throws IOException {
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();
        setSyncable(false);
        configureMockSearchEngine();
        Mockito.doReturn(false).when(mMockTemplateUrlService).isDefaultSearchEngineGoogle();
        Mockito.doReturn(true).when(mMockSearchEngine).getIsPrepopulated();

        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        View view = mSettingsActivityTestRule.getActivity()
                            .findViewById(android.R.id.content)
                            .getRootView();
        mRenderTestRule.render(view, "clear_browsing_data_basic_shl_known_signed_in");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.SEARCH_HISTORY_LINK)
    public void testRenderSearchHistoryLinkSignedInUnknownNonGoogleDSE() throws IOException {
        mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();
        setSyncable(false);
        configureMockSearchEngine();
        Mockito.doReturn(false).when(mMockTemplateUrlService).isDefaultSearchEngineGoogle();
        Mockito.doReturn(false).when(mMockSearchEngine).getIsPrepopulated();

        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        View view = mSettingsActivityTestRule.getActivity()
                            .findViewById(android.R.id.content)
                            .getRootView();
        mRenderTestRule.render(view, "clear_browsing_data_basic_shl_unknown_signed_in");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.SEARCH_HISTORY_LINK)
    public void testRenderSearchHistoryLinkSignedOutKnownNonGoogleDSE() throws IOException {
        configureMockSearchEngine();
        Mockito.doReturn(false).when(mMockTemplateUrlService).isDefaultSearchEngineGoogle();
        Mockito.doReturn(true).when(mMockSearchEngine).getIsPrepopulated();

        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        View view = mSettingsActivityTestRule.getActivity()
                            .findViewById(android.R.id.content)
                            .getRootView();
        mRenderTestRule.render(view, "clear_browsing_data_basic_shl_known_signed_out");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.SEARCH_HISTORY_LINK)
    public void testRenderSearchHistoryLinkSignedOutUnknownNonGoogleDSE() throws IOException {
        configureMockSearchEngine();
        Mockito.doReturn(false).when(mMockTemplateUrlService).isDefaultSearchEngineGoogle();
        Mockito.doReturn(false).when(mMockSearchEngine).getIsPrepopulated();

        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();
        View view = mSettingsActivityTestRule.getActivity()
                            .findViewById(android.R.id.content)
                            .getRootView();
        mRenderTestRule.render(view, "clear_browsing_data_basic_shl_unknown_signed_out");
    }
}

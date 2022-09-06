// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matcher;
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
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.ModelType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.IOException;
import java.util.HashSet;

/**
 * Integration tests for ClearBrowsingDataFragmentBasic.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.ENABLE_CBD_SIGN_OUT})
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
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.PRIVACY)
                    .build();

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
    public void testSignOutLinkNotOfferedToSupervisedAccounts() {
        mSigninTestRule.addChildTestAccountThenWaitForSignin();
        setSyncable(false);
        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();

        final ClearBrowsingDataFragmentBasic clearBrowsingDataFragmentBasic =
                mSettingsActivityTestRule.getFragment();
        onView(withText(clearBrowsingDataFragmentBasic.buildSignOutOfChromeText().toString()))
                .check(doesNotExist());
    }

    @Test
    @LargeTest
    public void testSigningOut() {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        setSyncable(true);
        mSettingsActivityTestRule.startSettingsActivity();
        waitForOptionsMenu();

        final ClearBrowsingDataFragmentBasic clearBrowsingDataFragmentBasic =
                mSettingsActivityTestRule.getFragment();
        onView(withText(clearBrowsingDataFragmentBasic.buildSignOutOfChromeText().toString()))
                .perform(clickOnSignOutLink());
        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        CriteriaHelper.pollUiThread(
                ()
                        -> !IdentityServicesProvider.get()
                                    .getIdentityManager(Profile.getLastUsedRegularProfile())
                                    .hasPrimaryAccount(ConsentLevel.SIGNIN),
                "Account should be signed out!");

        // Footer should be hidden after sign-out.
        onView(withText(clearBrowsingDataFragmentBasic.buildSignOutOfChromeText().toString()))
                .check(doesNotExist());
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderSignedInAndSyncing() throws IOException {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
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
    public void testRenderSearchHistoryLinkSignedInGoogleDSE() throws IOException {
        mSigninTestRule.addTestAccountThenSignin();
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
    public void testRenderSearchHistoryLinkSignedInKnownNonGoogleDSE() throws IOException {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
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
    public void testRenderSearchHistoryLinkSignedInUnknownNonGoogleDSE() throws IOException {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
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

    // TODO(https://crbug.com/1334586): Move this to a test util class.
    private ViewAction clickOnSignOutLink() {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return Matchers.instanceOf(TextView.class);
            }

            @Override
            public String getDescription() {
                return "Clicks on the sign out link in the clear browsing data footer";
            }

            @Override
            public void perform(UiController uiController, View view) {
                TextView textView = (TextView) view;
                Spanned spannedString = (Spanned) textView.getText();
                ClickableSpan[] spans =
                        spannedString.getSpans(0, spannedString.length(), ClickableSpan.class);
                if (spans.length != 1) {
                    throw new NoMatchingViewException.Builder()
                            .includeViewHierarchy(true)
                            .withRootView(textView)
                            .build();
                }
                spans[0].onClick(view);
            }
        };
    }
}

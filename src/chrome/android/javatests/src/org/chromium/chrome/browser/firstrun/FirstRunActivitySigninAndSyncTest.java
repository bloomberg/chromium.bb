// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.not;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.lifecycle.Stage;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
import org.chromium.chrome.browser.search_engines.SearchEnginePromoType;
import org.chromium.chrome.browser.signin.SigninFirstRunFragment;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Integration tests for the first run experience with sign-in and sync decoupled.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.FORCE_ENABLE_SIGNIN_FRE})
public class FirstRunActivitySigninAndSyncTest {
    private static final String TEST_EMAIL = "test.account@gmail.com";
    private static final String CHILD_EMAIL = "child.account@gmail.com";
    private static final String TEST_URL = "https://foo.com";

    @Rule
    public final TestRule mCommandLindFlagRule = CommandLineFlags.getTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    // TODO(crbug.com/1311260): Consider using a test rule to ensure this gets terminated correctly.
    public FirstRunActivity mFirstRunActivity;

    @Mock
    private ExternalAuthUtils mExternalAuthUtilsMock;

    @Mock
    private LocaleManagerDelegate mLocalManagerDelegateMock;

    @Before
    public void setUp() {
        when(mLocalManagerDelegateMock.getSearchEnginePromoShowType())
                .thenReturn(SearchEnginePromoType.DONT_SHOW);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            LocaleManager.getInstance().setDelegateForTest(mLocalManagerDelegateMock);
        });
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
    }

    @Test
    @MediumTest
    public void dismissButtonClickSkipsSyncConsentPageWhenNoAccountsAreOnDevice() {
        launchFirstRunActivity();
        onView(withId(R.id.signin_fre_selected_account)).check(matches(not(isDisplayed())));

        clickButton(R.id.signin_fre_dismiss_button);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void dismissButtonClickSkipsSyncConsentPageWhenOneAccountIsOnDevice() {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivity();
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        clickButton(R.id.signin_fre_dismiss_button);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void continueButtonClickShowsSyncConsentPage() {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivity();
        ensureCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        clickButton(R.id.signin_fre_continue_button);

        ensureCurrentPageIs(SyncConsentFirstRunFragment.class);
    }

    @Test
    @MediumTest
    public void continueButtonClickShowsSyncConsentPageWithChildAccount() {
        mAccountManagerTestRule.addAccount(CHILD_EMAIL);
        launchFirstRunActivity();
        ensureCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        clickButton(R.id.signin_fre_continue_button);

        ensureCurrentPageIs(SyncConsentFirstRunFragment.class);
    }

    @Test
    @MediumTest
    public void continueButtonClickSkipsSyncConsentPageWhenCannotUseGooglePlayServices() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(false);
        launchFirstRunActivity();
        ensureCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(not(isDisplayed())));
        onView(withId(R.id.signin_fre_dismiss_button)).check(matches(not(isDisplayed())));

        clickButton(R.id.signin_fre_continue_button);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void continueButtonClickShowsSearchEnginePageWhenItIsEnabled() {
        when(mLocalManagerDelegateMock.getSearchEnginePromoShowType())
                .thenReturn(SearchEnginePromoType.SHOW_NEW);
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivity();
        ensureCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        clickButton(R.id.signin_fre_continue_button);

        ensureCurrentPageIs(DefaultSearchEngineFirstRunFragment.class);
    }

    @Test
    @MediumTest
    public void dismissButtonClickShowsSearchEnginePageWhenItIsEnabled() {
        when(mLocalManagerDelegateMock.getSearchEnginePromoShowType())
                .thenReturn(SearchEnginePromoType.SHOW_NEW);
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivity();
        ensureCurrentPageIs(SigninFirstRunFragment.class);
        onView(withId(R.id.signin_fre_selected_account)).check(matches(isDisplayed()));

        clickButton(R.id.signin_fre_dismiss_button);

        ensureCurrentPageIs(DefaultSearchEngineFirstRunFragment.class);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ENABLE_SYNC_IMMEDIATELY_IN_FRE})
    public void acceptingSyncEndsFreAndEnablesSyncIfEnableSyncImmediatelyFeatureEnabled() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivity();
        ensureCurrentPageIs(SigninFirstRunFragment.class);
        clickButton(R.id.signin_fre_continue_button);
        ensureCurrentPageIs(SyncConsentFirstRunFragment.class);

        clickButton(R.id.positive_button);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);
        SyncTestUtil.waitForSyncFeatureEnabled();
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.ENABLE_SYNC_IMMEDIATELY_IN_FRE})
    public void acceptingSyncEndsFreAndEnablesSyncIfEnableSyncImmediatelyFeatureDisabled() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivity();
        ensureCurrentPageIs(SigninFirstRunFragment.class);
        clickButton(R.id.signin_fre_continue_button);
        ensureCurrentPageIs(SyncConsentFirstRunFragment.class);

        clickButton(R.id.positive_button);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);
        SyncTestUtil.waitForSyncFeatureEnabled();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ENABLE_SYNC_IMMEDIATELY_IN_FRE})
    public void refusingSyncEndsFreAndDoesNotEnableSyncIfEnableSyncImmediatelyFeatureEnabled() {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivity();
        ensureCurrentPageIs(SigninFirstRunFragment.class);
        clickButton(R.id.signin_fre_continue_button);
        ensureCurrentPageIs(SyncConsentFirstRunFragment.class);

        clickButton(R.id.negative_button);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);

        Assert.assertFalse(SyncTestUtil.canSyncFeatureStart());
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.ENABLE_SYNC_IMMEDIATELY_IN_FRE})
    public void refusingSyncEndsFreAndDoesNotEnableSyncIfEnableSyncImmediatelyFeatureDisabled() {
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivity();
        ensureCurrentPageIs(SigninFirstRunFragment.class);
        clickButton(R.id.signin_fre_continue_button);
        ensureCurrentPageIs(SyncConsentFirstRunFragment.class);

        clickButton(R.id.negative_button);

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);

        Assert.assertFalse(SyncTestUtil.canSyncFeatureStart());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ENABLE_SYNC_IMMEDIATELY_IN_FRE})
    public void
    clickingSettingsEndsFreAndStartsEnablingSyncIfEnableSyncImmediatelyFeatureEnabled() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivity();
        ensureCurrentPageIs(SigninFirstRunFragment.class);
        clickButton(R.id.signin_fre_continue_button);
        ensureCurrentPageIs(SyncConsentFirstRunFragment.class);

        onView(withId(R.id.signin_details_description)).perform(new LinkClick());

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);

        // Sync-the-feature can start but won't become enabled until the user clicks the "Confirm"
        // button in settings.
        SyncTestUtil.waitForCanSyncFeatureStart();
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.ENABLE_SYNC_IMMEDIATELY_IN_FRE})
    public void
    clickingSettingsEndsFreAndStartsEnablingSyncIfEnableSyncImmediatelyFeatureDisabled() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        mAccountManagerTestRule.addAccount(TEST_EMAIL);
        launchFirstRunActivity();
        ensureCurrentPageIs(SigninFirstRunFragment.class);
        clickButton(R.id.signin_fre_continue_button);
        ensureCurrentPageIs(SyncConsentFirstRunFragment.class);

        onView(withId(R.id.signin_details_description)).perform(new LinkClick());

        ApplicationTestUtils.waitForActivityState(mFirstRunActivity, Stage.DESTROYED);

        // Sync-the-feature can start but won't become enabled until the user clicks the "Confirm"
        // button in settings.
        SyncTestUtil.waitForCanSyncFeatureStart();
    }

    private void clickButton(@IdRes int buttonId) {
        // This helps to reduce flakiness on some marshmallow bots in comparison with
        // espresso click.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mFirstRunActivity.findViewById(buttonId).performClick(); });
    }

    private <T extends FirstRunFragment> void ensureCurrentPageIs(Class<T> fragmentClass) {
        CriteriaHelper.pollUiThread(() -> {
            return fragmentClass.isInstance(mFirstRunActivity.getCurrentFragmentForTesting());
        }, fragmentClass.getName() + " should be the current page");
    }

    private void launchFirstRunActivity() {
        final Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        // The FirstRunActivity relaunches the original intent when it finishes, see
        // FirstRunActivityBase.EXTRA_FRE_COMPLETE_LAUNCH_INTENT. So to guarantee that
        // ChromeTabbedActivity gets started, we must ask for more than just FirstRunActivity
        // here.
        final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(TEST_URL));
        intent.setPackage(context.getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mFirstRunActivity = ApplicationTestUtils.waitForActivityWithClass(
                FirstRunActivity.class, Stage.RESUMED, () -> context.startActivity(intent));
        CriteriaHelper.pollUiThread(mFirstRunActivity::isNativeSideIsInitializedForTest);
    }

    private static class LinkClick implements ViewAction {
        @Override
        public Matcher<View> getConstraints() {
            return Matchers.instanceOf(TextView.class);
        }

        @Override
        public String getDescription() {
            return "Clicks on the one and only clickable link in the view.";
        }

        @Override
        public void perform(UiController uiController, View view) {
            final TextView textView = (TextView) view;
            final Spanned spannedString = (Spanned) textView.getText();
            final ClickableSpan[] spans =
                    spannedString.getSpans(0, spannedString.length(), ClickableSpan.class);
            Assert.assertEquals("There should be only one clickable link.", 1, spans.length);
            spans[0].onClick(view);
        }
    };
}

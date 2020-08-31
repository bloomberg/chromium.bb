// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.accounts.Account;
import android.support.test.InstrumentationRegistry;

import androidx.fragment.app.FragmentManager;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Spy;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.AccountManagerTestRule;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

import java.io.IOException;

/**
 * Render tests for {@link AccountPickerDialogFragment}.
 * TODO(https://crbug.com/1032488):
 * Use FragmentScenario to test this fragment once we start using fragment from androidx.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags
        .Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
        @Features.DisableFeatures({ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY})
        public class AccountPickerDialogFragmentTest extends DummyUiActivityTestCase {
    @Rule
    public final Features.JUnitProcessor mProcessor = new Features.JUnitProcessor();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule = new ChromeRenderTestRule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(FakeAccountManagerDelegate.ENABLE_PROFILE_DATA_SOURCE);

    @Spy
    private DummyAccountPickerTargetFragment mTargetFragment =
            new DummyAccountPickerTargetFragment();

    private final String mFullName1 = "Test Account1";

    private final String mAccountName1 = "test.account1@gmail.com";

    private final String mAccountName2 = "test.account2@gmail.com";

    private AccountPickerDialogFragment mDialog;

    @Before
    public void setUp() {
        initMocks(this);
        addAccount(mAccountName1, mFullName1);
        addAccount(mAccountName2, "");
        FragmentManager fragmentManager = getActivity().getSupportFragmentManager();
        fragmentManager.beginTransaction().add(mTargetFragment, "target").commit();
        mDialog = AccountPickerDialogFragment.create(mAccountName1);
        mDialog.setTargetFragment(mTargetFragment, 0);
        mDialog.show(fragmentManager, null);
    }

    @After
    public void tearDown() {
        if (mDialog.getDialog() != null) mDialog.dismiss();
    }

    @Test
    @MediumTest
    public void testTitle() {
        onView(withText(R.string.signin_account_picker_dialog_title)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testAddAccount() {
        onView(withText(R.string.signin_add_account)).perform(click());
        verify(mTargetFragment).addAccount();
    }

    @Test
    @MediumTest
    public void testSelectDefaultAccount() {
        onView(withText(mAccountName1)).check(matches(isDisplayed()));
        onView(withText(mFullName1)).perform(click());
        verify(mTargetFragment).onAccountSelected(mAccountName1, true);
    }

    @Test
    @MediumTest
    public void testSelectNonDefaultAccount() {
        onView(withText(mAccountName2)).perform(click());
        verify(mTargetFragment).onAccountSelected(mAccountName2, false);
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testAccountPickerDialogViewLegacy() throws IOException {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mRenderTestRule.render(
                mDialog.getDialog().getWindow().getDecorView(), "account_picker_dialog_legacy");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testUpdateSelectedAccountChangesSelectionMark() throws IOException {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        TestThreadUtils.runOnUiThreadBlocking(() -> mDialog.updateSelectedAccount(mAccountName2));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mRenderTestRule.render(mDialog.getDialog().getWindow().getDecorView(),
                "account_picker_dialog_update_selected_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @Features.EnableFeatures({ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY})
    public void testAccountPickerDialogView() throws IOException {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mRenderTestRule.render(
                mDialog.getDialog().getWindow().getDecorView(), "account_picker_dialog");
    }

    private void addAccount(String accountName, String fullName) {
        Account account = AccountUtils.createAccountFromName(accountName);
        AccountHolder.Builder accountHolder = AccountHolder.builder(account).alwaysAccept(true);
        ProfileDataSource.ProfileData profileData =
                new ProfileDataSource.ProfileData(accountName, null, fullName, null);
        mAccountManagerTestRule.addAccount(accountHolder.build(), profileData);
    }
}

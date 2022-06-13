// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.os.Build.VERSION_CODES;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountRenameChecker;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;

/**
 * This class tests the sign-in checks done at Chrome start-up or when accounts
 * change on device.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableIf.Build(sdk_is_less_than = VERSION_CODES.LOLLIPOP_MR1)
public class SigninCheckerTest {
    private static final String CHILD_ACCOUNT_NAME =
            AccountManagerTestRule.generateChildEmail("test@gmail.com");

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Mock
    private ExternalAuthUtils mExternalAuthUtilsMock;

    @Mock
    private AccountRenameChecker.Delegate mAccountRenameCheckerDelegateMock;

    @Before
    public void setUp() {
        AccountRenameChecker.overrideDelegateForTests(mAccountRenameCheckerDelegateMock);
    }

    @Test
    @MediumTest
    @FlakyTest(message = "https://crbug.com/1205346")
    public void signinWhenPrimaryAccountIsRenamedToAKnownAccount() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mAccountManagerTestRule.addAccountAndWaitForSeeding("the.second.account@gmail.com");
        final CoreAccountInfo oldAccount =
                mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();
        final String newAccountEmail = "test.new.account@gmail.com";
        when(mAccountRenameCheckerDelegateMock.getNewNameOfRenamedAccount(oldAccount.getEmail()))
                .thenReturn(newAccountEmail);
        final CoreAccountInfo expectedPrimaryAccount =
                mAccountManagerTestRule.addAccount(newAccountEmail);

        mAccountManagerTestRule.removeAccountAndWaitForSeeding(oldAccount.getEmail());

        CriteriaHelper.pollUiThread(() -> {
            return expectedPrimaryAccount.equals(
                    mAccountManagerTestRule.getPrimaryAccount(ConsentLevel.SYNC));
        });
    }

    @Test
    @MediumTest
    @FlakyTest(message = "https://crbug.com/1205346")
    public void signoutWhenPrimaryAccountIsRenamedToAnUnknownAccount() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mAccountManagerTestRule.addAccountAndWaitForSeeding("the.second.account@gmail.com");
        final CoreAccountInfo oldAccount =
                mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();
        final String newAccountEmail = "test.new.account@gmail.com";
        when(mAccountRenameCheckerDelegateMock.getNewNameOfRenamedAccount(oldAccount.getEmail()))
                .thenReturn(newAccountEmail);

        mAccountManagerTestRule.removeAccountAndWaitForSeeding(oldAccount.getEmail());

        CriteriaHelper.pollUiThread(() -> {
            return !IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .hasPrimaryAccount(ConsentLevel.SYNC);
        });
        Assert.assertNull(mAccountManagerTestRule.getPrimaryAccount(ConsentLevel.SYNC));
    }

    @Test
    @MediumTest
    @FlakyTest(message = "https://crbug.com/1205346")
    public void signoutWhenPrimaryAccountIsRemoved() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mAccountManagerTestRule.addAccountAndWaitForSeeding("the.second.account@gmail.com");
        final CoreAccountInfo oldAccount =
                mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();

        mAccountManagerTestRule.removeAccountAndWaitForSeeding(oldAccount.getEmail());

        CriteriaHelper.pollUiThread(() -> {
            return !IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .hasPrimaryAccount(ConsentLevel.SYNC);
        });
        Assert.assertNull(mAccountManagerTestRule.getPrimaryAccount(ConsentLevel.SYNC));
    }

    @Test
    @MediumTest
    @FlakyTest(message = "https://crbug.com/1205346")
    public void signoutWhenPrimaryAccountWithoutSyncConsentIsRemoved() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mAccountManagerTestRule.addAccountAndWaitForSeeding("the.second.account@gmail.com");
        final CoreAccountInfo oldAccount = mAccountManagerTestRule.addTestAccountThenSignin();

        mAccountManagerTestRule.removeAccountAndWaitForSeeding(oldAccount.getEmail());

        CriteriaHelper.pollUiThread(() -> {
            return !IdentityServicesProvider.get()
                            .getIdentityManager(Profile.getLastUsedRegularProfile())
                            .hasPrimaryAccount(ConsentLevel.SIGNIN);
        });
    }

    @Test
    @MediumTest
    public void signinWhenChildAccountIsTheOnlyAccount() {
        mActivityTestRule.startMainActivityOnBlankPage();
        UserActionTester actionTester = new UserActionTester();

        final CoreAccountInfo expectedPrimaryAccount =
                mAccountManagerTestRule.addAccountAndWaitForSeeding(CHILD_ACCOUNT_NAME);

        CriteriaHelper.pollUiThread(() -> {
            return expectedPrimaryAccount.equals(
                    mAccountManagerTestRule.getPrimaryAccount(ConsentLevel.SYNC));
        });
        Assert.assertEquals(
                2, SigninCheckerProvider.get().getNumOfChildAccountChecksDoneForTests());
        Assert.assertTrue(
                actionTester.getActions().contains("Signin_Signin_WipeDataOnChildAccountSignin2"));
    }

    @Test
    @MediumTest
    public void noSigninWhenChildAccountIsTheOnlyAccountButSigninIsNotAllowed() {
        mActivityTestRule.startMainActivityOnBlankPage();
        UserActionTester actionTester = new UserActionTester();
        when(mExternalAuthUtilsMock.isGooglePlayServicesMissing(any())).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);

        mAccountManagerTestRule.addAccountAndWaitForSeeding(CHILD_ACCOUNT_NAME);

        // The check should be done twice, once at activity start-up, the other when account
        // is added.
        CriteriaHelper.pollUiThread(() -> {
            return SigninCheckerProvider.get().getNumOfChildAccountChecksDoneForTests() == 2;
        });
        Assert.assertNull(mAccountManagerTestRule.getPrimaryAccount(ConsentLevel.SYNC));
        Assert.assertFalse(
                actionTester.getActions().contains("Signin_Signin_WipeDataOnChildAccountSignin2"));
    }

    @Test
    @MediumTest
    public void noSigninWhenChildAccountIsTheSecondaryAccount() {
        // If a child account co-exists with another account on the device, then the child account
        // must be the first device (this is enforced by the Kids Module).  The behaviour in this
        // test case therefore is not currently hittable on a real device; however it is included
        // here for completeness.
        mAccountManagerTestRule.addAccount("the.default.account@gmail.com");
        mAccountManagerTestRule.addAccount(CHILD_ACCOUNT_NAME);

        mActivityTestRule.startMainActivityOnBlankPage();
        UserActionTester actionTester = new UserActionTester();

        // The check should be done once at activity start-up
        CriteriaHelper.pollUiThread(() -> {
            return SigninCheckerProvider.get().getNumOfChildAccountChecksDoneForTests() == 1;
        });
        Assert.assertNull(mAccountManagerTestRule.getPrimaryAccount(ConsentLevel.SYNC));
        Assert.assertFalse(
                actionTester.getActions().contains("Signin_Signin_WipeDataOnChildAccountSignin2"));
    }

    @Test
    @MediumTest
    public void signinWhenChildAccountIsFirstAccount() {
        final CoreAccountInfo childAccount = mAccountManagerTestRule.addAccount(CHILD_ACCOUNT_NAME);
        mAccountManagerTestRule.addAccount("the.second.account@gmail.com");

        mActivityTestRule.startMainActivityOnBlankPage();
        UserActionTester actionTester = new UserActionTester();

        CriteriaHelper.pollUiThread(() -> {
            return childAccount.equals(
                    mAccountManagerTestRule.getPrimaryAccount(ConsentLevel.SYNC));
        });

        // The check should be done once at activity start-up
        Assert.assertEquals(
                1, SigninCheckerProvider.get().getNumOfChildAccountChecksDoneForTests());
        Assert.assertTrue(
                actionTester.getActions().contains("Signin_Signin_WipeDataOnChildAccountSignin2"));
    }
}

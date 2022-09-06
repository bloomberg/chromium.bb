// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityMutator;
import org.chromium.components.signin.metrics.SignoutDelete;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.HashSet;

/**
 * Integration test for the IdentityManager.
 *
 * These tests initialize the native part of the service.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class IdentityManagerIntegrationTest {
    @Rule
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    private static final String TEST_ACCOUNT1 = "foo@gmail.com";
    private static final String TEST_ACCOUNT2 = "bar@gmail.com";

    private CoreAccountInfo mTestAccount1;
    private CoreAccountInfo mTestAccount2;

    private IdentityMutator mIdentityMutator;
    private IdentityManager mIdentityManager;

    @Mock
    private SigninManager.SignInStateObserver mSignInStateObserverMock;

    @Before
    public void setUp() {
        mTestAccount1 = mSigninTestRule.toCoreAccountInfo(TEST_ACCOUNT1);
        mTestAccount2 = mSigninTestRule.toCoreAccountInfo(TEST_ACCOUNT2);

        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();

        mSigninTestRule.waitForSeeding();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = Profile.getLastUsedRegularProfile();
            SigninManagerImpl signinManager =
                    (SigninManagerImpl) IdentityServicesProvider.get().getSigninManager(profile);
            signinManager.addSignInStateObserver(mSignInStateObserverMock);
            mIdentityMutator = signinManager.getIdentityMutatorForTesting();
            mIdentityManager = IdentityServicesProvider.get().getIdentityManager(profile);
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(null); });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListNoAccountsRegisteredAndNoSignedInUser() {
        Assert.assertArrayEquals("Initial state: getAccounts must be empty",
                new CoreAccountInfo[] {}, mIdentityManager.getAccountsWithRefreshTokens());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Run test.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(null);

            Assert.assertArrayEquals("No account: getAccounts must be empty",
                    new CoreAccountInfo[] {}, mIdentityManager.getAccountsWithRefreshTokens());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListOneAccountsRegisteredAndNoSignedInUser() {
        mSigninTestRule.addAccountAndWaitForSeeding(TEST_ACCOUNT1);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Run test.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(null);

            Assert.assertArrayEquals("No signed in account: getAccounts must be empty",
                    new CoreAccountInfo[] {}, mIdentityManager.getAccountsWithRefreshTokens());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListOneAccountsRegisteredSignedIn() {
        mSigninTestRule.addAccountAndWaitForSeeding(TEST_ACCOUNT1);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Run test.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertArrayEquals("Signed in: one account should be available",
                    new CoreAccountInfo[] {mTestAccount1},
                    mIdentityManager.getAccountsWithRefreshTokens());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListOneAccountsRegisteredSignedInOther() {
        mSigninTestRule.addAccountAndWaitForSeeding(TEST_ACCOUNT1);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Run test.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount2.getId());

            Assert.assertArrayEquals(
                    "Signed in but different account, getAccounts must remain empty",
                    new CoreAccountInfo[] {}, mIdentityManager.getAccountsWithRefreshTokens());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListSingleAccountThenAddOne() {
        mSigninTestRule.addAccountAndWaitForSeeding(TEST_ACCOUNT1);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Run one validation.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertArrayEquals("Signed in and one account available",
                    new CoreAccountInfo[] {mTestAccount1},
                    mIdentityManager.getAccountsWithRefreshTokens());
        });

        // Add another account.
        mSigninTestRule.addAccountAndWaitForSeeding(TEST_ACCOUNT2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Re-run validation.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertEquals("Signed in and two accounts available",
                    new HashSet<>(Arrays.asList(mTestAccount1, mTestAccount2)),
                    new HashSet<>(Arrays.asList(mIdentityManager.getAccountsWithRefreshTokens())));
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListTwoAccountsThenRemoveOne() {
        // Add accounts.
        mSigninTestRule.addAccountAndWaitForSeeding(TEST_ACCOUNT1);
        mSigninTestRule.addAccountAndWaitForSeeding(TEST_ACCOUNT2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Run one validation.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertEquals("Signed in and two accounts available",
                    new HashSet<>(Arrays.asList(mTestAccount1, mTestAccount2)),
                    new HashSet<>(Arrays.asList(mIdentityManager.getAccountsWithRefreshTokens())));
        });

        mSigninTestRule.removeAccountAndWaitForSeeding(TEST_ACCOUNT2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertArrayEquals(
                    "Only one account available, account2 should not be returned anymore",
                    new CoreAccountInfo[] {mTestAccount1},
                    mIdentityManager.getAccountsWithRefreshTokens());
        });
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1294295")
    public void testUpdateAccountListTwoAccountsThenRemoveAll() {
        // Add accounts.
        mSigninTestRule.addAccountAndWaitForSeeding(TEST_ACCOUNT1);
        mSigninTestRule.addAccountAndWaitForSeeding(TEST_ACCOUNT2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertEquals("Signed in and two accounts available",
                    new HashSet<>(Arrays.asList(mTestAccount1, mTestAccount2)),
                    new HashSet<>(Arrays.asList(mIdentityManager.getAccountsWithRefreshTokens())));
        });

        // Remove all.
        mSigninTestRule.removeAccountAndWaitForSeeding(TEST_ACCOUNT1);
        mSigninTestRule.removeAccountAndWaitForSeeding(TEST_ACCOUNT2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Re-validate and run checks.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertArrayEquals("No account available", new CoreAccountInfo[] {},
                    mIdentityManager.getAccountsWithRefreshTokens());
        });
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1295158")
    public void testUpdateAccountListTwoAccountsThenRemoveAllSignOut() {
        // Add accounts.
        mSigninTestRule.addAccountAndWaitForSeeding(TEST_ACCOUNT1);
        mSigninTestRule.addAccountAndWaitForSeeding(TEST_ACCOUNT2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertEquals("Signed in and two accounts available",
                    new HashSet<>(Arrays.asList(mTestAccount1, mTestAccount2)),
                    new HashSet<>(Arrays.asList(mIdentityManager.getAccountsWithRefreshTokens())));
        });

        mSigninTestRule.removeAccountAndWaitForSeeding(TEST_ACCOUNT1);
        mSigninTestRule.removeAccountAndWaitForSeeding(TEST_ACCOUNT2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Re-validate and run checks.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(null);

            Assert.assertArrayEquals("Not signed in and no accounts available",
                    new CoreAccountInfo[] {}, mIdentityManager.getAccountsWithRefreshTokens());
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListTwoAccountsRegisteredAndOneSignedIn() {
        // Add accounts.
        mSigninTestRule.addAccountAndWaitForSeeding(TEST_ACCOUNT1);
        mSigninTestRule.addAccountAndWaitForSeeding(TEST_ACCOUNT2);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Run test.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertEquals("Signed in and two accounts available",
                    new HashSet<>(Arrays.asList(mTestAccount1, mTestAccount2)),
                    new HashSet<>(Arrays.asList(mIdentityManager.getAccountsWithRefreshTokens())));
        });
    }

    @Test
    @MediumTest
    public void testUpdateAccountListNoAccountsRegisteredButSignedIn() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Run test.
            mIdentityMutator.reloadAllAccountsFromSystemWithPrimaryAccount(mTestAccount1.getId());

            Assert.assertArrayEquals("No accounts available", new CoreAccountInfo[] {},
                    mIdentityManager.getAccountsWithRefreshTokens());
        });
    }

    @Test
    @MediumTest
    public void testClearPrimaryAccountWithSyncNotEnabled_signsOut() {
        // Add accounts.
        mSigninTestRule.addTestAccountThenSignin();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN));

            // Run test.
            mIdentityMutator.clearPrimaryAccount(
                    SignoutReason.SIGNOUT_TEST, SignoutDelete.IGNORE_METRIC);

            // Check the account is signed out
            Assert.assertFalse(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN));
        });

        // Wait for the operation to have completed - the revokeSyncConsent processing calls back
        // SigninManager, and if we don't wait for this to complete before test teardown then we
        // can hit a race condition where this async processing overlaps with the signout causing
        // teardown to fail.
        verify(mSignInStateObserverMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .onSignedOut();
    }

    @Test
    @MediumTest
    public void testClearPrimaryAccountWithSyncEnabled_signsOut() {
        // Add accounts.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(mIdentityManager.hasPrimaryAccount(ConsentLevel.SYNC));

            // Run test.
            mIdentityMutator.clearPrimaryAccount(
                    SignoutReason.SIGNOUT_TEST, SignoutDelete.IGNORE_METRIC);

            Assert.assertFalse(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN));
        });

        // Wait for the operation to have completed - the revokeSyncConsent processing calls back
        // SigninManager, and if we don't wait for this to complete before test teardown then we
        // can hit a race condition where this async processing overlaps with the signout causing
        // teardown to fail.
        verify(mSignInStateObserverMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .onSignedOut();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ALLOW_SYNC_OFF_FOR_CHILD_ACCOUNTS})
    public void testRevokeSyncConsent_whenSyncOffAlwaysAllowed_disablesSync() {
        // Add account.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(mIdentityManager.hasPrimaryAccount(ConsentLevel.SYNC));

            // Run test.
            mIdentityMutator.revokeSyncConsent(
                    SignoutReason.SIGNOUT_TEST, SignoutDelete.IGNORE_METRIC);

            Assert.assertFalse(mIdentityManager.hasPrimaryAccount(ConsentLevel.SYNC));
            Assert.assertTrue(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN));
        });

        // Wait for the operation to have completed - the revokeSyncConsent processing calls back
        // SigninManager, and if we don't wait for this to complete before test teardown then we
        // can hit a race condition where this async processing overlaps with the signout causing
        // teardown to fail.
        verify(mSignInStateObserverMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .onSignedOut();
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.ALLOW_SYNC_OFF_FOR_CHILD_ACCOUNTS})
    public void testRevokeSyncConsent_whenSyncOffNotAlwaysAllowed_signsOut() {
        // Add accounts.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(mIdentityManager.hasPrimaryAccount(ConsentLevel.SYNC));

            // Run test.
            mIdentityMutator.revokeSyncConsent(
                    SignoutReason.SIGNOUT_TEST, SignoutDelete.IGNORE_METRIC);

            Assert.assertFalse(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN));
        });

        // Wait for the operation to have completed - the revokeSyncConsent processing calls back
        // SigninManager, and if we don't wait for this to complete before test teardown then we
        // can hit a race condition where this async processing overlaps with the signout causing
        // teardown to fail.
        verify(mSignInStateObserverMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .onSignedOut();
    }
}

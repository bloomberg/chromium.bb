// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.Manifest;
import android.accounts.Account;
import android.accounts.AccountManager;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.UserManager;

import androidx.test.rule.GrantPermissionRule;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.android.util.concurrent.RoboExecutorService;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowAccountManager;
import org.robolectric.shadows.ShadowUserManager;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.UmaRecorder;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountManagerDelegate.CapabilityResponse;
import org.chromium.components.signin.AccountManagerFacade.ChildAccountStatusListener;
import org.chromium.components.signin.test.util.AccountHolder;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;

import java.util.List;

/**
 * Robolectric tests for {@link AccountManagerFacade}. See also {@link AccountManagerFacadeTest}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {CustomShadowAsyncTask.class, ShadowUserManager.class,
                ShadowAccountManager.class})
public class AccountManagerFacadeImplTest {
    private static final String TEST_TOKEN_SCOPE = "test-token-scope";

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final GrantPermissionRule mGrantPermissionRule =
            GrantPermissionRule.grant(Manifest.permission.GET_ACCOUNTS);

    @Mock
    private UmaRecorder mUmaRecorderMock;

    @Mock
    ExternalAuthUtils mExternalAuthUtilsMock;

    @Mock
    private AccountsChangeObserver mObserverMock;

    @Mock
    private ChildAccountStatusListener mChildAccountStatusListenerMock;

    private final Context mContext = RuntimeEnvironment.application;
    private ShadowUserManager mShadowUserManager;
    private ShadowAccountManager mShadowAccountManager;
    private FakeAccountManagerDelegate mDelegate;
    private AccountManagerFacade mFacade;

    // Prefer to use the facade with the real system delegate instead of the fake delegate
    // to test the facade more thoroughly
    private AccountManagerFacade mFacadeWithSystemDelegate;

    @Before
    public void setUp() {
        PostTask.setPrenativeThreadPoolExecutorForTesting(new RoboExecutorService());
        UmaRecorderHolder.setNonNativeDelegate(mUmaRecorderMock);
        when(mExternalAuthUtilsMock.canUseGooglePlayServices()).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);

        mShadowUserManager =
                shadowOf((UserManager) mContext.getSystemService(Context.USER_SERVICE));
        mShadowAccountManager = shadowOf(AccountManager.get(mContext));
        ThreadUtils.setThreadAssertsDisabledForTesting(true);
        mDelegate = spy(new FakeAccountManagerDelegate());
        mFacade = new AccountManagerFacadeImpl(mDelegate);

        mFacadeWithSystemDelegate =
                new AccountManagerFacadeImpl(new SystemAccountManagerDelegate());
    }

    @After
    public void tearDown() {
        PostTask.resetPrenativeThreadPoolExecutorForTesting();
    }

    @Test
    public void testAccountsChangerObservationInitialization() {
        mFacadeWithSystemDelegate.addObserver(mObserverMock);
        verify(mObserverMock, never()).onAccountsChanged();

        mContext.sendBroadcast(new Intent(AccountManager.LOGIN_ACCOUNTS_CHANGED_ACTION));

        verify(mObserverMock).onAccountsChanged();
    }

    @Test
    public void testCountOfAccountLoggedAfterAccountsFetched() {
        addTestAccount("test@gmail.com");

        AccountManagerFacade facade = new AccountManagerFacadeImpl(mDelegate);

        verify(mUmaRecorderMock)
                .recordLinearHistogram("Signin.AndroidNumberOfDeviceAccounts", 1, 1, 50, 51);
    }

    @Test
    public void testCanonicalAccount() {
        addTestAccount("test@gmail.com");
        List<Account> accounts = mFacade.getAccounts().getResult();

        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "test@gmail.com"));
        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "Test@gmail.com"));
        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "te.st@gmail.com"));
        Assert.assertNull(AccountUtils.findAccountByName(accounts, "te@googlemail.com"));
    }

    // If this test starts flaking, please re-open crbug.com/568636 and make sure there is some sort
    // of stack trace or error message in that bug BEFORE disabling the test.
    @Test
    public void testNonCanonicalAccount() {
        addTestAccount("test.me@gmail.com");
        List<Account> accounts = mFacade.getAccounts().getResult();

        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "test.me@gmail.com"));
        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "testme@gmail.com"));
        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "Testme@gmail.com"));
        Assert.assertNotNull(AccountUtils.findAccountByName(accounts, "te.st.me@gmail.com"));
    }

    @Test
    public void testGetAccounts() {
        Assert.assertEquals(List.of(), mFacade.getAccounts().getResult());

        Account account = addTestAccount("test@gmail.com");
        Assert.assertEquals(List.of(account), mFacade.getAccounts().getResult());

        Account account2 = addTestAccount("test2@gmail.com");
        Assert.assertEquals(List.of(account, account2), mFacade.getAccounts().getResult());

        Account account3 = addTestAccount("test3@gmail.com");
        Assert.assertEquals(
                List.of(account, account2, account3), mFacade.getAccounts().getResult());

        removeTestAccount(account2);
        Assert.assertEquals(List.of(account, account3), mFacade.getAccounts().getResult());
    }

    @Test
    public void testGetAccountsWithAccountPattern() {
        setAccountRestrictionPatterns("*@example.com");
        Account account = addTestAccount("test@example.com");
        Assert.assertEquals(List.of(account), mFacade.getAccounts().getResult());

        addTestAccount("test@gmail.com"); // Doesn't match the pattern.
        Assert.assertEquals(List.of(account), mFacade.getAccounts().getResult());

        Account account2 = addTestAccount("test2@example.com");
        Assert.assertEquals(List.of(account, account2), mFacade.getAccounts().getResult());

        addTestAccount("test2@gmail.com"); // Doesn't match the pattern.
        Assert.assertEquals(List.of(account, account2), mFacade.getAccounts().getResult());

        removeTestAccount(account);
        Assert.assertEquals(List.of(account2), mFacade.getAccounts().getResult());
    }

    @Test
    public void testGetAccountsWithTwoAccountPatterns() {
        setAccountRestrictionPatterns("test1@example.com", "test2@gmail.com");
        addTestAccount("test@gmail.com"); // Doesn't match the pattern.
        addTestAccount("test@example.com"); // Doesn't match the pattern.
        Assert.assertEquals(List.of(), mFacade.getAccounts().getResult());

        Account account = addTestAccount("test1@example.com");
        Assert.assertEquals(List.of(account), mFacade.getAccounts().getResult());

        addTestAccount("test2@example.com"); // Doesn't match the pattern.
        Assert.assertEquals(List.of(account), mFacade.getAccounts().getResult());

        Account account2 = addTestAccount("test2@gmail.com");
        Assert.assertEquals(List.of(account, account2), mFacade.getAccounts().getResult());
    }

    @Test
    public void testGetAccountsWithAccountPatternsChange() {
        Assert.assertEquals(List.of(), mFacade.getAccounts().getResult());

        Account account = addTestAccount("test@gmail.com");
        Assert.assertEquals(List.of(account), mFacade.getAccounts().getResult());

        Account account2 = addTestAccount("test2@example.com");
        Assert.assertEquals(List.of(account, account2), mFacade.getAccounts().getResult());

        Account account3 = addTestAccount("test3@gmail.com");
        Assert.assertEquals(
                List.of(account, account2, account3), mFacade.getAccounts().getResult());

        setAccountRestrictionPatterns("test@gmail.com");
        Assert.assertEquals(List.of(account), mFacade.getAccounts().getResult());

        setAccountRestrictionPatterns("*@example.com", "test3@gmail.com");
        Assert.assertEquals(List.of(account2, account3), mFacade.getAccounts().getResult());

        removeTestAccount(account3);
        Assert.assertEquals(List.of(account2), mFacade.getAccounts().getResult());
    }

    @Test
    public void testGetAccountsWithAccountPatternsCleared() {
        final Account account1 = addTestAccount("test1@gmail.com");
        final Account account2 = addTestAccount("testexample2@example.com");
        setAccountRestrictionPatterns("*@example.com");
        Assert.assertEquals(List.of(account2), mFacade.getAccounts().getResult());

        mShadowUserManager.setApplicationRestrictions(mContext.getPackageName(), new Bundle());
        mContext.sendBroadcast(new Intent(Intent.ACTION_APPLICATION_RESTRICTIONS_CHANGED));

        Assert.assertEquals(List.of(account1, account2), mFacade.getAccounts().getResult());
    }

    @Test
    public void testGetAccountsMultipleMatchingPatterns() {
        setAccountRestrictionPatterns("*@gmail.com", "test@gmail.com");
        Account account = addTestAccount("test@gmail.com"); // Matches both patterns
        Assert.assertEquals(List.of(account), mFacade.getAccounts().getResult());
    }

    @Test
    public void testCheckChildAccountForRegularChild() {
        final Account account = setFeaturesForAccount(
                "uca@gmail.com", AccountManagerFacadeImpl.FEATURE_IS_CHILD_ACCOUNT_KEY);

        mFacadeWithSystemDelegate.checkChildAccountStatus(account, mChildAccountStatusListenerMock);

        verify(mChildAccountStatusListenerMock).onStatusReady(ChildAccountStatus.REGULAR_CHILD);
    }

    @Test
    public void testCheckChildAccountForUSMChild() {
        final Account account = setFeaturesForAccount(
                "usm@gmail.com", AccountManagerFacadeImpl.FEATURE_IS_USM_ACCOUNT_KEY);

        mFacadeWithSystemDelegate.checkChildAccountStatus(account, mChildAccountStatusListenerMock);

        verify(mChildAccountStatusListenerMock).onStatusReady(ChildAccountStatus.USM_CHILD);
    }

    @Test
    public void testCheckChildAccountForRegularUSMChild() {
        final Account account = setFeaturesForAccount("usm_uca@gmail.com",
                AccountManagerFacadeImpl.FEATURE_IS_USM_ACCOUNT_KEY,
                AccountManagerFacadeImpl.FEATURE_IS_CHILD_ACCOUNT_KEY);

        mFacadeWithSystemDelegate.checkChildAccountStatus(account, mChildAccountStatusListenerMock);

        verify(mChildAccountStatusListenerMock).onStatusReady(ChildAccountStatus.REGULAR_CHILD);
    }

    @Test
    public void testCheckChildAccountForAdult() {
        final Account account = setFeaturesForAccount("adult@gmail.com");

        mFacadeWithSystemDelegate.checkChildAccountStatus(account, mChildAccountStatusListenerMock);

        verify(mChildAccountStatusListenerMock).onStatusReady(ChildAccountStatus.NOT_CHILD);
    }

    @Test
    public void testCanOfferExtendedSyncPromosForUnknownAccount() {
        final Account account = AccountUtils.createAccountFromName("test@gmail.com");

        Assert.assertFalse(mFacade.canOfferExtendedSyncPromos(account).isPresent());
    }

    @Test
    public void testAccountCanNotOfferExtendedSyncPromos() {
        final AccountHolder accountHolder = AccountHolder.createFromEmail("test@gmail.com");
        mDelegate.addAccount(accountHolder);

        Assert.assertFalse(mFacade.canOfferExtendedSyncPromos(accountHolder.getAccount()).get());
    }

    @Test
    public void testAccountCanNotOfferExtendedSyncPromosWhenExceptionCodeReturns() {
        final AccountHolder accountHolder = AccountHolder.createFromEmail("test@gmail.com");
        doReturn(CapabilityResponse.EXCEPTION)
                .when(mDelegate)
                .hasCapability(eq(accountHolder.getAccount()), any());
        mDelegate.addAccount(accountHolder);

        Assert.assertFalse(mFacade.canOfferExtendedSyncPromos(accountHolder.getAccount()).get());
    }

    @Test
    public void testAccountCanOfferExtendedSyncPromos() {
        final AccountHolder accountHolder1 = AccountHolder.createFromEmail("test1@gmail.com");
        mDelegate.addAccount(accountHolder1);
        final AccountHolder accountHolder2 = AccountHolder.createFromEmailAndFeatures(
                "test2@gmail.com", AccountManagerFacadeImpl.CAN_OFFER_EXTENDED_CHROME_SYNC_PROMOS);
        mDelegate.addAccount(accountHolder2);

        Assert.assertFalse(mFacade.canOfferExtendedSyncPromos(accountHolder1.getAccount()).get());
        Assert.assertTrue(mFacade.canOfferExtendedSyncPromos(accountHolder2.getAccount()).get());
    }

    @Test
    public void testGetAccessToken() throws AuthException {
        final Account account = AccountUtils.createAccountFromName("test@gmail.com");
        final AccessTokenData originalToken =
                mFacadeWithSystemDelegate.getAccessToken(account, TEST_TOKEN_SCOPE);

        Assert.assertEquals("The same token should be returned.",
                mFacadeWithSystemDelegate.getAccessToken(account, TEST_TOKEN_SCOPE).getToken(),
                originalToken.getToken());
    }

    @Test
    public void testGetAndInvalidateAccessToken() throws AuthException {
        final Account account = addTestAccount("test@gmail.com");
        final AccessTokenData originalToken = mFacade.getAccessToken(account, TEST_TOKEN_SCOPE);
        Assert.assertEquals("The same token should be returned before invalidating the token.",
                mFacade.getAccessToken(account, TEST_TOKEN_SCOPE).getToken(),
                originalToken.getToken());

        mFacade.invalidateAccessToken(originalToken.getToken());

        final AccessTokenData newToken = mFacade.getAccessToken(account, TEST_TOKEN_SCOPE);
        Assert.assertNotEquals(
                "A different token should be returned since the original token is invalidated.",
                newToken.getToken(), originalToken.getToken());
    }

    @Test(expected = IllegalStateException.class)
    public void testAccountManagerFacadeProviderGetNullInstance() {
        AccountManagerFacadeProvider.getInstance();
    }

    private Account setFeaturesForAccount(String email, String... features) {
        final Account account = AccountUtils.createAccountFromName(email);
        mShadowAccountManager.setFeatures(account, features);
        return account;
    }

    private void setAccountRestrictionPatterns(String... patterns) {
        Bundle restrictions = new Bundle();
        restrictions.putStringArray("RestrictAccountsToPatterns", patterns);
        mShadowUserManager.setApplicationRestrictions(mContext.getPackageName(), restrictions);
        mContext.sendBroadcast(new Intent(Intent.ACTION_APPLICATION_RESTRICTIONS_CHANGED));
    }

    private Account addTestAccount(String accountEmail) {
        final AccountHolder holder = AccountHolder.createFromEmail(accountEmail);
        mDelegate.addAccount(holder);
        return holder.getAccount();
    }

    private void removeTestAccount(Account account) {
        mDelegate.removeAccount(AccountHolder.createFromAccount(account));
    }
}

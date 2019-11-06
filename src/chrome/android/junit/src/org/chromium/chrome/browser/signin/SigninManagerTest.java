// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.accounts.Account;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountTrackerService;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.test.util.MockSyncContentResolverDelegate;

import java.util.concurrent.atomic.AtomicInteger;

/** Tests for {@link SigninManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SigninManagerTest {
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    SigninManager.Natives mNativeMock;

    @Mock
    SigninManagerDelegate mDelegateMock;

    private AccountTrackerService mAccountTrackerService;
    private SigninManager mSigninManager;

    @Before
    public void setUp() {
        initMocks(this);

        mocker.mock(SigninManagerJni.TEST_HOOKS, mNativeMock);
        doReturn(0l).when(mNativeMock).init(any());
        doReturn(true).when(mNativeMock).isSigninAllowedByPolicy(any(), anyLong());

        mAccountTrackerService = mock(AccountTrackerService.class);
        AndroidSyncSettings androidSyncSettings = mock(AndroidSyncSettings.class);

        mSigninManager = spy(new SigninManager(ContextUtils.getApplicationContext(), mDelegateMock,
                mAccountTrackerService, androidSyncSettings));

        // SigninManager interacts with AndroidSyncSettings, but its not the focus
        // of this test. Using MockSyncContentResolver reduces burden of test setup.
        AndroidSyncSettings.overrideForTests(new MockSyncContentResolverDelegate(), null);
    }

    @Test
    public void signOutFromJavaWithManagedDomain() {
        // Stub out various native calls. Some of these are verified as never called
        // and those stubs simply allow that verification to catch any issues.
        doNothing().when(mNativeMock).signOut(any(), anyLong(), anyInt());
        doNothing().when(mNativeMock).wipeProfileData(any(), anyLong());
        doNothing().when(mNativeMock).wipeGoogleServiceWorkerCaches(any(), anyLong());
        // See verification of nativeWipeProfileData below.
        doReturn("TestDomain").when(mNativeMock).getManagementDomain(any(), anyLong());

        // Trigger the sign out flow!
        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST);

        // nativeSignOut should be called *before* clearing any account data.
        // http://crbug.com/589028
        verify(mNativeMock, times(1)).signOut(any(), anyLong(), eq(SignoutReason.SIGNOUT_TEST));
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(any(), anyLong());
        verify(mNativeMock, never()).wipeProfileData(any(), anyLong());

        // Simulate native callback to trigger clearing of account data.
        mSigninManager.onNativeSignOut();

        // Sign-out should wipe all profile data when user has managed domain.
        verify(mNativeMock, times(1)).wipeProfileData(any(), anyLong());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(any(), anyLong());
    }

    @Test
    public void signOutFromJavaWithNullDomain() {
        // Stub out various native calls. Some of these are verified as never called
        // and those stubs simply allow that verification to catch any issues.
        doNothing().when(mNativeMock).signOut(any(), anyLong(), anyInt());
        doNothing().when(mNativeMock).wipeProfileData(any(), anyLong());
        doNothing().when(mNativeMock).wipeGoogleServiceWorkerCaches(any(), anyLong());
        // See verification of nativeWipeGoogleServiceWorkerCaches below.
        doReturn(null).when(mNativeMock).getManagementDomain(any(), anyLong());

        // Trigger the sign out flow!
        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST);

        // nativeSignOut should be called *before* clearing any account data.
        // http://crbug.com/589028
        verify(mNativeMock, times(1)).signOut(any(), anyLong(), eq(SignoutReason.SIGNOUT_TEST));
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(any(), anyLong());
        verify(mNativeMock, never()).wipeProfileData(any(), anyLong());

        // Simulate native callback to trigger clearing of account data.
        mSigninManager.onNativeSignOut();

        // Sign-out should only clear the service worker cache when the domain is null.
        verify(mNativeMock, never()).wipeProfileData(any(), anyLong());
        verify(mNativeMock, times(1)).wipeGoogleServiceWorkerCaches(any(), anyLong());
    }

    @Test
    public void signOutFromNativeWithManagedDomain() {
        // Stub out various native calls. Some of these are verified as never called
        // and those stubs simply allow that verification to catch any issues.
        doNothing().when(mNativeMock).signOut(any(), anyLong(), anyInt());
        doNothing().when(mNativeMock).wipeProfileData(any(), anyLong());
        doNothing().when(mNativeMock).wipeGoogleServiceWorkerCaches(any(), anyLong());
        // See verification of nativeWipeProfileData below.
        doReturn("TestDomain").when(mNativeMock).getManagementDomain(any(), anyLong());

        // Trigger the sign out flow!
        mSigninManager.onNativeSignOut();

        // nativeSignOut should only be called when signOut() is triggered on
        // the Java side of the JNI boundary. This test instead initiates sign-out
        // from the native side.
        verify(mNativeMock, never()).signOut(any(), anyLong(), anyInt());

        // Sign-out should wipe profile data when user has managed domain.
        verify(mNativeMock, times(1)).wipeProfileData(any(), anyLong());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(any(), anyLong());
    }

    @Test
    public void signOutFromNativeWithNullDomain() {
        // Stub out various native calls. Some of these are verified as never called
        // and those stubs simply allow that verification to catch any issues.
        doNothing().when(mNativeMock).signOut(any(), anyLong(), anyInt());
        doNothing().when(mNativeMock).wipeProfileData(any(), anyLong());
        doNothing().when(mNativeMock).wipeGoogleServiceWorkerCaches(any(), anyLong());
        // See verification of nativeWipeGoogleServiceWorkerCaches below.
        doReturn(null).when(mNativeMock).getManagementDomain(any(), anyLong());

        // Trigger the sign out flow!
        mSigninManager.onNativeSignOut();

        // nativeSignOut should only be called when signOut() is triggered on
        // the Java side of the JNI boundary. This test instead initiates sign-out
        // from the native side.
        verify(mNativeMock, never()).signOut(any(), anyLong(), anyInt());

        // Sign-out should only clear the service worker cache when the domain is null.
        verify(mNativeMock, never()).wipeProfileData(any(), anyLong());
        verify(mNativeMock, times(1)).wipeGoogleServiceWorkerCaches(any(), anyLong());
    }

    @Test
    public void callbackNotifiedWhenNoOperationIsInProgress() {
        assertFalse(mSigninManager.isOperationInProgress());

        AtomicInteger callCount = new AtomicInteger(0);
        mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
        assertEquals(1, callCount.get());
    }

    @Test
    public void callbackNotifiedOnSignout() {
        doAnswer(invocation -> {
            mSigninManager.onNativeSignOut();
            return null;
        })
                .when(mNativeMock)
                .signOut(any(), anyLong(), anyInt());
        doReturn(null).when(mNativeMock).getManagementDomain(any(), anyLong());
        doNothing().when(mNativeMock).wipeGoogleServiceWorkerCaches(any(), anyLong());

        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST);
        assertTrue(mSigninManager.isOperationInProgress());
        AtomicInteger callCount = new AtomicInteger(0);
        mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
        assertEquals(0, callCount.get());

        mSigninManager.onProfileDataWiped();
        assertFalse(mSigninManager.isOperationInProgress());
        assertEquals(1, callCount.get());
    }

    @Test
    public void callbackNotifiedOnSignin() {
        // No need to seed accounts to the native code.
        doReturn(true).when(mAccountTrackerService).checkAndSeedSystemAccounts();
        // Request that policy is loaded. It will pause sign-in until onPolicyCheckedBeforeSignIn is
        // invoked.
        doReturn(true).when(mNativeMock).shouldLoadPolicyForUser(any());
        doNothing().when(mNativeMock).checkPolicyBeforeSignIn(any(), anyLong(), any());

        doReturn(true).when(mSigninManager).isSigninSupported();
        doNothing().when(mNativeMock).onSignInCompleted(any(), anyLong(), any());
        doNothing().when(mSigninManager).logInSignedInUser();

        mSigninManager.onFirstRunCheckDone(); // Allow sign-in.

        Account account = new Account("test@gmail.com", AccountManagerFacade.GOOGLE_ACCOUNT_TYPE);
        mSigninManager.signIn(account, null, null);
        assertTrue(mSigninManager.isOperationInProgress());
        AtomicInteger callCount = new AtomicInteger(0);
        mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
        assertEquals(0, callCount.get());

        mSigninManager.onPolicyCheckedBeforeSignIn(null); // Test user is unmanaged.
        assertFalse(mSigninManager.isOperationInProgress());
        assertEquals(1, callCount.get());
    }
}

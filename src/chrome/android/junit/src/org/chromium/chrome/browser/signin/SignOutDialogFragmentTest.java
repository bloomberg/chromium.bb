// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowAlertDialog;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.ProfileAccountManagementMetrics;
import org.chromium.components.signin.GAIAServiceType;

/** Tests for {@link SignOutDialogFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SignOutDialogFragmentTest {
    private static final String TEST_DOMAIN = "test.domain.example.com";

    private class DummySignOutTargetFragment
            extends Fragment implements SignOutDialogFragment.SignOutDialogListener {
        @Override
        public void onSignOutClicked(boolean forceWipeUserData) {}
    }


    @Rule
    public final JniMocker mocker = new JniMocker();

    @Mock
    private SigninUtils.Natives mSigninUtilsNativeMock;

    @Mock
    private SigninManager mSigninManagerMock;

    @Spy
    private final DummySignOutTargetFragment mTargetFragment = new DummySignOutTargetFragment();

    private SignOutDialogFragment mSignOutDialog;

    private FragmentManager mFragmentManager;

    @Before
    public void setUp() {
        initMocks(this);
        mocker.mock(SigninUtilsJni.TEST_HOOKS, mSigninUtilsNativeMock);
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getSigninManager()).thenReturn(mSigninManagerMock);
        setUpSignOutDialog();
    }

    private void setUpSignOutDialog() {
        mFragmentManager =
                Robolectric.setupActivity(FragmentActivity.class).getSupportFragmentManager();
        mFragmentManager.beginTransaction().add(mTargetFragment, null).commit();
        mSignOutDialog = SignOutDialogFragment.create(GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        mSignOutDialog.setTargetFragment(mTargetFragment, 0);
    }

    @Test
    public void testMessageWhenAccountIsManaged() {
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        AlertDialog alertDialog = showSignOutAlertDialog();
        TextView messageTextView = alertDialog.findViewById(android.R.id.message);
        assertEquals(
                mSignOutDialog.getString(R.string.signout_managed_account_message, TEST_DOMAIN),
                messageTextView.getText());
    }

    @Test
    public void testPositiveButtonWhenAccountIsManaged() {
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        AlertDialog alertDialog = showSignOutAlertDialog();
        alertDialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
        verify(mSigninUtilsNativeMock)
                .logEvent(ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        verify(mTargetFragment).onSignOutClicked(false);
    }

    @Test
    public void testPositiveButtonWhenAccountIsNotManagedAndRemoveLocalDataNotChecked() {
        AlertDialog alertDialog = showSignOutAlertDialog();
        alertDialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
        verify(mSigninUtilsNativeMock)
                .logEvent(ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        verify(mTargetFragment).onSignOutClicked(false);
    }

    @Test
    public void testPositiveButtonWhenAccountIsNotManagedAndRemoveLocalDataChecked() {
        AlertDialog alertDialog = showSignOutAlertDialog();
        alertDialog.findViewById(R.id.remove_local_data).performClick();
        alertDialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
        verify(mSigninUtilsNativeMock)
                .logEvent(ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        verify(mTargetFragment).onSignOutClicked(true);
    }

    @Test
    public void testNegativeButtonWhenAccountIsManaged() {
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        AlertDialog alertDialog = showSignOutAlertDialog();
        alertDialog.getButton(AlertDialog.BUTTON_NEGATIVE).performClick();
        verify(mTargetFragment, never()).onSignOutClicked(anyBoolean());
        verify(mSigninUtilsNativeMock)
                .logEvent(ProfileAccountManagementMetrics.SIGNOUT_CANCEL,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
    }

    @Test
    public void testNegativeButtonWhenAccountIsNotManaged() {
        AlertDialog alertDialog = showSignOutAlertDialog();
        alertDialog.getButton(AlertDialog.BUTTON_NEGATIVE).performClick();
        verify(mTargetFragment, never()).onSignOutClicked(anyBoolean());
        verify(mSigninUtilsNativeMock)
                .logEvent(ProfileAccountManagementMetrics.SIGNOUT_CANCEL,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
    }

    @Test
    public void testEventLoggedWhenDialogDismissed() {
        AlertDialog alertDialog = showSignOutAlertDialog();
        alertDialog.dismiss();
        verify(mTargetFragment, never()).onSignOutClicked(anyBoolean());
        verify(mSigninUtilsNativeMock)
                .logEvent(ProfileAccountManagementMetrics.SIGNOUT_CANCEL,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
    }

    private AlertDialog showSignOutAlertDialog() {
        mSignOutDialog.show(mFragmentManager, null);
        return (AlertDialog) ShadowAlertDialog.getLatestDialog();
    }
}

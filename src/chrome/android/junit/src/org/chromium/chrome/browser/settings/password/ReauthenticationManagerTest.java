// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.password;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Intent;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentTransaction;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;

/**
 * Tests for the "Save Passwords" settings screen.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ReauthenticationManagerTest {
    private FragmentManager mFragmentManager;

    private FragmentActivity mTestActivity;

    @Before
    public void setUp() {
        mTestActivity = Robolectric.setupActivity(FragmentActivity.class);
        PasswordReauthenticationFragment.preventLockingForTesting();

        mFragmentManager = mTestActivity.getSupportFragmentManager();

        // Prepare a dummy Fragment and commit a FragmentTransaction with it.
        FragmentTransaction fragmentTransaction = mFragmentManager.beginTransaction();
        // Replacement fragment for PasswordEntryViewer, which is the fragment that
        // replaces PasswordReauthentication after popBackStack is called.
        Fragment mockPasswordEntryViewer = new Fragment();
        fragmentTransaction.add(mockPasswordEntryViewer, "password_entry_viewer");
        fragmentTransaction.addToBackStack(null);
        fragmentTransaction.commit();
    }

    /**
     * Prepares a dummy Intent to pass to PasswordReauthenticationFragment as a fake result of the
     * reauthentication screen.
     * @return The dummy Intent.
     */
    private Intent prepareDummyDataForActivityResult() {
        Intent data = new Intent();
        data.putExtra("result", "This is the result");
        return data;
    }

    /**
     * Ensure that displayReauthenticationFragment puts the reauthentication fragment on the
     * transaction stack and updates the validity of the reauth when reauth passed.
     */
    @Test
    public void testDisplayReauthenticationFragment_Passed() {
        ReauthenticationManager.resetLastReauth();
        assertFalse(ReauthenticationManager.authenticationStillValid(
                ReauthenticationManager.ReauthScope.ONE_AT_A_TIME));
        assertFalse(ReauthenticationManager.authenticationStillValid(
                ReauthenticationManager.ReauthScope.BULK));

        ReauthenticationManager.displayReauthenticationFragment(
                R.string.lockscreen_description_view, View.NO_ID, mFragmentManager,
                ReauthenticationManager.ReauthScope.ONE_AT_A_TIME);
        Fragment reauthFragment =
                mFragmentManager.findFragmentByTag(ReauthenticationManager.FRAGMENT_TAG);
        assertNotNull(reauthFragment);

        reauthFragment.onActivityResult(
                PasswordReauthenticationFragment.CONFIRM_DEVICE_CREDENTIAL_REQUEST_CODE,
                Activity.RESULT_OK, prepareDummyDataForActivityResult());
        mFragmentManager.executePendingTransactions();

        assertTrue(ReauthenticationManager.authenticationStillValid(
                ReauthenticationManager.ReauthScope.ONE_AT_A_TIME));
    }

    /**
     * Ensure that displayReauthenticationFragment puts the reauthentication fragment on the
     * transaction stack and updates the validity of the reauth when reauth failed.
     */
    @Test
    public void testDisplayReauthenticationFragment_Failed() {
        ReauthenticationManager.resetLastReauth();
        assertFalse(ReauthenticationManager.authenticationStillValid(
                ReauthenticationManager.ReauthScope.ONE_AT_A_TIME));
        assertFalse(ReauthenticationManager.authenticationStillValid(
                ReauthenticationManager.ReauthScope.BULK));

        ReauthenticationManager.displayReauthenticationFragment(
                R.string.lockscreen_description_view, View.NO_ID, mFragmentManager,
                ReauthenticationManager.ReauthScope.ONE_AT_A_TIME);
        Fragment reauthFragment =
                mFragmentManager.findFragmentByTag(ReauthenticationManager.FRAGMENT_TAG);
        assertNotNull(reauthFragment);

        reauthFragment.onActivityResult(
                PasswordReauthenticationFragment.CONFIRM_DEVICE_CREDENTIAL_REQUEST_CODE,
                Activity.RESULT_CANCELED, prepareDummyDataForActivityResult());
        mFragmentManager.executePendingTransactions();

        assertFalse(ReauthenticationManager.authenticationStillValid(
                ReauthenticationManager.ReauthScope.ONE_AT_A_TIME));
        assertFalse(ReauthenticationManager.authenticationStillValid(
                ReauthenticationManager.ReauthScope.BULK));
    }

    /**
     * Ensure that displayReauthenticationFragment considers BULK scope to cover the ONE_AT_A_TIME
     * scope as well.
     */
    @Test
    public void testDisplayReauthenticationFragment_OneAtATimeCovered() {
        ReauthenticationManager.resetLastReauth();
        assertFalse(ReauthenticationManager.authenticationStillValid(
                ReauthenticationManager.ReauthScope.ONE_AT_A_TIME));
        assertFalse(ReauthenticationManager.authenticationStillValid(
                ReauthenticationManager.ReauthScope.BULK));

        ReauthenticationManager.displayReauthenticationFragment(
                R.string.lockscreen_description_export, View.NO_ID, mFragmentManager,
                ReauthenticationManager.ReauthScope.BULK);
        Fragment reauthFragment =
                mFragmentManager.findFragmentByTag(ReauthenticationManager.FRAGMENT_TAG);
        assertNotNull(reauthFragment);

        reauthFragment.onActivityResult(
                PasswordReauthenticationFragment.CONFIRM_DEVICE_CREDENTIAL_REQUEST_CODE,
                Activity.RESULT_OK, prepareDummyDataForActivityResult());
        mFragmentManager.executePendingTransactions();

        // Both BULK and ONE_AT_A_TIME scopes should be covered by the BULK request above.
        assertTrue(ReauthenticationManager.authenticationStillValid(
                ReauthenticationManager.ReauthScope.ONE_AT_A_TIME));
        assertTrue(ReauthenticationManager.authenticationStillValid(
                ReauthenticationManager.ReauthScope.BULK));
    }

    /**
     * Ensure that displayReauthenticationFragment does not consider ONE_AT_A_TIME scope to cover
     * the BULK scope.
     */
    @Test
    public void testDisplayReauthenticationFragment_BulkNotCovered() {
        ReauthenticationManager.resetLastReauth();
        assertFalse(ReauthenticationManager.authenticationStillValid(
                ReauthenticationManager.ReauthScope.ONE_AT_A_TIME));
        assertFalse(ReauthenticationManager.authenticationStillValid(
                ReauthenticationManager.ReauthScope.BULK));

        ReauthenticationManager.displayReauthenticationFragment(
                R.string.lockscreen_description_view, View.NO_ID, mFragmentManager,
                ReauthenticationManager.ReauthScope.ONE_AT_A_TIME);
        Fragment reauthFragment =
                mFragmentManager.findFragmentByTag(ReauthenticationManager.FRAGMENT_TAG);
        assertNotNull(reauthFragment);

        reauthFragment.onActivityResult(
                PasswordReauthenticationFragment.CONFIRM_DEVICE_CREDENTIAL_REQUEST_CODE,
                Activity.RESULT_OK, prepareDummyDataForActivityResult());
        mFragmentManager.executePendingTransactions();

        // Only ONE_AT_A_TIME scope should be covered by the ONE_AT_A_TIME request above.
        assertTrue(ReauthenticationManager.authenticationStillValid(
                ReauthenticationManager.ReauthScope.ONE_AT_A_TIME));
        assertFalse(ReauthenticationManager.authenticationStillValid(
                ReauthenticationManager.ReauthScope.BULK));
    }
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.notifier.signin;

import android.accounts.Account;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.sync.notifier.InvalidationController;
import org.chromium.sync.notifier.SyncStatusHelper;
import org.chromium.sync.signin.ChromeSigninController;
import org.chromium.sync.test.util.MockSyncContentResolverDelegate;

public class SyncStatusHelperTest extends InstrumentationTestCase {

    private static class CountingMockSyncContentResolverDelegate
            extends MockSyncContentResolverDelegate {
        private int mGetMasterSyncAutomaticallyCalls;
        private int mGetSyncAutomaticallyCalls;
        private int mGetIsSyncableCalls;

        @Override
        public boolean getMasterSyncAutomatically() {
            mGetMasterSyncAutomaticallyCalls++;
            return super.getMasterSyncAutomatically();
        }

        @Override
        public boolean getSyncAutomatically(Account account, String authority) {
            mGetSyncAutomaticallyCalls++;
            return super.getSyncAutomatically(account, authority);
        }

        @Override
        public int getIsSyncable(Account account, String authority) {
            mGetIsSyncableCalls++;
            return super.getIsSyncable(account, authority);
        }
    }

    private SyncStatusHelper mHelper;

    private CountingMockSyncContentResolverDelegate mSyncContentResolverDelegate;

    private String mAuthority;

    private Account mTestAccount;

    private Account mAlternateTestAccount;

    @Override
    protected void setUp() throws Exception {
        mSyncContentResolverDelegate = new CountingMockSyncContentResolverDelegate();
        SyncStatusHelper.overrideSyncStatusHelperForTests(
                getInstrumentation().getTargetContext(), mSyncContentResolverDelegate);
        mHelper = SyncStatusHelper.get(getInstrumentation().getTargetContext());
        // Need to set the signed in account name to ensure that sync settings notifications
        // update the right account.
        ChromeSigninController.get(
                getInstrumentation().getTargetContext()).setSignedInAccountName(
                        "account@example.com");
        mAuthority = InvalidationController.get(
                getInstrumentation().getTargetContext()).getContractAuthority();
        mTestAccount = new Account("account@example.com", "com.google");
        mAlternateTestAccount = new Account("alternateAccount@example.com", "com.google");
        super.setUp();
    }

    @SmallTest
    @Feature({"Sync"})
    public void testToggleMasterSyncAutomaticallyFromSettings() throws InterruptedException {
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue("master sync should be set", mHelper.isMasterSyncAutomaticallyEnabled());

        mSyncContentResolverDelegate.setMasterSyncAutomatically(false);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertFalse("master sync should be unset", mHelper.isMasterSyncAutomaticallyEnabled());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testToggleAccountSyncFromSettings() throws InterruptedException {
        // Turn on syncability.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        // First sync
        mSyncContentResolverDelegate.setIsSyncable(mTestAccount, mAuthority, 1);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        mSyncContentResolverDelegate.setSyncAutomatically(mTestAccount, mAuthority, true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue("sync should be set", mHelper.isSyncEnabled(mTestAccount));
        assertTrue("sync should be set for chrome app",
                mHelper.isSyncEnabledForChrome(mTestAccount));

        // Disable sync automatically for the app
        mSyncContentResolverDelegate.setSyncAutomatically(mTestAccount, mAuthority, false);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertFalse("sync should be unset", mHelper.isSyncEnabled(mTestAccount));
        assertFalse("sync should be unset for chrome app",
                mHelper.isSyncEnabledForChrome(mTestAccount));

        // Re-enable sync
        mSyncContentResolverDelegate.setSyncAutomatically(mTestAccount, mAuthority, true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue("sync should be re-enabled", mHelper.isSyncEnabled(mTestAccount));
        assertTrue("sync should be unset for chrome app",
                mHelper.isSyncEnabledForChrome(mTestAccount));

        // Disabled from master sync
        mSyncContentResolverDelegate.setMasterSyncAutomatically(false);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertFalse("sync should be disabled due to master sync",
                mHelper.isSyncEnabled(mTestAccount));
        assertTrue("sync should be set for chrome app",
                mHelper.isSyncEnabledForChrome(mTestAccount));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testToggleAccountSyncFromApplication() throws InterruptedException {
        // Turn on syncability.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mHelper.enableAndroidSync(mTestAccount);
            }
        });
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue("account should be synced", mHelper.isSyncEnabled(mTestAccount));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mHelper.disableAndroidSync(mTestAccount);
            }
        });
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertFalse("account should not be synced", mHelper.isSyncEnabled(mTestAccount));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testToggleSyncabilityForMultipleAccounts() throws InterruptedException {
        // Turn on syncability.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mHelper.enableAndroidSync(mTestAccount);
            }
        });
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue("account should be synced", mHelper.isSyncEnabled(mTestAccount));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mHelper.enableAndroidSync(mAlternateTestAccount);
            }
        });
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue("alternate account should be synced",
                mHelper.isSyncEnabled(mAlternateTestAccount));

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mHelper.disableAndroidSync(mAlternateTestAccount);
            }
        });
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertFalse("alternate account should not be synced",
                mHelper.isSyncEnabled(mAlternateTestAccount));
        assertTrue("account should still be synced", mHelper.isSyncEnabled(mTestAccount));

        // Ensure we don't erroneously re-use cached data.
        assertFalse("null account should not be synced", mHelper.isSyncEnabled(null));
    }

    @SmallTest
    @Feature({"Sync"})
    public void testSyncSettingsCaching() throws InterruptedException {
        // Turn on syncability.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mHelper.enableAndroidSync(mTestAccount);
            }
        });
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue("account should be synced", mHelper.isSyncEnabled(mTestAccount));

        int masterSyncAutomaticallyCalls =
                mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls;
        int isSyncableCalls = mSyncContentResolverDelegate.mGetIsSyncableCalls;
        int getSyncAutomaticallyAcalls = mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls;

        // Do a bunch of reads.
        mHelper.isMasterSyncAutomaticallyEnabled();
        mHelper.isSyncEnabled();
        mHelper.isSyncEnabled(mTestAccount);
        mHelper.isSyncEnabledForChrome(mTestAccount);

        // Ensure values were read from cache.
        assertEquals(masterSyncAutomaticallyCalls,
                mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls);
        assertEquals(isSyncableCalls, mSyncContentResolverDelegate.mGetIsSyncableCalls);
        assertEquals(getSyncAutomaticallyAcalls,
                mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls);

        // Do a bunch of reads for alternate account.
        mHelper.isMasterSyncAutomaticallyEnabled();
        mHelper.isSyncEnabled(mAlternateTestAccount);
        mHelper.isSyncEnabledForChrome(mAlternateTestAccount);

        // Ensure master sync was cached but others are fetched once.
        assertEquals(masterSyncAutomaticallyCalls,
                mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls);
        assertEquals(isSyncableCalls + 1, mSyncContentResolverDelegate.mGetIsSyncableCalls);
        assertEquals(getSyncAutomaticallyAcalls + 1,
                mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls);
    }
}

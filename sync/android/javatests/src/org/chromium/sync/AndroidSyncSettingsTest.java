// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync;

import android.accounts.Account;
import android.content.Context;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.sync.AndroidSyncSettings.AndroidSyncSettingsObserver;
import org.chromium.sync.test.util.MockSyncContentResolverDelegate;

/**
 * Tests for AndroidSyncSettings.
 */
public class AndroidSyncSettingsTest extends InstrumentationTestCase {

    private static class CountingMockSyncContentResolverDelegate
            extends MockSyncContentResolverDelegate {
        private int mGetMasterSyncAutomaticallyCalls;
        private int mGetSyncAutomaticallyCalls;
        private int mGetIsSyncableCalls;
        private int mSetIsSyncableCalls;
        private int mSetSyncAutomaticallyCalls;

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

        @Override
        public void setIsSyncable(Account account, String authority, int syncable) {
            mSetIsSyncableCalls++;
            super.setIsSyncable(account, authority, syncable);
        }

        @Override
        public void setSyncAutomatically(Account account, String authority, boolean sync) {
            mSetSyncAutomaticallyCalls++;
            super.setSyncAutomatically(account, authority, sync);
        }
    }

    private static class MockSyncSettingsObserver implements AndroidSyncSettingsObserver {
        private boolean mReceivedNotification;

        public void clearNotification() {
            mReceivedNotification = false;
        }

        public boolean receivedNotification() {
            return mReceivedNotification;
        }

        @Override
        public void androidSyncSettingsChanged() {
            mReceivedNotification = true;
        }
    }

    private AndroidSyncSettings mAndroid;
    private CountingMockSyncContentResolverDelegate mSyncContentResolverDelegate;
    private String mAuthority;
    private Account mAccount;
    private Account mAlternateAccount;
    private MockSyncSettingsObserver mSyncSettingsObserver;

    @Override
    protected void setUp() throws Exception {
        mSyncContentResolverDelegate = new CountingMockSyncContentResolverDelegate();
        Context context = getInstrumentation().getTargetContext();
        AndroidSyncSettings.overrideForTests(context, mSyncContentResolverDelegate);
        mAndroid = AndroidSyncSettings.get(context);
        mAuthority = mAndroid.getContractAuthority();
        mAccount = new Account("account@example.com", "com.google");
        mAlternateAccount = new Account("alternateAccount@example.com", "com.google");
        mAndroid.updateAccount(mAccount);

        mSyncSettingsObserver = new MockSyncSettingsObserver();
        mAndroid.registerObserver(mSyncSettingsObserver);

        super.setUp();
    }

    private void enableChromeSyncOnUiThread() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAndroid.enableChromeSync();
            }
        });
    }

    private void disableChromeSyncOnUiThread() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                mAndroid.disableChromeSync();
            }
        });
    }

    @SmallTest
    @Feature({"Sync"})
    public void testToggleMasterSyncFromSettings() throws InterruptedException {
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue("master sync should be set", mAndroid.isMasterSyncEnabled());

        mSyncContentResolverDelegate.setMasterSyncAutomatically(false);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertFalse("master sync should be unset", mAndroid.isMasterSyncEnabled());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testToggleChromeSyncFromSettings() throws InterruptedException {
        // Turn on syncability.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        // First sync
        mSyncContentResolverDelegate.setIsSyncable(mAccount, mAuthority, 1);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue("sync should be set", mAndroid.isSyncEnabled());
        assertTrue("sync should be set for chrome app", mAndroid.isChromeSyncEnabled());

        // Disable sync automatically for the app
        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, false);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertFalse("sync should be unset", mAndroid.isSyncEnabled());
        assertFalse("sync should be unset for chrome app", mAndroid.isChromeSyncEnabled());

        // Re-enable sync
        mSyncContentResolverDelegate.setSyncAutomatically(mAccount, mAuthority, true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue("sync should be re-enabled", mAndroid.isSyncEnabled());
        assertTrue("sync should be set for chrome app", mAndroid.isChromeSyncEnabled());

        // Disabled from master sync
        mSyncContentResolverDelegate.setMasterSyncAutomatically(false);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertFalse("sync should be disabled due to master sync", mAndroid.isSyncEnabled());
        assertFalse("master sync should be disabled", mAndroid.isMasterSyncEnabled());
        assertTrue("sync should be set for chrome app", mAndroid.isChromeSyncEnabled());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testToggleAccountSyncFromApplication() throws InterruptedException {
        // Turn on syncability.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        enableChromeSyncOnUiThread();
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue("account should be synced", mAndroid.isSyncEnabled());

        disableChromeSyncOnUiThread();
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertFalse("account should not be synced", mAndroid.isSyncEnabled());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testToggleSyncabilityForMultipleAccounts() throws InterruptedException {
        // Turn on syncability.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        enableChromeSyncOnUiThread();
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue("account should be synced", mAndroid.isSyncEnabled());

        mAndroid.updateAccount(mAlternateAccount);
        enableChromeSyncOnUiThread();
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue("alternate account should be synced", mAndroid.isSyncEnabled());

        disableChromeSyncOnUiThread();
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertFalse("alternate account should not be synced", mAndroid.isSyncEnabled());
        mAndroid.updateAccount(mAccount);
        assertTrue("account should still be synced", mAndroid.isSyncEnabled());

        // Ensure we don't erroneously re-use cached data.
        mAndroid.updateAccount(null);
        assertFalse("null account should not be synced", mAndroid.isSyncEnabled());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testSyncSettingsCaching() throws InterruptedException {
        // Turn on syncability.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        enableChromeSyncOnUiThread();
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();
        assertTrue("account should be synced", mAndroid.isSyncEnabled());

        int masterSyncAutomaticallyCalls =
                mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls;
        int isSyncableCalls = mSyncContentResolverDelegate.mGetIsSyncableCalls;
        int getSyncAutomaticallyAcalls = mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls;

        // Do a bunch of reads.
        mAndroid.isMasterSyncEnabled();
        mAndroid.isSyncEnabled();
        mAndroid.isChromeSyncEnabled();

        // Ensure values were read from cache.
        assertEquals(masterSyncAutomaticallyCalls,
                mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls);
        assertEquals(isSyncableCalls, mSyncContentResolverDelegate.mGetIsSyncableCalls);
        assertEquals(getSyncAutomaticallyAcalls,
                mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls);

        // Do a bunch of reads for alternate account.
        mAndroid.updateAccount(mAlternateAccount);
        mAndroid.isMasterSyncEnabled();
        mAndroid.isSyncEnabled();
        mAndroid.isChromeSyncEnabled();

        // Ensure settings were only fetched once.
        assertEquals(masterSyncAutomaticallyCalls + 1,
                mSyncContentResolverDelegate.mGetMasterSyncAutomaticallyCalls);
        assertEquals(isSyncableCalls + 1, mSyncContentResolverDelegate.mGetIsSyncableCalls);
        assertEquals(getSyncAutomaticallyAcalls + 1,
                mSyncContentResolverDelegate.mGetSyncAutomaticallyCalls);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testGetContractAuthority() throws Exception {
        assertEquals("The contract authority should be the package name.",
                getInstrumentation().getTargetContext().getPackageName(),
                mAndroid.getContractAuthority());
    }

    @SmallTest
    @Feature({"Sync"})
    public void testAndroidSyncSettingsPostsNotifications() throws InterruptedException {
        // Turn on syncability.
        mSyncContentResolverDelegate.setMasterSyncAutomatically(true);
        mSyncContentResolverDelegate.waitForLastNotificationCompleted();

        mSyncSettingsObserver.clearNotification();
        mAndroid.enableChromeSync();
        assertTrue("enableChromeSync should trigger observers",
                mSyncSettingsObserver.receivedNotification());

        mSyncSettingsObserver.clearNotification();
        mAndroid.updateAccount(mAlternateAccount);
        assertTrue("switching to account with different settings should notify",
                mSyncSettingsObserver.receivedNotification());

        mSyncSettingsObserver.clearNotification();
        mAndroid.updateAccount(mAccount);
        assertTrue("switching to account with different settings should notify",
                mSyncSettingsObserver.receivedNotification());

        mSyncSettingsObserver.clearNotification();
        mAndroid.enableChromeSync();
        assertFalse("enableChromeSync shouldn't trigger observers",
                mSyncSettingsObserver.receivedNotification());

        mSyncSettingsObserver.clearNotification();
        mAndroid.disableChromeSync();
        assertTrue("disableChromeSync should trigger observers",
                mSyncSettingsObserver.receivedNotification());

        mSyncSettingsObserver.clearNotification();
        mAndroid.disableChromeSync();
        assertFalse("disableChromeSync shouldn't observers",
                mSyncSettingsObserver.receivedNotification());
    }
}

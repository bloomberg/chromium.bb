// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sync.notifier.signin;

import android.content.ContentResolver;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import org.chromium.base.test.util.Feature;
import org.chromium.sync.notifier.SyncStatusHelper;

public class SyncStatusHelperTest extends InstrumentationTestCase {

    @SmallTest
    @Feature({"Sync"})
    public void testStatusChangeForSettingsShouldCauseCall() {
        TestSyncSettingsChangedObserver observer = new TestSyncSettingsChangedObserver();
        observer.onStatusChanged(ContentResolver.SYNC_OBSERVER_TYPE_SETTINGS);
        assertTrue("Should have called the observer", observer.gotCalled);
    }

    @SmallTest
    @Feature({"Sync"})
    public void testStatusChangeForAnythingElseShouldNoCauseCall() {
        TestSyncSettingsChangedObserver observer = new TestSyncSettingsChangedObserver();
        observer.onStatusChanged(ContentResolver.SYNC_OBSERVER_TYPE_ACTIVE);
        observer.onStatusChanged(ContentResolver.SYNC_OBSERVER_TYPE_PENDING);
        assertFalse("Should not have called the observer", observer.gotCalled);
    }

    private static class TestSyncSettingsChangedObserver
            extends SyncStatusHelper.SyncSettingsChangedObserver {

        public boolean gotCalled;

        @Override
        protected void syncSettingsChanged() {
            gotCalled = true;
        }
    }

}

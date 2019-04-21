// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import java.util.HashSet;
import java.util.Set;

/**
 * Fake some ProfileSyncService methods for testing.
 *
 * Only what has been needed for tests so far has been faked.
 */
public class FakeProfileSyncService extends ProfileSyncService {
    private boolean mEngineInitialized;
    private int mNumberOfSyncedDevices;
    private Set<Integer> mChosenTypes = new HashSet<>();

    public FakeProfileSyncService() {
        super();
    }

    @Override
    public boolean isEngineInitialized() {
        return mEngineInitialized;
    }

    public void setEngineInitialized(boolean engineInitialized) {
        mEngineInitialized = engineInitialized;
    }

    @Override
    public boolean isUsingSecondaryPassphrase() {
        return true;
    }

    @Override
    public int getNumberOfSyncedDevices() {
        return mNumberOfSyncedDevices;
    }

    public void setNumberOfSyncedDevices(int numDevices) {
        mNumberOfSyncedDevices = numDevices;
    }

    @Override
    public void setChosenDataTypes(boolean syncEverything, Set<Integer> enabledTypes) {
        mChosenTypes = enabledTypes;
    }

    @Override
    public Set<Integer> getPreferredDataTypes() {
        return mChosenTypes;
    }
}

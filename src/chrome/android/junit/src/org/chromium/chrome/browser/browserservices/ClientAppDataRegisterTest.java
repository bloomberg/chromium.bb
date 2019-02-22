// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.Set;

/**
 * Tests for {@link ClientAppDataRegister}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ClientAppDataRegisterTest {
    private static final int UID = 23;
    private static final String APP_NAME = "Example App";
    private static final Origin ORIGIN = new Origin("https://www.example.com/");
    private static final Origin OTHER_ORIGIN = new Origin("https://other.example.com/");

    private ClientAppDataRegister mRegister;

    @Before
    public void setUp() {
        mRegister = new ClientAppDataRegister();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void testRegistration() {
        mRegister.registerPackageForOrigin(UID, APP_NAME, ORIGIN);

        Assert.assertTrue(mRegister.chromeHoldsDataForPackage(UID));
        Assert.assertEquals(APP_NAME, mRegister.getAppNameForRegisteredUid(UID));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void testDeregistration() {
        mRegister.registerPackageForOrigin(UID, APP_NAME, ORIGIN);
        mRegister.removePackage(UID);

        Assert.assertFalse(mRegister.chromeHoldsDataForPackage(UID));
        Assert.assertNull(mRegister.getAppNameForRegisteredUid(UID));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void testGetOrigins() {
        mRegister.registerPackageForOrigin(UID, APP_NAME, ORIGIN);
        mRegister.registerPackageForOrigin(UID, APP_NAME, OTHER_ORIGIN);

        Set<String> origins = mRegister.getOriginsForRegisteredUid(UID);
        Assert.assertEquals(2, origins.size());
        Assert.assertTrue(origins.contains(ORIGIN.toString()));
        Assert.assertTrue(origins.contains(OTHER_ORIGIN.toString()));
    }

    @Test
    @Feature("TrustedWebActivities")
    public void testClearOrigins() {
        mRegister.registerPackageForOrigin(UID, APP_NAME, ORIGIN);
        mRegister.registerPackageForOrigin(UID, APP_NAME, OTHER_ORIGIN);
        mRegister.removePackage(UID);

        Set<String> origins = mRegister.getOriginsForRegisteredUid(UID);
        Assert.assertTrue(origins.isEmpty());
    }
}

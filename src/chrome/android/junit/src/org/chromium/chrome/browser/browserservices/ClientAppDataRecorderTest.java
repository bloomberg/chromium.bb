// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/**
 * Tests for {@link ClientAppDataRecorder}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ClientAppDataRecorderTest {
    private static final int APP_UID = 123;
    private static final String APP_NAME = "Example App";
    private static final String APP_PACKAGE = "com.example.app";
    private static final String MISSING_PACKAGE = "com.missing.app";
    private static final Origin ORIGIN = new Origin("https://www.example.com/");
    private static final Origin OTHER_ORIGIN = new Origin("https://www.example.com/");

    @Mock public ClientAppDataRegister mRegister;
    @Mock public PackageManager mPackageManager;

    private ClientAppDataRecorder mRecorder;

    @Before
    public void setUp() throws PackageManager.NameNotFoundException {
        MockitoAnnotations.initMocks(this);

        ApplicationInfo appInfo = new ApplicationInfo();
        appInfo.uid = APP_UID;

        // Even though we're not actually calling getApplicationInfo here, the code needs to deal
        // with a checked exception.
        doReturn(appInfo).when(mPackageManager).getApplicationInfo(eq(APP_PACKAGE), anyInt());
        doReturn(APP_NAME).when(mPackageManager).getApplicationLabel(appInfo);

        doThrow(new PackageManager.NameNotFoundException())
                .when(mPackageManager)
                .getApplicationInfo(eq(MISSING_PACKAGE), anyInt());

        mRecorder = new ClientAppDataRecorder(mPackageManager, mRegister);
    }


    @Test
    @Feature("TrustedWebActivities")
    public void testRegister() {
        mRecorder.register(APP_PACKAGE, ORIGIN);
        verify(mRegister).registerPackageForOrigin(APP_UID, APP_NAME, ORIGIN);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void testDeduplicate() {
        mRecorder.register(APP_PACKAGE, ORIGIN);
        mRecorder.register(APP_PACKAGE, ORIGIN);
        verify(mRegister).registerPackageForOrigin(APP_UID, APP_NAME, ORIGIN);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void testDifferentOrigins() {
        mRecorder.register(APP_PACKAGE, ORIGIN);
        mRecorder.register(APP_PACKAGE, OTHER_ORIGIN);
        verify(mRegister).registerPackageForOrigin(APP_UID, APP_NAME, ORIGIN);
        verify(mRegister).registerPackageForOrigin(APP_UID, APP_NAME, OTHER_ORIGIN);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void testMisingPackage() {
        mRecorder.register(MISSING_PACKAGE, ORIGIN);
        // Implicitly checking we don't throw.
        verify(mRegister, times(0)).registerPackageForOrigin(anyInt(), anyString(), any());
    }
}

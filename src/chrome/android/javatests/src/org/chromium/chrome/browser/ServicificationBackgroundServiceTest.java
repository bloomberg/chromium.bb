// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.support.test.filters.MediumTest;

import com.google.android.gms.gcm.TaskParams;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.init.ServiceManagerStartupUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests background tasks can be run in Service Manager only mode, i.e., without starting the full
 * browser.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@RetryOnFailure
public final class ServicificationBackgroundServiceTest {
    private ServicificationBackgroundService mServicificationBackgroundService;

    @Before
    public void setUp() {
        mServicificationBackgroundService =
                new ServicificationBackgroundService(true /*supportsServiceManagerOnly*/);
        BackgroundSyncLauncher.setGCMEnabled(false);
        RecordHistogram.setDisabledForTests(true);
    }

    @After
    public void tearDown() {
        RecordHistogram.setDisabledForTests(false);
    }

    private static void startServiceAndWaitForNative(
            ServicificationBackgroundService backgroundService) {
        backgroundService.onRunTask(new TaskParams(ServiceManagerStartupUtils.TASK_TAG));
        backgroundService.assertLaunchBrowserCalled();
        backgroundService.waitForNativeLoaded();
    }

    @Test
    @MediumTest
    @Feature({"ServicificationStartup"})
    @CommandLineFlags.Add({"enable-features=NetworkService,AllowStartingServiceManagerOnly"})
    public void testFullBrowserStartsAfterServiceManager() {
        startServiceAndWaitForNative(mServicificationBackgroundService);
        ServicificationBackgroundService.assertOnlyServiceManagerStarted();

        // Now native is loaded in service manager only mode, lets try and load the full browser to
        // test the transition from service manager only to full browser.
        mServicificationBackgroundService.setSupportsServiceManagerOnly(false);
        startServiceAndWaitForNative(mServicificationBackgroundService);
        ServicificationBackgroundService.assertFullBrowserStarted();
    }
}

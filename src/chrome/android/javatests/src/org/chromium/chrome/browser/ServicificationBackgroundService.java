// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

/**
 * Class for launching the service manager only mode for tests.
 */
public class ServicificationBackgroundService extends ChromeBackgroundService {
    private boolean mDidLaunchBrowser;
    private boolean mDidStartServiceManager;

    @Override
    protected void launchBrowser(Context context, String tag) {
        mDidLaunchBrowser = true;

        final BrowserParts parts = new EmptyBrowserParts() {
            @Override
            public void finishNativeInitialization() {
                mDidStartServiceManager = true;
            }

            @Override
            public boolean startServiceManagerOnly() {
                return true;
            }
        };

        try {
            ChromeBrowserInitializer.getInstance().handlePreNativeStartup(parts);
            ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
        } catch (ProcessInitException e) {
            ChromeApplication.reportStartupErrorAndExit(e);
        }
    }

    // Posts an assertion task to the UI thread. Since this is only called after the call
    // to onRunTask, it will be enqueued after any possible call to launchBrowser, and we
    // can reliably check whether launchBrowser was called.
    protected void checkExpectations(final boolean expectedLaunchBrowser) {
        ThreadUtils.runOnUiThread(() -> {
            Assert.assertEquals("StartedService", expectedLaunchBrowser, mDidLaunchBrowser);
        });
    }

    public void waitForServiceManagerStart() {
        CriteriaHelper.pollUiThread(
                new Criteria("Failed while waiting for starting Service Manager.") {
                    @Override
                    public boolean isSatisfied() {
                        return mDidStartServiceManager;
                    }
                });
    }

    public void postTaskAndVerifyFullBrowserNotStarted() {
        // This task will always be queued and executed after
        // the BrowserStartupControllerImpl#browserStartupComplete() is called on the UI thread when
        // the full browser starts. So we can use it to checks whether the
        // {@link mFullBrowserStartupDone} has been set to true.
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                Assert.assertFalse("The full browser is started instead of ServiceManager only.",
                        BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                                .isStartupSuccessfullyCompleted());
            }
        });
    }
}

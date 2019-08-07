// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * A class for native code to request full browser start when running in service manager only mode.
 */
public class NativeStartupBridge {
    private static final String TAG = "NativeStartupBridge";

    @CalledByNative
    private static void loadFullBrowser() {
        if (BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                        .isStartupSuccessfullyCompleted()) {
            return;
        }
        final BrowserParts parts = new EmptyBrowserParts() {
            @Override
            public boolean startServiceManagerOnly() {
                return false;
            }
        };

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                try {
                    ChromeBrowserInitializer.getInstance().handlePreNativeStartup(parts);
                    ChromeBrowserInitializer.getInstance().handlePostNativeStartup(
                            true /* isAsync */, parts);
                } catch (ProcessInitException e) {
                    Log.e(TAG, "ProcessInitException while starting the browser process.");
                }
            }
        });
    }
}

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Coordinator for the WebAPK activity component.
 * Add methods here if other components need to communicate with the WebAPK activity component.
 */
@ActivityScope
public class WebApkActivityCoordinator implements Destroyable {
    private final WebappActivity mActivity;
    private final Lazy<WebApkUpdateManager> mWebApkUpdateManager;

    @Inject
    public WebApkActivityCoordinator(ChromeActivity<?> activity,
            WebappDeferredStartupWithStorageHandler deferredStartupWithStorageHandler,
            WebappDisclosureSnackbarController disclosureSnackbarController,
            WebApkActivityLifecycleUmaTracker webApkActivityLifecycleUmaTracker,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            Lazy<WebApkUpdateManager> webApkUpdateManager) {
        // We don't need to do anything with |disclosureSnackbarController| and
        // |webApkActivityLifecycleUmaTracker|. We just need to resolve
        // them so that they start working.

        mActivity = (WebappActivity) activity;
        mWebApkUpdateManager = webApkUpdateManager;

        deferredStartupWithStorageHandler.addTask((storage, didCreateStorage) -> {
            if (activity.isActivityFinishingOrDestroyed()) return;

            onDeferredStartupWithStorage(storage, didCreateStorage);
        });
        lifecycleDispatcher.register(this);
    }

    public void onDeferredStartupWithStorage(
            @Nullable WebappDataStorage storage, boolean didCreateStorage) {
        assert storage != null;
        storage.incrementLaunchCount();

        mWebApkUpdateManager.get().updateIfNeeded(storage, mActivity.getIntentDataProvider());
    }

    @Override
    public void destroy() {
        // The common case is to be connected to just one WebAPK's services. For the sake of
        // simplicity disconnect from the services of all WebAPKs.
        ChromeWebApkHost.disconnectFromAllServices(true /* waitForPendingWork */);
    }
}

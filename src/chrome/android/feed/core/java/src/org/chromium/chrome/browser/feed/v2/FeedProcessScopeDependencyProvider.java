// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import android.content.Context;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.base.SplitCompatUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.xsurface.ImageFetchClient;
import org.chromium.chrome.browser.xsurface.ProcessScopeDependencyProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Provides logging and context for all surfaces.
 */
public class FeedProcessScopeDependencyProvider implements ProcessScopeDependencyProvider {
    private static final String FEED_SPLIT_NAME = "feedv2";

    private Context mContext;
    private ImageFetchClient mImageFetchClient;
    private LibraryResolver mLibraryResolver;

    FeedProcessScopeDependencyProvider() {
        mContext = createFeedContext(ContextUtils.getApplicationContext());
        mImageFetchClient = new FeedImageFetchClient();
        if (BundleUtils.isIsolatedSplitInstalled(mContext, FEED_SPLIT_NAME)) {
            mLibraryResolver = (libName) -> {
                return BundleUtils.getNativeLibraryPath(libName);
            };
        }
    }

    @Override
    public Context getContext() {
        return mContext;
    }

    @Deprecated
    @Override
    public String getAccountName() {
        assert ThreadUtils.runningOnUiThread();
        CoreAccountInfo primaryAccount =
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.NOT_REQUIRED);
        return (primaryAccount == null) ? "" : primaryAccount.getEmail();
    }

    @Deprecated
    @Override
    public int[] getExperimentIds() {
        // Note: this is thread-safe.
        return FeedStreamSurfaceJni.get().getExperimentIds();
    }

    @Deprecated
    @Override
    public String getClientInstanceId() {
        assert ThreadUtils.runningOnUiThread();
        return FeedServiceBridge.getClientInstanceId();
    }

    @Override
    public ImageFetchClient getImageFetchClient() {
        return mImageFetchClient;
    }

    @Override
    public void logError(String tag, String format, Object... args) {
        Log.e(tag, format, args);
    }

    @Override
    public void logWarning(String tag, String format, Object... args) {
        Log.w(tag, format, args);
    }

    @Override
    public void postTask(int taskType, Runnable task, long delayMs) {
        TaskTraits traits;
        switch (taskType) {
            case ProcessScopeDependencyProvider.TASK_TYPE_UI_THREAD:
                traits = UiThreadTaskTraits.DEFAULT;
                break;
            case ProcessScopeDependencyProvider.TASK_TYPE_BACKGROUND_MAY_BLOCK:
                traits = TaskTraits.BEST_EFFORT_MAY_BLOCK;
                break;
            default:
                assert false : "Invalid task type";
                return;
        }
        PostTask.postDelayedTask(traits, task, delayMs);
    }

    @Override
    public LibraryResolver getLibraryResolver() {
        return mLibraryResolver;
    }

    public static Context createFeedContext(Context context) {
        return SplitCompatUtils.createContextForInflation(context, FEED_SPLIT_NAME);
    }
}

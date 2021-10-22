// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.GoogleAPIKeys;
import org.chromium.chrome.browser.base.SplitCompatUtils;
import org.chromium.chrome.browser.feed.FeedImageFetchClient;
import org.chromium.chrome.browser.feed.FeedPersistentKeyValueCache;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.xsurface.ImageFetchClient;
import org.chromium.chrome.browser.xsurface.PersistentKeyValueCache;
import org.chromium.chrome.browser.xsurface.ProcessScopeDependencyProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.version_info.VersionConstants;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Provides logging and context for all surfaces.
 */
@JNINamespace("feed::android")
public class FeedProcessScopeDependencyProvider implements ProcessScopeDependencyProvider {
    private static final String FEED_SPLIT_NAME = "feedv2";

    private Context mContext;
    private ImageFetchClient mImageFetchClient;
    private FeedPersistentKeyValueCache mPersistentKeyValueCache;
    private LibraryResolver mLibraryResolver;

    @VisibleForTesting
    static PrivacyPreferencesManager sPrivacyPreferencesManagerForTest;

    public FeedProcessScopeDependencyProvider() {
        mContext = createFeedContext(ContextUtils.getApplicationContext());
        mImageFetchClient = new FeedImageFetchClient();
        mPersistentKeyValueCache = new FeedPersistentKeyValueCache();
        if (BundleUtils.isIsolatedSplitInstalled(mContext, FEED_SPLIT_NAME)) {
            mLibraryResolver = (libName) -> {
                return BundleUtils.getNativeLibraryPath(libName, FEED_SPLIT_NAME);
            };
        }
    }

    @Override
    public Context getContext() {
        return mContext;
    }

    @Override
    public ImageFetchClient getImageFetchClient() {
        return mImageFetchClient;
    }

    @Override
    public PersistentKeyValueCache getPersistentKeyValueCache() {
        return mPersistentKeyValueCache;
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

    @Override
    public boolean isXsurfaceUsageAndCrashReportingEnabled() {
        PrivacyPreferencesManager manager = sPrivacyPreferencesManagerForTest;
        if (manager == null) {
            manager = PrivacyPreferencesManagerImpl.getInstance();
        }
        return ChromeFeatureList.isEnabled(ChromeFeatureList.XSURFACE_METRICS_REPORTING)
                && manager.isMetricsReportingEnabled();
    }

    public static Context createFeedContext(Context context) {
        return SplitCompatUtils.createContextForInflation(context, FEED_SPLIT_NAME);
    }

    @Override
    public long getReliabilityLoggingId() {
        return FeedServiceBridge.getReliabilityLoggingId();
    }

    @Override
    public String getGoogleApiKey() {
        return GoogleAPIKeys.GOOGLE_API_KEY;
    }

    @Override
    public String getChromeVersion() {
        return VersionConstants.PRODUCT_VERSION;
    }

    @Override
    public int getChromeChannel() {
        return VersionConstants.CHANNEL;
    }

    @Override
    public int getImageMemoryCacheSizePercentage() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.FEED_IMAGE_MEMORY_CACHE_SIZE_PERCENTAGE,
                "image_memory_cache_size_percentage",
                /*default=*/100);
    }

    @Override
    public int getBitmapPoolSizePercentage() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.FEED_IMAGE_MEMORY_CACHE_SIZE_PERCENTAGE,
                "bitmap_pool_size_percentage",
                /*default=*/100);
    }

    @Override
    public String getAccountName() {
        // Don't return account name if there's a signed-out session ID.
        if (!getSignedOutSessionId().isEmpty()) {
            return "";
        }
        assert ThreadUtils.runningOnUiThread();
        CoreAccountInfo primaryAccount =
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        return (primaryAccount == null) ? "" : primaryAccount.getEmail();
    }

    @Override
    public String getClientInstanceId() {
        // Don't return client instance id if there's a signed-out session ID.
        if (!getSignedOutSessionId().isEmpty()) {
            return "";
        }
        assert ThreadUtils.runningOnUiThread();
        return FeedServiceBridge.getClientInstanceId();
    }

    @Override
    public int[] getExperimentIds() {
        assert ThreadUtils.runningOnUiThread();
        return FeedProcessScopeDependencyProviderJni.get().getExperimentIds();
    }

    @Override
    public String getSignedOutSessionId() {
        ThreadUtils.runningOnUiThread();
        return FeedProcessScopeDependencyProviderJni.get().getSessionId();
    }

    /**
     * Stores a view FeedAction for eventual upload. 'data' is a serialized FeedAction protobuf
     * message.
     */
    @Override
    public void processViewAction(byte[] data) {
        FeedProcessScopeDependencyProviderJni.get().processViewAction(data);
    }

    @Override
    public void reportOnUploadVisibilityLog(boolean success) {
        RecordHistogram.recordBooleanHistogram(
                "ContentSuggestions.Feed.UploadVisibilityLog", success);
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        int[] getExperimentIds();
        String getSessionId();
        void processViewAction(byte[] data);
    }
}

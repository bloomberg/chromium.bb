// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.content.Context;

import androidx.annotation.Nullable;

/**
 * Provides application-level dependencies for an external surface.
 */
public interface ProcessScopeDependencyProvider {
    /**
     * Resolves a library name such as "foo" to an absolute path. The library name should be in the
     * same format given to System.loadLibrary().
     */
    public interface LibraryResolver {
        String resolvePath(String libName);
    }

    /** @return the context associated with the application. */
    @Nullable
    default Context getContext() {
        return null;
    }

    /** Returns the account name of the signed-in user, or the empty string. */
    @Deprecated
    default String getAccountName() {
        return "";
    }

    /** Returns the client instance id for this chrome. */
    @Deprecated
    default String getClientInstanceId() {
        return "";
    }

    /** Returns the collection of currently active experiment ids. */
    @Deprecated
    default int[] getExperimentIds() {
        return new int[0];
    }

    /** @see {Log.e} */
    default void logError(String tag, String messageTemplate, Object... args) {}

    /** @see {Log.w} */
    default void logWarning(String tag, String messageTemplate, Object... args) {}

    /**
     * Returns an ImageFetchClient. ImageFetchClient should only be used for fetching images.
     */
    @Nullable
    default ImageFetchClient getImageFetchClient() {
        return null;
    }

    @Nullable
    default PersistentKeyValueCache getPersistentKeyValueCache() {
        return null;
    }

    // Posts task to the UI thread.
    int TASK_TYPE_UI_THREAD = 1;
    // Posts to a background thread. The task may block.
    int TASK_TYPE_BACKGROUND_MAY_BLOCK = 2;

    /**
     * Runs task on a Chrome executor, see PostTask.java.
     *
     * @param taskType Type of task to run. Determines which thread and what priority is used.
     * @param task The task to run
     * @param delayMs The delay before executing the task in milliseconds.
     */
    default void postTask(int taskType, Runnable task, long delayMs) {}

    /**
     * Returns a LibraryResolver to be used for resolving native library paths. If null is
     * returned, the default library loading mechanism should be used.
     */
    @Nullable
    default LibraryResolver getLibraryResolver() {
        return null;
    }

    default boolean isXsurfaceUsageAndCrashReportingEnabled() {
        return false;
    }

    /** Returns the reliability logging id. */
    default long getReliabilityLoggingId() {
        return 0L;
    }

    /** Returns the google API key. */
    default String getGoogleApiKey() {
        return null;
    }

    /** Returns Chrome's version string. */
    default String getChromeVersion() {
        return "";
    }

    /** Returns Chrome's channel as enumerated in components/version_info/channel.h. */
    default int getChromeChannel() {
        return 0;
    }

    /** Returns the percentage size that the memory cache is allowed to use. */
    default int getImageMemoryCacheSizePercentage() {
        return 100;
    }
}

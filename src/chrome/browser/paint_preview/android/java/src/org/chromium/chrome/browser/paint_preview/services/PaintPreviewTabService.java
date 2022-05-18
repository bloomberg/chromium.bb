// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview.services;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.paintpreview.browser.NativePaintPreviewServiceProvider;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;

import java.io.File;

/**
 * The Java-side implementations of paint_preview_tab_service.cc. The C++ side owns and controls
 * the lifecycle of the Java implementation.
 * This class provides the required functionalities for capturing the Paint Preview representation
 * of a tab.
 */
@JNINamespace("paint_preview")
public class PaintPreviewTabService implements NativePaintPreviewServiceProvider {
    public static final BooleanCachedFieldTrialParameter ALLOW_SRP =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.PAINT_PREVIEW_SHOW_ON_STARTUP, "allow_srp", true);

    private static final long AUDIT_START_DELAY_MS = 2 * 60 * 1000; // Two minutes;
    private static boolean sIsAccessibilityEnabledForTesting;

    private Runnable mAuditRunnable;
    private long mNativePaintPreviewBaseService;
    private long mNativePaintPreviewTabService;

    private class CaptureTriggerListener extends TabModelSelectorTabObserver
            implements ApplicationStatus.ApplicationStateListener {
        private @ApplicationState int mCurrentApplicationState;

        private CaptureTriggerListener(TabModelSelector tabModelSelector) {
            super(tabModelSelector);
            ApplicationStatus.registerApplicationStateListener(this);
        }

        @Override
        public void onApplicationStateChange(int newState) {
            mCurrentApplicationState = newState;
            if (newState == ApplicationState.HAS_DESTROYED_ACTIVITIES) {
                ApplicationStatus.unregisterApplicationStateListener(this);
            }
        }

        @Override
        public void onHidden(Tab tab, int reason) {
            // Only attempt to capture when all activities are stopped.
            // We don't need to worry about race conditions between #onHidden and
            // #onApplicationStateChange when ChromeActivity is stopped.
            // Activity lifecycle callbacks (that run #onApplicationStateChange) are dispatched in
            // Activity#onStop, so they are executed before the call to #onHidden in
            // ChromeActivity#onStop.
            if (mCurrentApplicationState == ApplicationState.HAS_STOPPED_ACTIVITIES
                    && qualifiesForCapture(tab)) {
                captureTab(tab, success -> {
                    if (!success) {
                        // Treat the tab as if it was closed to cleanup any partial capture
                        // data.
                        tabClosed(tab);
                    }
                });
            }
        }

        @Override
        public void onTabUnregistered(Tab tab) {
            tabClosed(tab);
        }

        private boolean qualifiesForCapture(Tab tab) {
            String scheme = tab.getUrl().getScheme();
            boolean schemeAllowed = scheme.equals("http") || scheme.equals("https");
            return !tab.isIncognito() && !tab.isNativePage() && !tab.isShowingErrorPage()
                    && tab.getWebContents() != null && !tab.isLoading() && schemeAllowed
                    && allowIfSrp(tab);
        }

        private boolean allowIfSrp(Tab tab) {
            if (ALLOW_SRP.getValue()) return true;

            return !UrlUtilitiesJni.get().isGoogleSearchUrl(tab.getUrl().getSpec());
        }
    }

    @CalledByNative
    private PaintPreviewTabService(
            long nativePaintPreviewTabService, long nativePaintPreviewBaseService) {
        mNativePaintPreviewTabService = nativePaintPreviewTabService;
        mNativePaintPreviewBaseService = nativePaintPreviewBaseService;
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativePaintPreviewTabService = 0;
        mNativePaintPreviewBaseService = 0;
    }

    @Override
    public long getNativeBaseService() {
        return mNativePaintPreviewBaseService;
    }

    @VisibleForTesting
    public boolean hasNativeServiceForTesting() {
        return mNativePaintPreviewTabService != 0;
    }

    /**
     * Returns whether there exists a capture for the tab ID.
     * @param tabId the id for the tab requested.
     * @return Will be true if there is a capture for the tab.
     */
    public boolean hasCaptureForTab(int tabId) {
        if (mNativePaintPreviewTabService == 0) return false;

        if (!isNativeCacheInitialized()) {
            return previewExistsPreNative(getPath(), tabId);
        }

        return PaintPreviewTabServiceJni.get().hasCaptureForTabAndroid(
                mNativePaintPreviewTabService, tabId);
    }

    /**
     * Should be called when all tabs are restored. Registers a {@link TabModelSelectorTabObserver}
     * for the regular to capture and delete paint previews as needed. Audits restored tabs to
     * remove any failed deletions.
     * @param tabModelSelector the TabModelSelector for the activity.
     * @param runAudit whether to delete tabs not in the tabModelSelector.
     */
    public void onRestoreCompleted(TabModelSelector tabModelSelector, boolean runAudit) {
        new CaptureTriggerListener(tabModelSelector);

        if (!runAudit || mAuditRunnable != null) return;

        // Delay actually performing the audit by a bit to avoid contention with the native task
        // runner that handles IO when showing at startup.
        int id = tabModelSelector.getCurrentTabId();
        int[] ids;
        if (id == Tab.INVALID_TAB_ID || tabModelSelector.isIncognitoSelected()) {
            // Delete all previews.
            ids = new int[0];
        } else {
            // Delete all previews keeping the current tab.
            ids = new int[] {id};
        }
        mAuditRunnable = () -> auditArtifacts(ids);
        PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT,
                () -> {
                    mAuditRunnable.run();
                    mAuditRunnable = null;
                },
                AUDIT_START_DELAY_MS);
    }

    @VisibleForTesting
    public boolean isNativeCacheInitialized() {
        if (mNativePaintPreviewTabService == 0) return false;

        return PaintPreviewTabServiceJni.get().isCacheInitializedAndroid(
                mNativePaintPreviewTabService);
    }

    private String getPath() {
        if (mNativePaintPreviewTabService == 0) return "";

        return PaintPreviewTabServiceJni.get().getPathAndroid(mNativePaintPreviewTabService);
    }

    @VisibleForTesting
    boolean previewExistsPreNative(String rootPath, int tabId) {
        assert rootPath != null;
        assert !rootPath.isEmpty();

        boolean exists = false;
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            File zipPath = new File(
                    rootPath, (new StringBuilder()).append(tabId).append(".zip").toString());
            exists = zipPath.exists();
        }

        return exists;
    }

    public void captureTab(Tab tab, Callback<Boolean> successCallback) {
        if (mNativePaintPreviewTabService == 0) {
            successCallback.onResult(false);
            return;
        }

        boolean isAccessibilityEnabled = sIsAccessibilityEnabledForTesting
                || ChromeAccessibilityUtil.get().isAccessibilityEnabled();
        RenderCoordinates coords = RenderCoordinates.fromWebContents(tab.getWebContents());
        PaintPreviewTabServiceJni.get().captureTabAndroid(mNativePaintPreviewTabService,
                tab.getId(), tab.getWebContents(), isAccessibilityEnabled,
                coords.getPageScaleFactor(), coords.getScrollXPixInt(), coords.getScrollYPixInt(),
                successCallback);
    }

    private void tabClosed(Tab tab) {
        if (mNativePaintPreviewTabService == 0) return;

        PaintPreviewTabServiceJni.get().tabClosedAndroid(
                mNativePaintPreviewTabService, tab.getId());
    }

    @VisibleForTesting
    void auditArtifacts(int[] activeTabIds) {
        if (mNativePaintPreviewTabService == 0) return;

        PaintPreviewTabServiceJni.get().auditArtifactsAndroid(
                mNativePaintPreviewTabService, activeTabIds);
    }

    @VisibleForTesting
    public static void setAccessibilityEnabledForTesting(boolean isAccessibilityEnabled) {
        sIsAccessibilityEnabledForTesting = isAccessibilityEnabled;
    }

    @NativeMethods
    interface Natives {
        void captureTabAndroid(long nativePaintPreviewTabService, int tabId,
                WebContents webContents, boolean accessibilityEnabled, float pageScaleFactor,
                int scrollOffsetX, int scrollOffsetY, Callback<Boolean> successCallback);
        void tabClosedAndroid(long nativePaintPreviewTabService, int tabId);
        boolean hasCaptureForTabAndroid(long nativePaintPreviewTabService, int tabId);
        void auditArtifactsAndroid(long nativePaintPreviewTabService, int[] activeTabIds);
        boolean isCacheInitializedAndroid(long nativePaintPreviewTabService);
        String getPathAndroid(long nativePaintPreviewTabService);
    }
}

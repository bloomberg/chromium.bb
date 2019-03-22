// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.resources.ResourceManager;

/**
 * A SceneLayer to render layers for Ephemeral Tab.
 */
@JNINamespace("android")
public class EphemeralTabSceneLayer extends SceneOverlayLayer {
    /** Pointer to native EphemeralTabSceneLayer. */
    private long mNativePtr;

    /** If the scene layer has been initialized. */
    private boolean mIsInitialized;

    /** The conversion multiple from dp to px. */
    private final float mDpToPx;

    /**
     * @param dpToPx The conversion multiple from dp to px for the device.
     */
    public EphemeralTabSceneLayer(float dpToPx) {
        mDpToPx = dpToPx;
    }

    /**
     * Update the scene layer to draw an OverlayPanel.
     * @param resourceManager Manager to get view and image resources.
     * @param panel The OverlayPanel to render.
     * @param barTextViewId The ID of the view containing the ephemeral tab bar.
     * @param barTextOpacity The opacity of the text specified by {@code barTextViewId}.
     */
    public void update(ResourceManager resourceManager, OverlayPanel panel, int barTextViewId,
            float barTextOpacity) {
        // Don't try to update the layer if not initialized or showing.
        if (resourceManager == null || !panel.isShowing()) return;
        if (!mIsInitialized) {
            nativeCreateEphemeralTabLayer(mNativePtr, resourceManager);

            // TODO(jinsukkim): Find the right icon/background resource for the tab bar.
            nativeSetResourceIds(mNativePtr, barTextViewId,
                    R.drawable.contextual_search_bar_background, R.drawable.modern_toolbar_shadow,
                    R.drawable.infobar_chrome, R.drawable.btn_close);
            mIsInitialized = true;
        }
        boolean isProgressBarVisible = panel.isProgressBarVisible();
        float progressBarHeight = panel.getProgressBarHeight();
        float progressBarOpacity = panel.getProgressBarOpacity();
        int progressBarCompletion = panel.getProgressBarCompletion();

        WebContents panelWebContents = panel.getWebContents();
        nativeUpdate(mNativePtr, R.drawable.progress_bar_background,
                R.drawable.progress_bar_foreground, mDpToPx, panel.getBasePageBrightness(),
                panel.getBasePageY() * mDpToPx, panelWebContents, panel.getOffsetX() * mDpToPx,
                panel.getOffsetY() * mDpToPx, panel.getWidth() * mDpToPx,
                panel.getHeight() * mDpToPx, panel.getBarMarginSide() * mDpToPx,
                panel.getBarHeight() * mDpToPx, barTextOpacity, panel.isBarBorderVisible(),
                panel.getBarBorderHeight() * mDpToPx, panel.getBarShadowVisible(),
                panel.getBarShadowOpacity(), isProgressBarVisible, progressBarHeight * mDpToPx,
                progressBarOpacity, progressBarCompletion);
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        nativeSetContentTree(mNativePtr, contentTree);
    }

    /**
     * Hide the layer tree; for use if the panel is not being shown.
     */
    public void hideTree() {
        if (!mIsInitialized) return;
        nativeHideTree(mNativePtr);
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = nativeInit();
        }
        assert mNativePtr != 0;
    }

    /**
     * Destroys this object and the corresponding native component.
     */
    @Override
    public void destroy() {
        super.destroy();
        mIsInitialized = false;
        mNativePtr = 0;
    }

    private native long nativeInit();
    private native void nativeCreateEphemeralTabLayer(
            long nativeEphemeralTabSceneLayer, ResourceManager resourceManager);
    private native void nativeSetContentTree(
            long nativeEphemeralTabSceneLayer, SceneLayer contentTree);
    private native void nativeHideTree(long nativeEphemeralTabSceneLayer);
    private native void nativeSetResourceIds(long nativeEphemeralTabSceneLayer,
            int barTextResourceId, int barBackgroundResourceId, int barShadowResourceId,
            int panelIconResourceId, int closeIconResourceId);
    private native void nativeUpdate(long nativeEphemeralTabSceneLayer,
            int progressBarBackgroundResourceId, int progressBarResourceId, float dpToPx,
            float basePageBrightness, float basePageYOffset, WebContents webContents, float panelX,
            float panelY, float panelWidth, float panelHeight, float barMarginSide, float barHeight,
            float textOpacity, boolean barBorderVisible, float barBorderHeight,
            boolean barShadowVisible, float barShadowOpacity, boolean isProgressBarVisible,
            float progressBarHeight, float progressBarOpacity, int progressBarCompletion);
}

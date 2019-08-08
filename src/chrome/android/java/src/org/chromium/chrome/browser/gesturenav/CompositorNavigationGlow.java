// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.view.ViewGroup;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.compositor.scene_layer.SceneLayer;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Implements navigation glow using compositor layer for tab switcher or rendered web contents.
 */
@JNINamespace("android")
class CompositorNavigationGlow extends NavigationGlow {
    private float mAccumulatedScroll;
    private long mNativeNavigationGlow;

    /**
     * @pararm parentView Parent view where the glow view gets attached to.
     * @pararm webContents WebContents whose native view's cc layer will be used
     *        for rendering glow effect.
     * @return NavigationGlow object for rendered pages
     */
    public static NavigationGlow forWebContents(ViewGroup parentView, WebContents webContents) {
        CompositorNavigationGlow glow = new CompositorNavigationGlow(parentView);
        glow.initWithWebContents(webContents);
        return glow;
    }

    /**
     * @pararm parentView Parent view where the glow view gets attached to.
     * @pararm sceneLayer SceneLayer whose cc layer will be used for rendering glow effect.
     * @pararm window WindowAndroid object to get WindowAndroidCompositor from for animation effect.
     * @return {@link NavigationGlow} object for the screen rendered with {@link SceneLayer}
     */
    public static NavigationGlow forSceneLayer(
            ViewGroup parentView, SceneLayer sceneLayer, WindowAndroid window) {
        CompositorNavigationGlow glow = new CompositorNavigationGlow(parentView);
        glow.initWithSceneLayer(sceneLayer, window);
        return glow;
    }

    private CompositorNavigationGlow(ViewGroup parentView) {
        super(parentView);
        mNativeNavigationGlow = nativeInit(parentView.getResources().getDisplayMetrics().density);
    }

    private void initWithSceneLayer(SceneLayer sceneLayer, WindowAndroid window) {
        nativeInitWithSceneLayer(mNativeNavigationGlow, sceneLayer, window);
    }

    private void initWithWebContents(WebContents webContents) {
        nativeInitWithWebContents(mNativeNavigationGlow, webContents);
    }

    @Override
    public void prepare(float startX, float startY) {
        nativePrepare(mNativeNavigationGlow, startX, startY, mParentView.getWidth(),
                mParentView.getHeight());
    }

    @Override
    public void onScroll(float xDelta) {
        if (mNativeNavigationGlow == 0) return;
        mAccumulatedScroll += xDelta;
        nativeOnOverscroll(mNativeNavigationGlow, mAccumulatedScroll, xDelta);
    }

    @Override
    public void release() {
        if (mNativeNavigationGlow == 0) return;
        nativeOnOverscroll(mNativeNavigationGlow, 0, 0);
        mAccumulatedScroll = 0;
    }

    @Override
    public void reset() {
        if (mNativeNavigationGlow == 0) return;
        nativeOnReset(mNativeNavigationGlow);
        mAccumulatedScroll = 0;
    }

    @Override
    public void destroy() {
        if (mNativeNavigationGlow == 0) return;
        nativeDestroy(mNativeNavigationGlow);
        mNativeNavigationGlow = 0;
    }

    private native long nativeInit(float dipScale);
    private native void nativeInitWithSceneLayer(
            long nativeNavigationGlow, SceneLayer sceneLayer, WindowAndroid window);
    private native void nativeInitWithWebContents(
            long nativeNavigationGlow, WebContents webContents);
    private native void nativePrepare(
            long nativeNavigationGlow, float startX, float startY, int width, int height);
    private native void nativeOnOverscroll(
            long nativeNavigationGlow, float accumulatedScroll, float delta);
    private native void nativeOnReset(long nativeNavigationGlow);
    private native void nativeDestroy(long nativeNavigationGlow);
}

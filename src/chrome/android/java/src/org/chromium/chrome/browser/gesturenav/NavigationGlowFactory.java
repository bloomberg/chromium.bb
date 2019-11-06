// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.view.ViewGroup;

import org.chromium.base.Supplier;
import org.chromium.chrome.browser.compositor.scene_layer.SceneLayer;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Factory class that provides {@link NavigationGlow} according to the actual surface
 * the glow effect is rendered on.
 */
public class NavigationGlowFactory {
    /**
     * @pararm parentView Parent view where the glow view gets attached to.
     * @pararm sceneLayer SceneLayer whose cc layer will be used for rendering glow effect.
     * @pararm window WindowAndroid object to get WindowAndroidCompositor from for animation effect.
     * @return Supplier for {@link NavigationGlow} object for the screen rendered with {@link
     *         SceneLayer}.
     */
    public static Supplier<NavigationGlow> forSceneLayer(
            ViewGroup parentView, SceneLayer sceneLayer, WindowAndroid window) {
        return () -> CompositorNavigationGlow.forSceneLayer(parentView, sceneLayer, window);
    }

    /**
     * @pararm parentView Parent view where the glow view gets attached to.
     * @pararm webContents WebContents whose native view's cc layer will be used
     *        for rendering glow effect.
     * @return Supplier for {@link NavigationGlow} object for rendered pages.
     */
    public static Supplier<NavigationGlow> forRenderedPage(
            ViewGroup parentView, WebContents webContents) {
        return () -> CompositorNavigationGlow.forWebContents(parentView, webContents);
    }

    /**
     * @pararm parentView Parent view where the glow view gets attached to.
     * @return Supplier for {@link NavigationGlow} object for pages where glow effect can be
     *         rendered on the parent android view.
     */
    public static Supplier<NavigationGlow> forJavaLayer(ViewGroup parentView) {
        return () -> new AndroidUiNavigationGlow(parentView);
    }
}

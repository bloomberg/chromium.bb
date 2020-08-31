// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.RectF;

import org.chromium.chrome.browser.compositor.TitleCache;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.resources.ResourceManager;

/**
 * The {@link LayoutManagerHost} usually is based on a {@link android.view.View}. This
 * implementation is stripped down with static sizes but still support 2 different orientations.
 */
class MockLayoutHost implements LayoutManagerHost, LayoutRenderHost {

    public static final int LAYOUT_HOST_PORTRAIT_WIDTH = 320; // dp
    public static final int LAYOUT_HOST_PORTRAIT_HEIGHT = 460; // dp

    private final Context mContext;
    private boolean mPortrait = true;

    static class MockTitleCache implements TitleCache {
        @Override
        public String getUpdatedTitle(Tab tab, String defaultTitle) {
            return null;
        }

        @Override
        public void remove(int tabId) {}

        @Override
        public void clearExcept(int tabId) {}
    }

    private final MockTitleCache mMockTitleCache = new MockTitleCache();

    MockLayoutHost(Context context) {
        mContext = context;
    }

    public void setOrientation(boolean portrait) {
        mPortrait = portrait;
    }

    @Override
    public void requestRender() {}

    @Override
    public void onCompositorLayout() {}

    @Override
    public void didSwapFrame(int pendingFrameCount) {}

    @Override
    public void onSurfaceCreated() {}

    @Override
    public void onSurfaceResized(int width, int height) {}

    @Override
    public Context getContext() {
        return mContext;
    }

    @Override
    public int getWidth() {
        final float density = mContext.getResources().getDisplayMetrics().density;
        final float size = mPortrait ? LAYOUT_HOST_PORTRAIT_WIDTH : LAYOUT_HOST_PORTRAIT_HEIGHT;
        return (int) (density * size);
    }

    @Override
    public int getHeight() {
        final float density = mContext.getResources().getDisplayMetrics().density;
        final float size = mPortrait ? LAYOUT_HOST_PORTRAIT_HEIGHT : LAYOUT_HOST_PORTRAIT_WIDTH;
        return (int) (density * size);
    }

    @Override
    public void getWindowViewport(RectF outRect) {
        outRect.set(0, 0, getWidth(), getHeight());
    }

    @Override
    public void getVisibleViewport(RectF outRect) {
        outRect.set(0, 0, getWidth(), getHeight());
    }

    @Override
    public void getViewportFullControls(RectF outRect) {
        outRect.set(0, 0, getWidth(), getHeight());
    }

    @Override
    public float getHeightMinusBrowserControls() {
        return getHeight();
    }

    @Override
    public int getTopControlsHeightPixels() {
        return 0;
    }

    @Override
    public int getBottomControlsHeightPixels() {
        return 0;
    }

    @Override
    public LayoutRenderHost getLayoutRenderHost() {
        return this;
    }

    @Override
    public void pushDebugRect(Rect rect, int color) {}

    @Override
    public void loadPersitentTextureDataIfNeeded() {}

    @Override
    public void setContentOverlayVisibility(boolean visible, boolean canBeFocusable) {}

    @Override
    public TitleCache getTitleCache() {
        return mMockTitleCache;
    }

    @Override
    public ChromeFullscreenManager getFullscreenManager() {
        return null;
    }

    @Override
    public ResourceManager getResourceManager() {
        return null;
    }

    @Override
    public void invalidateAccessibilityProvider() {}

    @Override
    public void onContentChanged() {}

    @Override
    public int getBrowserControlsBackgroundColor(Resources res) {
        return 0;
    }

    @Override
    public void hideKeyboard(Runnable postHideTask) {
    }
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.compositor.layouts.ToolbarSwipeLayout;
import org.chromium.chrome.browser.fullscreen.BrowserControlsStateProvider;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.resources.ResourceManager;

/**
 * This class is responsible for reacting to events from the outside world, interacting with other
 * coordinators, running most of the business logic associated with the bottom controls component,
 * and updating the model accordingly.
 */
class BottomControlsMediator implements BrowserControlsStateProvider.Observer,
                                        KeyboardVisibilityDelegate.KeyboardVisibilityListener,
                                        SceneChangeObserver,
                                        OverlayPanelManager.OverlayPanelManagerObserver {
    /** The model for the bottom controls component that holds all of its view state. */
    private final PropertyModel mModel;

    /** The fullscreen manager to observe browser controls events. */
    private final ChromeFullscreenManager mFullscreenManager;

    /**
     * The height of the bottom bar in pixels, not including the top shadow.
     */
    private int mBottomControlsHeight;

    /** A {@link WindowAndroid} for watching keyboard visibility events. */
    private WindowAndroid mWindowAndroid;

    /** The bottom controls visibility. */
    private boolean mIsBottomControlsVisible;

    /** Whether any overlay panel is showing. */
    private boolean mIsOverlayPanelShowing;

    /** Whether the swipe layout is currently active. */
    private boolean mIsInSwipeLayout;

    /** Whether the soft keyboard is visible. */
    private boolean mIsKeyboardVisible;

    /**
     * Build a new mediator that handles events from outside the bottom controls component.
     * @param model The {@link BottomControlsProperties} that holds all the view state for the
     *         bottom controls component.
     * @param fullscreenManager A {@link ChromeFullscreenManager} for events related to the browser
     *                          controls.
     * @param bottomControlsHeight The height of the bottom bar in pixels.
     */
    BottomControlsMediator(PropertyModel model, ChromeFullscreenManager fullscreenManager,
            int bottomControlsHeight) {
        mModel = model;

        mFullscreenManager = fullscreenManager;
        mFullscreenManager.addObserver(this);

        mBottomControlsHeight = bottomControlsHeight;
    }

    void setResourceManager(ResourceManager resourceManager) {
        mModel.set(BottomControlsProperties.RESOURCE_MANAGER, resourceManager);
    }

    void setWindowAndroid(WindowAndroid windowAndroid) {
        assert mWindowAndroid == null : "#setWindowAndroid should only be called once per toolbar.";
        // Watch for keyboard events so we can hide the bottom toolbar when the keyboard is showing.
        mWindowAndroid = windowAndroid;
        mWindowAndroid.getKeyboardDelegate().addKeyboardVisibilityListener(this);
    }

    void setLayoutManager(LayoutManager layoutManager) {
        mModel.set(BottomControlsProperties.LAYOUT_MANAGER, layoutManager);
        layoutManager.addSceneChangeObserver(this);
        layoutManager.getOverlayPanelManager().addObserver(this);
    }

    void setBottomControlsVisible(boolean visible) {
        mIsBottomControlsVisible = visible;
        updateCompositedViewVisibility();
        updateAndroidViewVisibility();
    }

    /**
     * Clean up anything that needs to be when the bottom controls component is destroyed.
     */
    void destroy() {
        mFullscreenManager.removeObserver(this);
        if (mWindowAndroid != null) {
            mWindowAndroid.getKeyboardDelegate().removeKeyboardVisibilityListener(this);
            mWindowAndroid = null;
        }
        if (mModel.get(BottomControlsProperties.LAYOUT_MANAGER) != null) {
            LayoutManager manager = mModel.get(BottomControlsProperties.LAYOUT_MANAGER);
            manager.getOverlayPanelManager().removeObserver(this);
            manager.removeSceneChangeObserver(this);
        }
    }

    @Override
    public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
            int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
        mModel.set(BottomControlsProperties.Y_OFFSET, bottomOffset);
        updateAndroidViewVisibility();
    }

    @Override
    public void onOverlayPanelShown() {
        mIsOverlayPanelShowing = true;
        updateAndroidViewVisibility();
    }

    @Override
    public void onOverlayPanelHidden() {
        mIsOverlayPanelShowing = false;
        updateAndroidViewVisibility();
    }

    @Override
    public void keyboardVisibilityChanged(boolean isShowing) {
        mIsKeyboardVisible = isShowing;
        updateCompositedViewVisibility();
        updateAndroidViewVisibility();
    }

    @Override
    public void onTabSelectionHinted(int tabId) {}

    @Override
    public void onSceneChange(Layout layout) {
        mIsInSwipeLayout = layout instanceof ToolbarSwipeLayout;
        updateAndroidViewVisibility();
    }

    /**
     * @return Whether the browser is currently in fullscreen mode.
     */
    private boolean isInFullscreenMode() {
        return mFullscreenManager != null && mFullscreenManager.getPersistentFullscreenMode();
    }

    /**
     * The composited view is the composited version of the Android View. It is used to be able to
     * scroll the bottom controls off-screen synchronously. Since the bottom controls live below
     * the webcontents we re-size the webcontents through
     * {@link ChromeFullscreenManager#setBottomControlsHeight(int,int)} whenever the composited view
     * visibility changes.
     */
    private void updateCompositedViewVisibility() {
        final boolean isCompositedViewVisible =
                mIsBottomControlsVisible && !mIsKeyboardVisible && !isInFullscreenMode();
        mModel.set(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE, isCompositedViewVisible);
        mFullscreenManager.setBottomControlsHeight(
                isCompositedViewVisible ? mBottomControlsHeight : 0,
                mFullscreenManager.getBottomControlsMinHeight());
    }

    /**
     * The Android View is the interactive view. The composited view should always be behind the
     * Android view which means we hide the Android view whenever the composited view is hidden.
     * We also hide the Android view as we are scrolling the bottom controls off screen this is
     * done by checking if {@link ChromeFullscreenManager#getBottomControlOffset()} is
     * non-zero.
     */
    private void updateAndroidViewVisibility() {
        mModel.set(BottomControlsProperties.ANDROID_VIEW_VISIBLE,
                mIsBottomControlsVisible && !mIsKeyboardVisible && !mIsOverlayPanelShowing
                        && !mIsInSwipeLayout && mFullscreenManager.getBottomControlOffset() == 0
                        && !isInFullscreenMode());
    }
}
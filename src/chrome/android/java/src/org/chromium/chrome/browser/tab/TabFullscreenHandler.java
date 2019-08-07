// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.ImeEventObserver;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.BrowserControlsState;

/**
 * {@link TabObserver} for basic fullscreen operations for {@link Tab}.
 */
public final class TabFullscreenHandler extends TabWebContentsUserData implements ImeEventObserver {
    private static final Class<TabFullscreenHandler> USER_DATA_KEY = TabFullscreenHandler.class;

    private final Tab mTab;

    /** A runnable to delay the enabling of fullscreen mode if necessary. */
    private Runnable mEnterFullscreenRunnable;

    public static void createForTab(Tab tab) {
        assert get(tab) == null;
        tab.getUserDataHost().setUserData(USER_DATA_KEY, new TabFullscreenHandler(tab));
    }

    /**
     * Push state about whether or not the browser controls can show or hide to the renderer.
     */
    public static void updateEnabledState(Tab tab) {
        if (tab == null) return;
        TabFullscreenHandler.get(tab).updateEnabledState();
    }

    private TabFullscreenHandler(Tab tab) {
        super(tab);
        mTab = tab;
        mTab.addObserver(new EmptyTabObserver() {
            @Override
            public void onSSLStateUpdated(Tab tab) {
                updateEnabledState();
            }

            @Override
            public void onInteractabilityChanged(boolean interactable) {
                if (interactable && mEnterFullscreenRunnable != null)
                    mEnterFullscreenRunnable.run();
            }

            @Override
            public void onEnterFullscreenMode(Tab tab, final FullscreenOptions options) {
                if (!tab.isUserInteractable()) {
                    mEnterFullscreenRunnable = new Runnable() {
                        @Override
                        public void run() {
                            enterFullscreenInternal(tab, options);
                            mEnterFullscreenRunnable = null;
                        }
                    };
                    return;
                }

                enterFullscreenInternal(tab, options);
            }

            @Override
            public void onExitFullscreenMode(Tab tab) {
                if (mEnterFullscreenRunnable != null) {
                    mEnterFullscreenRunnable = null;
                    return;
                }

                if (tab.getFullscreenManager() != null) {
                    tab.getFullscreenManager().exitPersistentFullscreenMode();
                }
            }

            /**
             * Do the actual enter of fullscreen mode.
             * @param options Options adjust fullscreen mode.
             */
            private void enterFullscreenInternal(Tab tab, FullscreenOptions options) {
                if (tab.getFullscreenManager() != null) {
                    tab.getFullscreenManager().enterPersistentFullscreenMode(options);
                }

                WebContents webContents = tab.getWebContents();
                if (webContents != null) {
                    SelectionPopupController.fromWebContents(webContents).destroySelectActionMode();
                }
            }

            @Override
            public void onRendererResponsiveStateChanged(Tab tab, boolean isResponsive) {
                if (tab.getFullscreenManager() == null) return;
                if (isResponsive) {
                    updateEnabledState();
                } else {
                    TabBrowserControlsState.update(mTab, BrowserControlsState.SHOWN, false);
                }
            }

            @Override
            public void onPageLoadFinished(Tab tab, String url) {
                updateEnabledState();
            }

            @Override
            public void onDestroyed(Tab tab) {
                tab.removeObserver(this);
            }
        });
    }

    static TabFullscreenHandler get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    @Override
    public void initWebContents(WebContents webContents) {
        ImeAdapter.fromWebContents(webContents).addEventObserver(this);
    }

    @Override
    public void cleanupWebContents(WebContents webContents) {}

    // ImeEventObserver

    @Override
    public void onNodeAttributeUpdated(boolean editable, boolean password) {
        if (mTab.getFullscreenManager() == null) return;
        updateEnabledState();
    }

    private void updateEnabledState() {
        if (mTab.isFrozen()) return;

        int current = TabBrowserControlsState.getConstraints(mTab);
        TabBrowserControlsState.update(
                mTab, BrowserControlsState.BOTH, current != BrowserControlsState.HIDDEN);

        WebContents webContents = mTab.getWebContents();
        if (webContents != null) {
            GestureListenerManager gestureManager =
                    GestureListenerManager.fromWebContents(webContents);
            FullscreenManager fullscreenManager = mTab.getFullscreenManager();
            if (gestureManager != null && fullscreenManager != null) {
                gestureManager.updateMultiTouchZoomSupport(
                        !fullscreenManager.getPersistentFullscreenMode());
            }
        }
    }
}

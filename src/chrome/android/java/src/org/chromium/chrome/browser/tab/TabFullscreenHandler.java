// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.common.BrowserControlsState;

/**
 * {@link TabObserver} for basic fullscreen operations for {@link Tab}.
 */
public final class TabFullscreenHandler extends EmptyTabObserver implements UserData {
    private static final Class<TabFullscreenHandler> USER_DATA_KEY = TabFullscreenHandler.class;

    private final Tab mTab;

    /** A runnable to delay the enabling of fullscreen mode if necessary. */
    private Runnable mEnterFullscreenRunnable;

    public static void createForTab(Tab tab) {
        UserDataHost host = tab.getUserDataHost();
        assert host.getUserData(USER_DATA_KEY) == null;
        host.setUserData(USER_DATA_KEY, new TabFullscreenHandler(tab));
    }

    private TabFullscreenHandler(Tab tab) {
        mTab = tab;
        mTab.addObserver(this);
    }

    // UserData

    @Override
    public void destroy() {
        mTab.removeObserver(this);
    }

    @Override
    public void onSSLStateUpdated(Tab tab) {
        tab.updateFullscreenEnabledState();
    }

    @Override
    public void onInteractabilityChanged(boolean interactable) {
        if (interactable && mEnterFullscreenRunnable != null) mEnterFullscreenRunnable.run();
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

        if (tab.getWebContents() != null) {
            SelectionPopupController controller =
                    SelectionPopupController.fromWebContents(tab.getWebContents());
            controller.destroySelectActionMode();
        }

        // We want to remove any cached thumbnail of the Tab.
        tab.clearThumbnailPlaceholder();
    }

    @Override
    public void onRendererResponsiveStateChanged(Tab tab, boolean isResponsive) {
        if (tab.getFullscreenManager() == null) return;
        if (isResponsive) {
            tab.updateFullscreenEnabledState();
        } else {
            tab.updateBrowserControlsState(BrowserControlsState.SHOWN, false);
        }
    }

    @Override
    public void onPageLoadFinished(Tab tab, String url) {
        tab.updateFullscreenEnabledState();
    }
}

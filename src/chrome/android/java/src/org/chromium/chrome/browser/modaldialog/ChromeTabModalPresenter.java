// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import android.content.res.Resources;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewStub;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.fullscreen.BrowserControlsStateProvider;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAttributeKeys;
import org.chromium.chrome.browser.tab.TabAttributes;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.components.browser_ui.modaldialog.TabModalPresenter;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.BrowserControlsState;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;

/**
 * This presenter creates tab modality by blocking interaction with select UI elements while a
 * dialog is visible.
 */
public class ChromeTabModalPresenter
        extends TabModalPresenter implements BrowserControlsStateProvider.Observer {
    /** The activity displaying the dialogs. */
    private final ChromeActivity mChromeActivity;
    private final Supplier<TabObscuringHandler> mTabObscuringHandlerSupplier;
    private final ChromeFullscreenManager mChromeFullscreenManager;
    private final TabModalBrowserControlsVisibilityDelegate mVisibilityDelegate;

    /** The active tab of which the dialog will be shown on top. */
    private Tab mActiveTab;

    /** The parent view that contains the dialog container. */
    private ViewGroup mContainerParent;

    /** Whether the dialog container is brought to the front in its parent. */
    private boolean mContainerIsAtFront;

    /**
     * Whether an enter animation on the dialog container should run when
     * {@link #onBrowserControlsFullyVisible} is called.
     */
    private boolean mRunEnterAnimationOnCallback;

    /**
     * The sibling view of the dialog container drawn next in its parent when it should be behind
     * browser controls. If BottomSheet is opened or UrlBar is focused, the dialog container should
     * be behind the browser controls and the URL suggestions.
     */
    private View mDefaultNextSiblingView;

    private int mBottomControlsHeight;
    private boolean mShouldUpdateContainerLayoutParams;

    /** A token held while the dialog manager is obscuring all tabs. */
    private int mTabObscuringToken;

    /**
     * Constructor for initializing dialog container.
     * @param chromeActivity The activity displaying the dialogs.
     * @param tabObscuringHandler TabObscuringHandler object.
     */
    public ChromeTabModalPresenter(
            ChromeActivity chromeActivity, Supplier<TabObscuringHandler> tabObscuringHandler) {
        super(chromeActivity);
        mChromeActivity = chromeActivity;
        mTabObscuringHandlerSupplier = tabObscuringHandler;
        mChromeFullscreenManager = mChromeActivity.getFullscreenManager();
        mChromeFullscreenManager.addObserver(this);
        mVisibilityDelegate = new TabModalBrowserControlsVisibilityDelegate();
        mTabObscuringToken = TokenHolder.INVALID_TOKEN;
    }

    public void destroy() {
        mChromeFullscreenManager.removeObserver(this);
    }

    /**
     * @return The browser controls visibility delegate associated with tab modal dialogs.
     */
    public BrowserControlsVisibilityDelegate getBrowserControlsVisibilityDelegate() {
        return mVisibilityDelegate;
    }

    @Override
    protected ViewGroup createDialogContainer() {
        ViewStub dialogContainerStub =
                mChromeActivity.findViewById(R.id.tab_modal_dialog_container_stub);
        dialogContainerStub.setLayoutResource(R.layout.modal_dialog_container);

        ViewGroup dialogContainer = (ViewGroup) dialogContainerStub.inflate();
        dialogContainer.setVisibility(View.GONE);

        // Make sure clicks are not consumed by content beneath the container view.
        dialogContainer.setClickable(true);

        mContainerParent = (ViewGroup) dialogContainer.getParent();
        // The default sibling view is the next view of the dialog container stub in main.xml and
        // should not be removed from its parent.
        mDefaultNextSiblingView =
                mChromeActivity.findViewById(R.id.tab_modal_dialog_container_sibling_view);
        assert mDefaultNextSiblingView != null;

        Resources resources = mChromeActivity.getResources();

        MarginLayoutParams params = (MarginLayoutParams) dialogContainer.getLayoutParams();
        params.width = ViewGroup.MarginLayoutParams.MATCH_PARENT;
        params.height = ViewGroup.MarginLayoutParams.MATCH_PARENT;
        params.topMargin = getContainerTopMargin(resources, mChromeActivity.getFullscreenManager());
        params.bottomMargin = getContainerBottomMargin(mChromeActivity.getFullscreenManager());
        dialogContainer.setLayoutParams(params);

        int scrimVerticalMargin =
                resources.getDimensionPixelSize(R.dimen.tab_modal_scrim_vertical_margin);
        View scrimView = dialogContainer.findViewById(R.id.scrim);
        params = (MarginLayoutParams) scrimView.getLayoutParams();
        params.width = MarginLayoutParams.MATCH_PARENT;
        params.height = MarginLayoutParams.MATCH_PARENT;
        params.topMargin = scrimVerticalMargin;
        scrimView.setLayoutParams(params);

        return dialogContainer;
    }

    @Override
    protected void showDialogContainer() {
        if (mShouldUpdateContainerLayoutParams) {
            MarginLayoutParams params = (MarginLayoutParams) getDialogContainer().getLayoutParams();
            params.topMargin =
                    getContainerTopMargin(mChromeActivity.getResources(), mChromeFullscreenManager);
            params.bottomMargin = mBottomControlsHeight;
            getDialogContainer().setLayoutParams(params);
            mShouldUpdateContainerLayoutParams = false;
        }

        // Don't show the dialog container before browser controls are guaranteed fully visible.
        if (mChromeFullscreenManager.areBrowserControlsFullyVisible()) {
            runEnterAnimation();
        } else {
            mRunEnterAnimationOnCallback = true;
        }
        assert mTabObscuringToken == TokenHolder.INVALID_TOKEN;
        mTabObscuringToken = mTabObscuringHandlerSupplier.get().obscureAllTabs();
    }

    @Override
    protected void setBrowserControlsAccess(boolean restricted) {
        if (mChromeActivity.getToolbarManager() == null) return;

        View menuButton = mChromeActivity.getToolbarManager().getMenuButtonView();

        if (restricted) {
            mActiveTab = mChromeActivity.getActivityTab();
            assert mActiveTab
                    != null : "Tab modal dialogs should be shown on top of an active tab.";

            // Hide contextual search panel so that bottom toolbar will not be
            // obscured and back press is not overridden.
            ContextualSearchManager contextualSearchManager =
                    mChromeActivity.getContextualSearchManager();
            if (contextualSearchManager != null) {
                contextualSearchManager.hideContextualSearch(
                        OverlayPanel.StateChangeReason.UNKNOWN);
            }

            // Dismiss the action bar that obscures the dialogs but preserve the text selection.
            WebContents webContents = mActiveTab.getWebContents();
            if (webContents != null) {
                saveOrRestoreTextSelection(webContents, true);
            }

            // Force toolbar to show and disable overflow menu.
            onTabModalDialogStateChanged(true);

            mChromeActivity.getToolbarManager().setUrlBarFocus(
                    false, LocationBar.OmniboxFocusReason.UNFOCUS);

            menuButton.setEnabled(false);
        } else {
            // Show the action bar back if it was dismissed when the dialogs were showing.
            WebContents webContents = mActiveTab.getWebContents();
            if (webContents != null) {
                saveOrRestoreTextSelection(webContents, false);
            }

            onTabModalDialogStateChanged(false);
            menuButton.setEnabled(true);
            mActiveTab = null;
        }
    }

    @Override
    protected void removeDialogView(PropertyModel model) {
        mRunEnterAnimationOnCallback = false;
        mTabObscuringHandlerSupplier.get().unobscureAllTabs(mTabObscuringToken);
        mTabObscuringToken = TokenHolder.INVALID_TOKEN;
        super.removeDialogView(model);
    }

    @Override
    public void onContentOffsetChanged(int offset) {}

    @Override
    public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
            int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
        if (getDialogModel() == null || !mRunEnterAnimationOnCallback
                || !mChromeFullscreenManager.areBrowserControlsFullyVisible()) {
            return;
        }
        mRunEnterAnimationOnCallback = false;
        runEnterAnimation();
    }

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        mBottomControlsHeight = bottomControlsHeight;
        mShouldUpdateContainerLayoutParams = true;
    }

    @Override
    public void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        mShouldUpdateContainerLayoutParams = true;
    }

    @Override
    public void updateContainerHierarchy(boolean toFront) {
        super.updateContainerHierarchy(toFront);

        if (toFront == mContainerIsAtFront) return;
        mContainerIsAtFront = toFront;
        if (toFront) {
            getDialogContainer().bringToFront();
        } else {
            UiUtils.removeViewFromParent(getDialogContainer());
            UiUtils.insertBefore(mContainerParent, getDialogContainer(), mDefaultNextSiblingView);
        }
    }

    /**
     * Calculate the top margin of the dialog container and the dialog scrim so that the scrim
     * doesn't overlap the toolbar.
     * @param resources {@link Resources} to use to get the scrim vertical margin.
     * @param provider {@link BrowserControlsStateProvider} for browser controls heights.
     * @return The container top margin.
     */
    public static int getContainerTopMargin(
            Resources resources, BrowserControlsStateProvider provider) {
        int scrimVerticalMargin =
                resources.getDimensionPixelSize(R.dimen.tab_modal_scrim_vertical_margin);
        return provider.getTopControlsHeight() - scrimVerticalMargin;
    }

    /**
     * Calculate the bottom margin of the dialog container.
     * @param provider {@link BrowserControlsStateProvider} for browser controls heights.
     * @return The container bottom margin.
     */
    public static int getContainerBottomMargin(BrowserControlsStateProvider provider) {
        return provider.getBottomControlsHeight();
    }

    public static boolean isDialogShowing(Tab tab) {
        return TabAttributes.from(tab).get(TabAttributeKeys.MODAL_DIALOG_SHOWING, false);
    }

    private void onTabModalDialogStateChanged(boolean isShowing) {
        TabAttributes.from(mActiveTab).set(TabAttributeKeys.MODAL_DIALOG_SHOWING, isShowing);
        mVisibilityDelegate.updateConstraintsForTab(mActiveTab);

        // Make sure to exit fullscreen mode before showing the tab modal dialog view.
        mChromeFullscreenManager.onExitFullscreen(mActiveTab);

        // Also need to update browser control state after dismissal to refresh the constraints.
        if (isShowing && areRendererInputEventsIgnored()) {
            mChromeFullscreenManager.showAndroidControls(true);
        } else {
            TabBrowserControlsConstraintsHelper.update(mActiveTab, BrowserControlsState.SHOWN,
                    !mChromeFullscreenManager.offsetOverridden());
        }
    }

    private boolean areRendererInputEventsIgnored() {
        return mActiveTab.getWebContents().getMainFrame().areInputEventsIgnored();
    }

    @VisibleForTesting
    ViewGroup getContainerParentForTest() {
        return mContainerParent;
    }

    /**
     * Handles browser controls constraints for the TabModal dialogs.
     */
    static class TabModalBrowserControlsVisibilityDelegate
            extends BrowserControlsVisibilityDelegate {
        public TabModalBrowserControlsVisibilityDelegate() {
            super(BrowserControlsState.BOTH);
        }

        /**
         * Updates the tab modal browser constraints for the given tab.
         */
        public void updateConstraintsForTab(Tab tab) {
            if (tab == null) return;
            set(isDialogShowing(tab) ? BrowserControlsState.SHOWN : BrowserControlsState.BOTH);
        }
    }
}

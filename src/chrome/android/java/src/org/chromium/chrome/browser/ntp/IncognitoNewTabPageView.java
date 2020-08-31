// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;
import android.graphics.Canvas;
import android.util.AttributeSet;
import android.view.View;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.FrameLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.ui.base.ViewUtils;

/**
 * The New Tab Page for use in the incognito profile.
 */
public class IncognitoNewTabPageView extends FrameLayout {
    private IncognitoNewTabPageManager mManager;
    private boolean mFirstShow = true;
    private NewTabPageScrollView mScrollView;
    private IncognitoDescriptionView mDescriptionView;

    private int mSnapshotWidth;
    private int mSnapshotHeight;
    private int mSnapshotScrollY;

    /**
     * Manages the view interaction with the rest of the system.
     */
    interface IncognitoNewTabPageManager {
        /** Loads a page explaining details about incognito mode in the current tab. */
        void loadIncognitoLearnMore();

        /**
         * Initializes the cookie controls manager for interaction with the cookie controls toggle.
         * */
        void initCookieControlsManager();

        /**
         * Tells the caller whether a new snapshot is required or not.
         * */
        boolean shouldCaptureThumbnail();

        /**
         * Cleans up the manager after it is finished being used.
         * */
        void destroy();

        /**
         * Called when the NTP has completely finished loading (all views will be inflated
         * and any dependent resources will have been loaded).
         */
        void onLoadingComplete();
    }

    /** Default constructor needed to inflate via XML. */
    public IncognitoNewTabPageView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mScrollView = (NewTabPageScrollView) findViewById(R.id.ntp_scrollview);
        mScrollView.setBackgroundColor(
                ApiCompatibilityUtils.getColor(getResources(), R.color.ntp_bg_incognito));
        setContentDescription(
                getResources().getText(R.string.accessibility_new_incognito_tab_page));

        // FOCUS_BEFORE_DESCENDANTS is needed to support keyboard shortcuts. Otherwise, pressing
        // any shortcut causes the UrlBar to be focused. See ViewRootImpl.leaveTouchMode().
        mScrollView.setDescendantFocusability(FOCUS_BEFORE_DESCENDANTS);

        mDescriptionView =
                (IncognitoDescriptionView) findViewById(R.id.new_tab_incognito_container);
        mDescriptionView.setLearnMoreOnclickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                mManager.loadIncognitoLearnMore();
            }
        });
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        assert mManager != null;
        if (mFirstShow) {
            mManager.onLoadingComplete();
            mFirstShow = false;
        }
    }

    /**
     * Initialize the incognito New Tab Page.
     * @param manager The manager that handles external dependencies of the view.
     */
    void initialize(IncognitoNewTabPageManager manager) {
        mManager = manager;
        mManager.initCookieControlsManager();
    }

    /** @return The IncognitoNewTabPageManager associated with this IncognitoNewTabPageView. */
    protected IncognitoNewTabPageManager getManager() {
        return mManager;
    }

    /**
     * @see org.chromium.chrome.browser.compositor.layouts.content.
     *         InvalidationAwareThumbnailProvider#shouldCaptureThumbnail()
     */
    boolean shouldCaptureThumbnail() {
        if (getWidth() == 0 || getHeight() == 0) return false;

        return mManager.shouldCaptureThumbnail() || getWidth() != mSnapshotWidth
                || getHeight() != mSnapshotHeight || mScrollView.getScrollY() != mSnapshotScrollY;
    }

    /**
     * @see org.chromium.chrome.browser.compositor.layouts.content.
     *         InvalidationAwareThumbnailProvider#captureThumbnail(Canvas)
     */
    void captureThumbnail(Canvas canvas) {
        ViewUtils.captureBitmap(this, canvas);
        mSnapshotWidth = getWidth();
        mSnapshotHeight = getHeight();
        mSnapshotScrollY = mScrollView.getScrollY();
    }

    /**
     * Set the visibility of the cookie controls card on the incognito description.
     * @param isVisible Whether it's visible or not.
     */
    void setIncognitoCookieControlsCardVisibility(boolean isVisible) {
        mDescriptionView.showCookieControlsCard(isVisible);
    }

    /**
     * Set the toggle on the cookie controls card.
     * @param isChecked Whether it's checked or not.
     */
    void setIncognitoCookieControlsToggleChecked(boolean isChecked) {
        mDescriptionView.setCookieControlsToggle(isChecked);
    }

    /**
     * Set the incognito cookie controls toggle checked change listener.
     * @param listener The given checked change listener.
     */
    void setIncognitoCookieControlsToggleCheckedListener(OnCheckedChangeListener listener) {
        mDescriptionView.setCookieControlsToggleOnCheckedChangeListener(listener);
    }

    /**
     * Set the enforcement rule for the incognito cookie controls toggle.
     * @param enforcement The enforcement enum to set.
     */
    void setIncognitoCookieControlsToggleEnforcement(@CookieControlsEnforcement int enforcement) {
        mDescriptionView.setCookieControlsEnforcement(enforcement);
    }

    /**
     * Set the incognito cookie controls icon click listener.
     * @param listener The given onclick listener.
     */
    void setIncognitoCookieControlsIconOnclickListener(OnClickListener listener) {
        mDescriptionView.setCookieControlsIconOnclickListener(listener);
    }
}

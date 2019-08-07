// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.ephemeraltab;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelTextViewInflater;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Controls the Caption View that is shown at the bottom of the control and used
 * as a dynamic resource.
 */
public class EphemeralTabCaptionControl extends OverlayPanelTextViewInflater {
    /** The caption View. */
    private TextView mCaption;

    /** Whether the caption is showing. */
    private boolean mShowingCaption;

    /** The caption visibility. */
    private boolean mIsVisible;

    /**
     * The caption animation percentage, which controls how and where to draw. It is
     * 0 when the Contextual Search bar is peeking and 1 when it is maxmized.
     */
    private float mAnimationPercentage;

    /**
     * @param panel                     The panel.
     * @param context                   The Android Context used to inflate the View.
     * @param container                 The container View used to inflate the View.
     * @param resourceLoader            The resource loader that will handle the snapshot capturing.
     */
    public EphemeralTabCaptionControl(OverlayPanel panel, Context context, ViewGroup container,
            DynamicResourceLoader resourceLoader) {
        super(panel, R.layout.ephemeral_tab_caption_view, R.id.ephemeral_tab_caption_view, context,
                container, resourceLoader);
    }

    /**
     * Updates the caption when in transition between peeked to maximized states.
     * @param percentage The percentage to the more opened state.
     */
    public void updatePanelForMaximization(float percentage) {
        // If the caption is not showing, show it now.
        if (!mShowingCaption && percentage > 0.f) {
            mShowingCaption = true;

            if (mCaption == null) {
                // |mCaption| gets initialized synchronously in |onFinishInflate|.
                inflate();
                mCaption.setText(R.string.contextmenu_open_in_new_tab);
            }
            invalidate();
            mIsVisible = true;
        }

        mAnimationPercentage = percentage;
        if (mAnimationPercentage == 0.f) mShowingCaption = false;
    }

    /**
     * Hides the caption.
     */
    public void hide() {
        if (mShowingCaption) {
            mIsVisible = false;
            mAnimationPercentage = 0.f;
        }
    }

    /**
     * Controls whether the caption is visible and can be rendered.
     * The caption must be visible in order to draw it and take a snapshot.
     * Even though the caption is visible the user might not be able to see it due to a
     * completely transparent opacity associated with an animation percentage of zero.
     * @return Whether the caption is visible or not.
     */
    public boolean getIsVisible() {
        return mIsVisible;
    }

    /**
     * Gets the animation percentage which controls the drawing of the caption and how high to
     * position it in the Bar.
     * @return The current percentage ranging from 0.0 to 1.0.
     */
    public float getAnimationPercentage() {
        return mAnimationPercentage;
    }

    /**
     * @return The text currently showing in the caption view.
     */
    public CharSequence getCaptionText() {
        return mCaption.getText();
    }

    // OverlayPanelTextViewInflater

    @Override
    protected TextView getTextView() {
        return mCaption;
    }

    // OverlayPanelInflater

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        View view = getView();
        mCaption = (TextView) view.findViewById(R.id.ephemeral_tab_caption);
    }
}

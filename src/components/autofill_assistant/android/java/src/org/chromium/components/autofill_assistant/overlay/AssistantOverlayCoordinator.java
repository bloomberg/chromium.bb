// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.overlay;

import android.content.Context;
import android.graphics.RectF;
import android.view.View;

import org.chromium.components.autofill_assistant.AssistantBrowserControlsFactory;
import org.chromium.components.autofill_assistant.overlay.AssistantOverlayModel.AssistantOverlayRect;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.AccessibilityUtil;

import java.util.List;

/**
 * Coordinator responsible for showing a full or partial overlay on top of the web page currently
 * displayed.
 */
public class AssistantOverlayCoordinator {
    private final AssistantOverlayModel mModel;
    private final AssistantOverlayEventFilter mEventFilter;
    private final AssistantOverlayDrawable mDrawable;
    private final View mRootView;
    private final ScrimCoordinator mScrim;
    private final AccessibilityUtil mAccessibilityUtil;
    private boolean mScrimEnabled;
    private boolean mScrimSuppressed;

    public AssistantOverlayCoordinator(Context context,
            AssistantBrowserControlsFactory browserControlsFactory, View rootView,
            ScrimCoordinator scrim, AssistantOverlayModel model,
            AccessibilityUtil accessibilityUtil) {
        mModel = model;
        mRootView = rootView;
        mScrim = scrim;
        mAccessibilityUtil = accessibilityUtil;
        mEventFilter = new AssistantOverlayEventFilter(context, browserControlsFactory, rootView);
        mDrawable = new AssistantOverlayDrawable(context, browserControlsFactory);

        // Listen for changes in the state.
        // TODO(crbug.com/806868): Bind model to view through a ViewBinder instead.
        model.addObserver((source, propertyKey) -> {
            if (AssistantOverlayModel.STATE == propertyKey) {
                setState(model.get(AssistantOverlayModel.STATE));
            } else if (AssistantOverlayModel.WEB_CONTENTS == propertyKey) {
                WebContents webContents = model.get(AssistantOverlayModel.WEB_CONTENTS);
                mEventFilter.setWebContents(webContents);
                mDrawable.setWebContents(webContents);
            } else if (AssistantOverlayModel.VISUAL_VIEWPORT == propertyKey) {
                RectF rect = model.get(AssistantOverlayModel.VISUAL_VIEWPORT);
                mEventFilter.setVisualViewport(rect);
                mDrawable.setVisualViewport(rect);
            } else if (AssistantOverlayModel.TOUCHABLE_AREA == propertyKey) {
                List<AssistantOverlayRect> area = model.get(AssistantOverlayModel.TOUCHABLE_AREA);
                mEventFilter.setTouchableArea(area);
                mDrawable.setTransparentArea(area);
            } else if (AssistantOverlayModel.RESTRICTED_AREA == propertyKey) {
                List<AssistantOverlayRect> area = model.get(AssistantOverlayModel.RESTRICTED_AREA);
                mEventFilter.setRestrictedArea(area);
                mDrawable.setRestrictedArea(area);
            } else if (AssistantOverlayModel.DELEGATE == propertyKey) {
                mEventFilter.setDelegate(model.get(AssistantOverlayModel.DELEGATE));
            } else if (AssistantOverlayModel.BACKGROUND_COLOR == propertyKey) {
                mDrawable.setBackgroundColor(model.get(AssistantOverlayModel.BACKGROUND_COLOR));
            } else if (AssistantOverlayModel.HIGHLIGHT_BORDER_COLOR == propertyKey) {
                mDrawable.setHighlightBorderColor(
                        model.get(AssistantOverlayModel.HIGHLIGHT_BORDER_COLOR));
            } else if (AssistantOverlayModel.TAP_TRACKING_COUNT == propertyKey) {
                mEventFilter.setTapTrackingCount(
                        model.get(AssistantOverlayModel.TAP_TRACKING_COUNT));
            } else if (AssistantOverlayModel.TAP_TRACKING_DURATION_MS == propertyKey) {
                mEventFilter.setTapTrackingDurationMs(
                        model.get(AssistantOverlayModel.TAP_TRACKING_DURATION_MS));
            } else if (AssistantOverlayModel.OVERLAY_IMAGE == propertyKey) {
                mDrawable.setFullOverlayImage(model.get(AssistantOverlayModel.OVERLAY_IMAGE));
            }
        });
    }

    /** Return the model observed by this coordinator. */
    public AssistantOverlayModel getModel() {
        return mModel;
    }

    /**
     * Destroy this coordinator.
     */
    public void destroy() {
        setScrimEnabled(false);
        mEventFilter.destroy();
        mDrawable.destroy();
    }

    /**
     * Suppress the Scrim.
     */
    public void suppress() {
        mScrimSuppressed = true;
        setScrimEnabled(false);
    }

    /**
     * Restore the Scrim to the current state.
     */
    public void restore() {
        mScrimSuppressed = false;
        setState(mModel.get(AssistantOverlayModel.STATE));
    }

    /**
     * Set the overlay state.
     */
    private void setState(@AssistantOverlayState int state) {
        if (mScrimSuppressed) {
            return;
        }

        if (state == AssistantOverlayState.PARTIAL && mAccessibilityUtil.isAccessibilityEnabled()) {
            // Touch exploration is fully disabled if there's an overlay in front. In this case, the
            // overlay must be fully gone and filtering elements for touch exploration must happen
            // at another level.
            //
            // TODO(crbug.com/806868): filter elements available to touch exploration, when it
            // is enabled.
            state = AssistantOverlayState.HIDDEN;
        }

        if (state == AssistantOverlayState.HIDDEN) {
            setScrimEnabled(false);
            mEventFilter.reset();
        } else {
            setScrimEnabled(true);
            mDrawable.setPartial(state == AssistantOverlayState.PARTIAL);
            mEventFilter.setPartial(state == AssistantOverlayState.PARTIAL);
        }
    }

    private void setScrimEnabled(boolean enabled) {
        if (enabled == mScrimEnabled) return;

        if (enabled) {
            PropertyModel params = new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                                           .with(ScrimProperties.TOP_MARGIN, 0)
                                           .with(ScrimProperties.AFFECTS_STATUS_BAR, false)
                                           .with(ScrimProperties.ANCHOR_VIEW, mRootView)
                                           .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, true)
                                           .with(ScrimProperties.VISIBILITY_CALLBACK, null)
                                           .with(ScrimProperties.CLICK_DELEGATE, null)
                                           .with(ScrimProperties.BACKGROUND_DRAWABLE, mDrawable)
                                           .with(ScrimProperties.GESTURE_DETECTOR, mEventFilter)
                                           .build();
            mScrim.showScrim(params);
        } else {
            mScrim.hideScrim(/* fadeOut= */ true);
        }
        mScrimEnabled = enabled;
    }
}

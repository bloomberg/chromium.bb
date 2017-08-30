// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.annotation.TargetApi;
import android.content.ClipData;
import android.graphics.Bitmap;
import android.os.Build;
import android.view.PointerIcon;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout.LayoutParams;
import android.widget.ImageView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blink_public.web.WebCursorInfoType;

/**
 * Class to acquire, position, and remove anchor views from the implementing View.
 */
@JNINamespace("ui")
public abstract class ViewAndroidDelegate {
    // Temporary storage for use as a parameter of getLocationOnScreen().
    private int[] mTemporaryContainerLocation = new int[2];

    /**
     * @return An anchor view that can be used to anchor decoration views like Autofill popup.
     */
    @CalledByNative
    public View acquireView() {
        ViewGroup containerView = getContainerView();
        if (containerView == null || containerView.getParent() == null) return null;
        View anchorView = new View(containerView.getContext());
        containerView.addView(anchorView);
        return anchorView;
    }

    /**
     * Release given anchor view.
     * @param anchorView The anchor view that needs to be released.
     */
    @CalledByNative
    public void removeView(View anchorView) {
        ViewGroup containerView = getContainerView();
        if (containerView == null) return;
        containerView.removeView(anchorView);
    }

    /**
     * Set the anchor view to specified position and size (all units in dp).
     * @param view The anchor view that needs to be positioned.
     * @param x X coordinate of the top left corner of the anchor view.
     * @param y Y coordinate of the top left corner of the anchor view.
     * @param width The width of the anchor view.
     * @param height The height of the anchor view.
     */
    @CalledByNative
    public void setViewPosition(View view, float x, float y,
            float width, float height, float scale, int leftMargin, int topMargin) {
        ViewGroup containerView = getContainerView();
        if (containerView == null) return;

        int scaledWidth = Math.round(width * scale);
        int scaledHeight = Math.round(height * scale);
        int startMargin;

        if (ApiCompatibilityUtils.isLayoutRtl(containerView)) {
            startMargin = containerView.getMeasuredWidth() - Math.round((width + x) * scale);
        } else {
            startMargin = leftMargin;
        }
        if (scaledWidth + startMargin > containerView.getWidth()) {
            scaledWidth = containerView.getWidth() - startMargin;
        }
        LayoutParams lp = new LayoutParams(scaledWidth, scaledHeight);
        ApiCompatibilityUtils.setMarginStart(lp, startMargin);
        lp.topMargin = topMargin;
        view.setLayoutParams(lp);
    }

    /**
     * Drag the text out of current view.
     * @param text The dragged text.
     * @param shadowImage The shadow image for the dragged text.
     */
    @SuppressWarnings("deprecation")
    @TargetApi(Build.VERSION_CODES.N)
    @CalledByNative
    private boolean startDragAndDrop(String text, Bitmap shadowImage) {
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) return false;

        ViewGroup containerView = getContainerView();
        if (containerView == null) return false;

        ImageView imageView = new ImageView(containerView.getContext());
        imageView.setImageBitmap(shadowImage);
        imageView.layout(0, 0, shadowImage.getWidth(), shadowImage.getHeight());

        return containerView.startDragAndDrop(ClipData.newPlainText(null, text),
                new View.DragShadowBuilder(imageView), null, View.DRAG_FLAG_GLOBAL);
    }

    @VisibleForTesting
    @CalledByNative
    public void onCursorChangedToCustom(Bitmap customCursorBitmap, int hotspotX, int hotspotY) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            PointerIcon icon = PointerIcon.create(customCursorBitmap, hotspotX, hotspotY);
            getContainerView().setPointerIcon(icon);
        }
    }

    @VisibleForTesting
    @CalledByNative
    public void onCursorChanged(int cursorType) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) return;

        int pointerIconType = PointerIcon.TYPE_ARROW;
        switch (cursorType) {
            case WebCursorInfoType.TYPE_NONE:
                pointerIconType = PointerIcon.TYPE_NULL;
                break;
            case WebCursorInfoType.TYPE_POINTER:
                pointerIconType = PointerIcon.TYPE_ARROW;
                break;
            case WebCursorInfoType.TYPE_CONTEXT_MENU:
                pointerIconType = PointerIcon.TYPE_CONTEXT_MENU;
                break;
            case WebCursorInfoType.TYPE_HAND:
                pointerIconType = PointerIcon.TYPE_HAND;
                break;
            case WebCursorInfoType.TYPE_HELP:
                pointerIconType = PointerIcon.TYPE_HELP;
                break;
            case WebCursorInfoType.TYPE_WAIT:
                pointerIconType = PointerIcon.TYPE_WAIT;
                break;
            case WebCursorInfoType.TYPE_CELL:
                pointerIconType = PointerIcon.TYPE_CELL;
                break;
            case WebCursorInfoType.TYPE_CROSS:
                pointerIconType = PointerIcon.TYPE_CROSSHAIR;
                break;
            case WebCursorInfoType.TYPE_I_BEAM:
                pointerIconType = PointerIcon.TYPE_TEXT;
                break;
            case WebCursorInfoType.TYPE_VERTICAL_TEXT:
                pointerIconType = PointerIcon.TYPE_VERTICAL_TEXT;
                break;
            case WebCursorInfoType.TYPE_ALIAS:
                pointerIconType = PointerIcon.TYPE_ALIAS;
                break;
            case WebCursorInfoType.TYPE_COPY:
                pointerIconType = PointerIcon.TYPE_COPY;
                break;
            case WebCursorInfoType.TYPE_NO_DROP:
                pointerIconType = PointerIcon.TYPE_NO_DROP;
                break;
            case WebCursorInfoType.TYPE_COLUMN_RESIZE:
                pointerIconType = PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.TYPE_ROW_RESIZE:
                pointerIconType = PointerIcon.TYPE_VERTICAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.TYPE_NORTH_EAST_SOUTH_WEST_RESIZE:
                pointerIconType = PointerIcon.TYPE_TOP_RIGHT_DIAGONAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.TYPE_NORTH_WEST_SOUTH_EAST_RESIZE:
                pointerIconType = PointerIcon.TYPE_TOP_LEFT_DIAGONAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.TYPE_ZOOM_IN:
                pointerIconType = PointerIcon.TYPE_ZOOM_IN;
                break;
            case WebCursorInfoType.TYPE_ZOOM_OUT:
                pointerIconType = PointerIcon.TYPE_ZOOM_OUT;
                break;
            case WebCursorInfoType.TYPE_GRAB:
                pointerIconType = PointerIcon.TYPE_GRAB;
                break;
            case WebCursorInfoType.TYPE_GRABBING:
                pointerIconType = PointerIcon.TYPE_GRABBING;
                break;
            // TODO(jaebaek): set types correctly
            // after fixing http://crbug.com/584424.
            case WebCursorInfoType.TYPE_EAST_WEST_RESIZE:
                pointerIconType = PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.TYPE_NORTH_SOUTH_RESIZE:
                pointerIconType = PointerIcon.TYPE_VERTICAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.TYPE_EAST_RESIZE:
                pointerIconType = PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.TYPE_NORTH_RESIZE:
                pointerIconType = PointerIcon.TYPE_VERTICAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.TYPE_NORTH_EAST_RESIZE:
                pointerIconType = PointerIcon.TYPE_TOP_RIGHT_DIAGONAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.TYPE_NORTH_WEST_RESIZE:
                pointerIconType = PointerIcon.TYPE_TOP_LEFT_DIAGONAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.TYPE_SOUTH_RESIZE:
                pointerIconType = PointerIcon.TYPE_VERTICAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.TYPE_SOUTH_EAST_RESIZE:
                pointerIconType = PointerIcon.TYPE_TOP_LEFT_DIAGONAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.TYPE_SOUTH_WEST_RESIZE:
                pointerIconType = PointerIcon.TYPE_TOP_RIGHT_DIAGONAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.TYPE_WEST_RESIZE:
                pointerIconType = PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.TYPE_PROGRESS:
                pointerIconType = PointerIcon.TYPE_WAIT;
                break;
            case WebCursorInfoType.TYPE_NOT_ALLOWED:
                pointerIconType = PointerIcon.TYPE_NO_DROP;
                break;
            case WebCursorInfoType.TYPE_MOVE:
            case WebCursorInfoType.TYPE_MIDDLE_PANNING:
                pointerIconType = PointerIcon.TYPE_ALL_SCROLL;
                break;
            case WebCursorInfoType.TYPE_EAST_PANNING:
            case WebCursorInfoType.TYPE_NORTH_PANNING:
            case WebCursorInfoType.TYPE_NORTH_EAST_PANNING:
            case WebCursorInfoType.TYPE_NORTH_WEST_PANNING:
            case WebCursorInfoType.TYPE_SOUTH_PANNING:
            case WebCursorInfoType.TYPE_SOUTH_EAST_PANNING:
            case WebCursorInfoType.TYPE_SOUTH_WEST_PANNING:
            case WebCursorInfoType.TYPE_WEST_PANNING:
                assert false : "These pointer icon types are not supported";
                break;
            case WebCursorInfoType.TYPE_CUSTOM:
                assert false : "onCursorChangedToCustom must be called instead";
                break;
        }
        ViewGroup containerView = getContainerView();
        PointerIcon icon = PointerIcon.getSystemIcon(containerView.getContext(), pointerIconType);
        containerView.setPointerIcon(icon);
    }

    /**
     * Called whenever the background color of the page changes as notified by Blink.
     * @param color The new ARGB color of the page background.
     */
    @CalledByNative
    public void onBackgroundColorChanged(int color) {}

    /**
     * Notify the client of the position of the top controls.
     * @param topControlsOffsetY The Y offset of the top controls in physical pixels.
     * @param topContentOffsetY The Y offset of the content in physical pixels.
     */
    @CalledByNative
    public void onTopControlsChanged(float topControlsOffsetY, float topContentOffsetY) {}

    /**
     * Notify the client of the position of the bottom controls.
     * @param bottomControlsOffsetY The Y offset of the bottom controls in physical pixels.
     * @param bottomContentOffsetY The Y offset of the content in physical pixels.
     */
    @CalledByNative
    public void onBottomControlsChanged(float bottomControlsOffsetY, float bottomContentOffsetY) {}

    /**
     * Returns the bottom system window inset in pixels. The system window inset represents the area
     * of a full-screen window that is partially or fully obscured by the status bar, navigation
     * bar, IME or other system windows.
     * @return The bottom system window inset.
     */
    @CalledByNative
    public int getSystemWindowInsetBottom() {
        return 0;
    }

    /**
     * @return container view that the anchor views are added to. May be null.
     */
    @CalledByNative
    public abstract ViewGroup getContainerView();

    /**
     * Return the X location of our container view.
     */
    @CalledByNative
    private int getXLocationOfContainerViewInWindow() {
        ViewGroup container = getContainerView();
        if (container == null) return 0;

        container.getLocationInWindow(mTemporaryContainerLocation);
        return mTemporaryContainerLocation[0];
    }

    /**
     * Return the Y location of our container view.
     */
    @CalledByNative
    private int getYLocationOfContainerViewInWindow() {
        ViewGroup container = getContainerView();
        if (container == null) return 0;

        container.getLocationInWindow(mTemporaryContainerLocation);
        return mTemporaryContainerLocation[1];
    }

    @CalledByNative
    private boolean hasFocus() {
        ViewGroup containerView = getContainerView();
        return containerView == null ? false : ViewUtils.hasFocus(containerView);
    }

    @CalledByNative
    private void requestFocus() {
        ViewGroup containerView = getContainerView();
        if (containerView != null) ViewUtils.requestFocus(containerView);
    }

    /**
     * Create and return a basic implementation of {@link ViewAndroidDelegate} where
     * the container view is not allowed to be changed after initialization.
     * @param containerView {@link ViewGroup} to be used as a container view.
     * @return a new instance of {@link ViewAndroidDelegate}.
     */
    public static ViewAndroidDelegate createBasicDelegate(ViewGroup containerView) {
        return new ViewAndroidDelegate() {
            private ViewGroup mContainerView;

            private ViewAndroidDelegate init(ViewGroup containerView) {
                mContainerView = containerView;
                return this;
            }

            @Override
            public ViewGroup getContainerView() {
                return mContainerView;
            }
        }.init(containerView);
    }
}

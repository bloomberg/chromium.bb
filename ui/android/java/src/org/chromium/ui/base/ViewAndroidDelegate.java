// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.annotation.TargetApi;
import android.content.ClipData;
import android.graphics.Bitmap;
import android.os.Build;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout.LayoutParams;
import android.widget.ImageView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.blink_public.web.WebCursorInfoType;

/**
 * Class to acquire, position, and remove anchor views from the implementing View.
 */
@JNINamespace("ui")
public class ViewAndroidDelegate {
    /**
     * The current container view. This view can be updated with
     * {@link #setContainerView()}.
     */
    protected ViewGroup mContainerView;

    // Temporary storage for use as a parameter of getLocationOnScreen().
    private int[] mTemporaryContainerLocation = new int[2];

    /**
     * Notifies the observer when container view is updated.
     */
    public interface ContainerViewObserver { void onUpdateContainerView(ViewGroup view); }

    private ObserverList<ContainerViewObserver> mContainerViewObservers = new ObserverList<>();

    /**
     * Create and return a basic implementation of {@link ViewAndroidDelegate}.
     * @param containerView {@link ViewGroup} to be used as a container view.
     * @return a new instance of {@link ViewAndroidDelegate}.
     */
    public static ViewAndroidDelegate createBasicDelegate(ViewGroup containerView) {
        return new ViewAndroidDelegate(containerView);
    }

    protected ViewAndroidDelegate(ViewGroup containerView) {
        mContainerView = containerView;
    }

    /**
     * Adds observer that needs notification when container view is updated. Note that
     * there is no {@code removObserver} since the added observers are all supposed to
     * go away with this object together.
     * @param observer {@link ContainerViewObserver} object. The object should have
     *        the lifetime same as this {@link ViewAndroidDelegate} to avoid gc issues.
     */
    public final void addObserver(ContainerViewObserver observer) {
        mContainerViewObservers.addObserver(observer);
    }

    /**
     * Updates the current container view to which this class delegates.
     *
     * <p>WARNING: This method can also be used to replace the existing container view,
     * but you should only do it if you have a very good reason to. Replacing the
     * container view has been designed to support fullscreen in the Webview so it
     * might not be appropriate for other use cases.
     *
     * <p>This method only performs a small part of replacing the container view and
     * embedders are responsible for:
     * <ul>
     *     <li>Disconnecting the old container view from all the references</li>
     *     <li>Updating the InternalAccessDelegate</li>
     *     <li>Reconciling the state with the new container view</li>
     *     <li>Tearing down and recreating the native GL rendering where appropriate</li>
     *     <li>etc.</li>
     * </ul>
     */
    public final void setContainerView(ViewGroup containerView) {
        ViewGroup oldContainerView = mContainerView;
        mContainerView = containerView;
        updateAnchorViews(oldContainerView);
        for (ContainerViewObserver observer : mContainerViewObservers) {
            observer.onUpdateContainerView(containerView);
        }
    }

    /**
     * Transfer existing anchor views from the old to the new container view. Called by
     * {@link setContainerView} only.
     * @param oldContainerView Old container view just replaced by a new one.
     */
    public void updateAnchorViews(ViewGroup oldContainerView) {}

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
     * Set the anchor view to specified position and size (all units in px).
     * @param view The anchor view that needs to be positioned.
     * @param x X coordinate of the top left corner of the anchor view.
     * @param y Y coordinate of the top left corner of the anchor view.
     * @param width The width of the anchor view.
     * @param height The height of the anchor view.
     */
    @CalledByNative
    public void setViewPosition(
            View view, float x, float y, float width, float height, int leftMargin, int topMargin) {
        ViewGroup containerView = getContainerView();
        if (containerView == null) return;

        int widthInt = Math.round(width);
        int heightInt = Math.round(height);
        int startMargin;

        if (ApiCompatibilityUtils.isLayoutRtl(containerView)) {
            startMargin = containerView.getMeasuredWidth() - Math.round(width + x);
        } else {
            startMargin = leftMargin;
        }
        if (widthInt + startMargin > containerView.getWidth()) {
            widthInt = containerView.getWidth() - startMargin;
        }
        LayoutParams lp = new LayoutParams(widthInt, heightInt);
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
            case WebCursorInfoType.NONE:
                pointerIconType = PointerIcon.TYPE_NULL;
                break;
            case WebCursorInfoType.POINTER:
                pointerIconType = PointerIcon.TYPE_ARROW;
                break;
            case WebCursorInfoType.CONTEXT_MENU:
                pointerIconType = PointerIcon.TYPE_CONTEXT_MENU;
                break;
            case WebCursorInfoType.HAND:
                pointerIconType = PointerIcon.TYPE_HAND;
                break;
            case WebCursorInfoType.HELP:
                pointerIconType = PointerIcon.TYPE_HELP;
                break;
            case WebCursorInfoType.WAIT:
                pointerIconType = PointerIcon.TYPE_WAIT;
                break;
            case WebCursorInfoType.CELL:
                pointerIconType = PointerIcon.TYPE_CELL;
                break;
            case WebCursorInfoType.CROSS:
                pointerIconType = PointerIcon.TYPE_CROSSHAIR;
                break;
            case WebCursorInfoType.I_BEAM:
                pointerIconType = PointerIcon.TYPE_TEXT;
                break;
            case WebCursorInfoType.VERTICAL_TEXT:
                pointerIconType = PointerIcon.TYPE_VERTICAL_TEXT;
                break;
            case WebCursorInfoType.ALIAS:
                pointerIconType = PointerIcon.TYPE_ALIAS;
                break;
            case WebCursorInfoType.COPY:
                pointerIconType = PointerIcon.TYPE_COPY;
                break;
            case WebCursorInfoType.NO_DROP:
                pointerIconType = PointerIcon.TYPE_NO_DROP;
                break;
            case WebCursorInfoType.COLUMN_RESIZE:
                pointerIconType = PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.ROW_RESIZE:
                pointerIconType = PointerIcon.TYPE_VERTICAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.NORTH_EAST_SOUTH_WEST_RESIZE:
                pointerIconType = PointerIcon.TYPE_TOP_RIGHT_DIAGONAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.NORTH_WEST_SOUTH_EAST_RESIZE:
                pointerIconType = PointerIcon.TYPE_TOP_LEFT_DIAGONAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.ZOOM_IN:
                pointerIconType = PointerIcon.TYPE_ZOOM_IN;
                break;
            case WebCursorInfoType.ZOOM_OUT:
                pointerIconType = PointerIcon.TYPE_ZOOM_OUT;
                break;
            case WebCursorInfoType.GRAB:
                pointerIconType = PointerIcon.TYPE_GRAB;
                break;
            case WebCursorInfoType.GRABBING:
                pointerIconType = PointerIcon.TYPE_GRABBING;
                break;
            // TODO(jaebaek): set types correctly
            // after fixing http://crbug.com/584424.
            case WebCursorInfoType.EAST_WEST_RESIZE:
                pointerIconType = PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.NORTH_SOUTH_RESIZE:
                pointerIconType = PointerIcon.TYPE_VERTICAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.EAST_RESIZE:
                pointerIconType = PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.NORTH_RESIZE:
                pointerIconType = PointerIcon.TYPE_VERTICAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.NORTH_EAST_RESIZE:
                pointerIconType = PointerIcon.TYPE_TOP_RIGHT_DIAGONAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.NORTH_WEST_RESIZE:
                pointerIconType = PointerIcon.TYPE_TOP_LEFT_DIAGONAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.SOUTH_RESIZE:
                pointerIconType = PointerIcon.TYPE_VERTICAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.SOUTH_EAST_RESIZE:
                pointerIconType = PointerIcon.TYPE_TOP_LEFT_DIAGONAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.SOUTH_WEST_RESIZE:
                pointerIconType = PointerIcon.TYPE_TOP_RIGHT_DIAGONAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.WEST_RESIZE:
                pointerIconType = PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW;
                break;
            case WebCursorInfoType.PROGRESS:
                pointerIconType = PointerIcon.TYPE_WAIT;
                break;
            case WebCursorInfoType.NOT_ALLOWED:
                pointerIconType = PointerIcon.TYPE_NO_DROP;
                break;
            case WebCursorInfoType.MOVE:
            case WebCursorInfoType.MIDDLE_PANNING:
                pointerIconType = PointerIcon.TYPE_ALL_SCROLL;
                break;
            case WebCursorInfoType.EAST_PANNING:
            case WebCursorInfoType.NORTH_PANNING:
            case WebCursorInfoType.NORTH_EAST_PANNING:
            case WebCursorInfoType.NORTH_WEST_PANNING:
            case WebCursorInfoType.SOUTH_PANNING:
            case WebCursorInfoType.SOUTH_EAST_PANNING:
            case WebCursorInfoType.SOUTH_WEST_PANNING:
            case WebCursorInfoType.WEST_PANNING:
                assert false : "These pointer icon types are not supported";
                break;
            case WebCursorInfoType.CUSTOM:
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
    public final ViewGroup getContainerView() {
        return mContainerView;
    }

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

    /**
     * Return the X location of our container view on screen.
     */
    @CalledByNative
    private int getXLocationOnScreen() {
        ViewGroup container = getContainerView();
        if (container == null) return 0;

        container.getLocationOnScreen(mTemporaryContainerLocation);
        return mTemporaryContainerLocation[0];
    }

    /**
     * Return the Y location of our container view on screen.
     */
    @CalledByNative
    private int getYLocationOnScreen() {
        ViewGroup container = getContainerView();
        if (container == null) return 0;

        container.getLocationOnScreen(mTemporaryContainerLocation);
        return mTemporaryContainerLocation[1];
    }

    @CalledByNative
    private void requestDisallowInterceptTouchEvent() {
        ViewGroup container = getContainerView();
        if (container != null) container.requestDisallowInterceptTouchEvent(true);
    }

    @CalledByNative
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void requestUnbufferedDispatch(MotionEvent event) {
        ViewGroup container = getContainerView();
        if (container != null) container.requestUnbufferedDispatch(event);
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
}

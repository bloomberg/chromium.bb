// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.Matrix;
import android.graphics.PointF;
import android.graphics.Rect;
import android.graphics.RectF;

/**
 * This class is responsible for transforming the desktop image matrix.
 */
public class DesktopCanvas {
    /**
     * Maximum allowed zoom level - see {@link #scaleAndRepositionImage()}.
     */
    private static final float MAX_ZOOM_FACTOR = 100.0f;

    /**
     * Used to smoothly reduce the amount of padding while the user is zooming.
     */
    private static final float PADDING_REDUCTION_FACTOR = 0.85f;

    private final RenderStub mRenderStub;
    private final RenderData mRenderData;

    /**
     * Represents the actual center of the viewport in image space.  This value needs to be a pair
     * of floats so the desktop image can be positioned with sub-pixel accuracy for smoother panning
     * animations at high zoom levels.
     */
    // TODO(joedow): See if we can collapse Viewport and Cursor position members.  They were needed
    // in the past due to how we calculated the center positions but may not be needed now.
    private PointF mViewportPosition = new PointF();

    /**
     * Represents the desired center of the viewport in image space.  This value may not represent
     * the actual center of the viewport as adjustments are made to ensure as much of the desktop is
     * visible as possible.  This value needs to be a pair of floats so the desktop image can be
     * positioned with sub-pixel accuracy for smoother panning animations at high zoom levels.
     * Note: The internal cursor position may be placed outside of the image boundary, however the
     * cursor position we inject on the host side is restricted to the actual image dimensions.
     */
    private PointF mCursorPosition = new PointF();

    /**
     * Represents the amount of space, in pixels, used by System UI on each edge of the screen.
     */
    // TODO(joedow): Update usage of this member so it is a true Rect instead of a set of offsets.
    private Rect mSystemUiScreenSize = new Rect();

    /**
     * Represents the amount of padding, in screen pixels, added to each edge of the desktop image.
     * This extra space allows the user to reveal portions of the desktop image which are obscured
     * by System UI.
     */
    private RectF mVisibleImagePadding = new RectF();

    /**
     * Tracks whether to adjust the viewport to account for System UI. If false, the viewport
     * center is the center of the screen.  If true, then System UI offsets will be used to
     * adjust the position of the viewport to ensure the cursor is visible.
     */
    private boolean mAdjustViewportForSystemUi = false;

    /**
    * Represents the amount of space, in pixels, to adjust the cursor center along the y-axis.
    */
    private float mCursorOffsetScreenY = 0.0f;

    public DesktopCanvas(RenderStub renderStub, RenderData renderData) {
        mRenderStub = renderStub;
        mRenderData = renderData;
    }

    /**
     * Sets the desired center position of the viewport (a.k.a. the cursor position) and ensures
     * the viewport is updated to include the cursor within it.
     *
     * @param newX The new x coordinate value for the desired center position.
     * @param newY The new y coordinate value for the desired center position.
     */
    public void setCursorPosition(float newX, float newY) {
        // First set the cursor position since its potential values are a superset of the viewport.
        mCursorPosition.set(newX, newY);
        constrainPointToBounds(mCursorPosition, getImageBounds());

        // Now set the viewport position based on the cursor.
        mViewportPosition.set(mCursorPosition);
        constrainPointToBounds(mViewportPosition, getViewportBounds());

        repositionImage();
    }

    /**
     * Shifts the viewport by the passed in values (in image coordinates).
     *
     * @param deltaX The distance (in image coordinates) to move the viewport along the x-axis.
     * @param deltaY The distance (in image coordinates) to move the viewport along the y-axis.
     */
    public void moveViewportCenter(float deltaX, float deltaY) {
        // Offset and adjust the viewport center position to fit the screen.
        mViewportPosition.offset(deltaX, deltaY);
        constrainPointToBounds(mViewportPosition, getViewportBounds());

        // We don't need to constrain the cursor position as the viewport position is always within
        // the bounds of the potential cursor positions.
        mCursorPosition.set(mViewportPosition);

        repositionImage();
    }

    /**
     * Shifts the cursor by the passed in values (in image coordinates) and adjusts the viewport.
     *
     * @param deltaX The distance (in image coordinates) to move the cursor along the x-axis.
     * @param deltaY The distance (in image coordinates) to move the cursor along the y-axis.
     * @return A point representing the new cursor position.
     */
    public PointF moveCursorPosition(float deltaX, float deltaY) {
        setCursorPosition(mCursorPosition.x + deltaX, mCursorPosition.y + deltaY);
        return new PointF(mCursorPosition.x, mCursorPosition.y);
    }

    /**
     * Handles System UI size and visibility changes.
     *
     * @param parameter The set of values defining the current System UI state.
     */
    public void onSystemUiVisibilityChanged(SystemUiVisibilityChangedEventParameter parameter) {
        if (parameter.softInputMethodVisible) {
            mSystemUiScreenSize.set(parameter.left, parameter.top,
                    mRenderData.screenWidth - parameter.right,
                    mRenderData.screenHeight - parameter.bottom);

            if (mAdjustViewportForSystemUi) {
                // Adjust the cursor position to ensure it's visible when large System UI (1/3 or
                // more of the total screen size) is displayed (typically the Soft Keyboard).
                // Without this change, it is difficult for users to enter text into edit controls
                // which are located bottom of the screen and may not see the cursor at all.
                if (mSystemUiScreenSize.bottom > (mRenderData.screenHeight / 3)) {
                    // Center the cursor within the viewable area (not obscured by System UI).
                    mCursorOffsetScreenY = (float) parameter.bottom / 2.0f;
                } else {
                    mCursorOffsetScreenY = 0.0f;
                }

                // Apply the cursor offset.
                setCursorPosition(mCursorPosition.x, mCursorPosition.y);
            }
        } else {
            mCursorOffsetScreenY = 0.0f;
            mSystemUiScreenSize.setEmpty();
        }
    }

    public void adjustViewportForSystemUi(boolean adjustViewportForSystemUi) {
        mAdjustViewportForSystemUi = adjustViewportForSystemUi;
    }

    /** Resizes the image by zooming it such that the image is displayed without borders. */
    public void resizeImageToFitScreen() {
        // Protect against being called before the image has been initialized.
        if (mRenderData.imageWidth == 0 || mRenderData.imageHeight == 0) {
            return;
        }

        float widthRatio = (float) mRenderData.screenWidth / mRenderData.imageWidth;
        float heightRatio = (float) mRenderData.screenHeight / mRenderData.imageHeight;
        float screenToImageScale = Math.max(widthRatio, heightRatio);

        // If the image is smaller than the screen in either dimension, then we want to scale it
        // up to fit both and fill the screen with the image of the remote desktop.
        if (screenToImageScale > 1.0f) {
            mRenderData.transform.setScale(screenToImageScale, screenToImageScale);
        }
    }

    /**
     * Repositions the image by translating and zooming it, to keep the zoom level within sensible
     * limits. The minimum zoom level is chosen to avoid black space around all 4 sides. The
     * maximum zoom level is set arbitrarily, so that the user can zoom out again in a reasonable
     * time, and to prevent arithmetic overflow problems from displaying the image.
     *
     * @param scaleFactor The factor used to zoom the canvas in or out.
     * @param px The center x coordinate for the scale action.
     * @param py The center y coordinate for the scale action.
     * @param centerOnCursor Determines whether the viewport will be translated to the desired
     *                       center position before being adjusted to fit the screen boundaries.
     */
    public void scaleAndRepositionImage(
            float scaleFactor, float px, float py, boolean centerOnCursor) {
        // Avoid division by zero in case this gets called before the image size is initialized.
        if (mRenderData.imageWidth == 0 || mRenderData.imageHeight == 0) {
            return;
        }

        mRenderData.transform.postScale(scaleFactor, scaleFactor, px, py);

        // Zoom out if the zoom level is too high.
        float currentZoomLevel = mRenderData.transform.mapRadius(1.0f);
        if (currentZoomLevel > MAX_ZOOM_FACTOR) {
            mRenderData.transform.setScale(MAX_ZOOM_FACTOR, MAX_ZOOM_FACTOR);
        }

        // Get image size scaled to screen coordinates.
        float[] imageSize = {mRenderData.imageWidth, mRenderData.imageHeight};
        mRenderData.transform.mapVectors(imageSize);

        if (imageSize[0] < mRenderData.screenWidth && imageSize[1] < mRenderData.screenHeight) {
            // Displayed image is too small in both directions, so apply the minimum zoom
            // level needed to fit either the width or height.
            float scale = Math.min((float) mRenderData.screenWidth / mRenderData.imageWidth,
                                   (float) mRenderData.screenHeight / mRenderData.imageHeight);
            mRenderData.transform.setScale(scale, scale);
        }

        // Trim the image padding if the user is zooming out.  This prevents cases where the image
        // pops to the center when it reaches its minimum size.  Note that we do not need to do
        // anything when the user is zooming in as the canvas will expand and absorb the padding.
        if (scaleFactor < 1.0f) {
            mVisibleImagePadding.set(mVisibleImagePadding.left * PADDING_REDUCTION_FACTOR,
                    mVisibleImagePadding.top * PADDING_REDUCTION_FACTOR,
                    mVisibleImagePadding.right * PADDING_REDUCTION_FACTOR,
                    mVisibleImagePadding.bottom * PADDING_REDUCTION_FACTOR);
        }

        if (centerOnCursor) {
            setCursorPosition(mCursorPosition.x, mCursorPosition.y);
        } else {
            // Find the new screen center (it was probably changed during the zoom operation) and
            // update the viewport and cursor.
            float[] mappedPoints = {
                    ((float) mRenderData.screenWidth / 2), ((float) mRenderData.screenHeight / 2)};
            Matrix screenToImage = new Matrix();
            mRenderData.transform.invert(screenToImage);
            screenToImage.mapPoints(mappedPoints);
            // The cursor is mapped to the center of the viewport in this case.
            setCursorPosition(mappedPoints[0], mappedPoints[1]);
        }
    }

    /**
     * Repositions the image by translating it (without affecting the zoom level).
     */
    private void repositionImage() {
        // Map the current viewport position to screen coordinates and adjust the image position.
        float[] viewportPosition = {mViewportPosition.x, mViewportPosition.y};
        mRenderData.transform.mapPoints(viewportPosition);

        float viewportTransX = ((float) mRenderData.screenWidth / 2) - viewportPosition[0];
        float viewportTransY =
                ((float) mRenderData.screenHeight / 2) - viewportPosition[1] - mCursorOffsetScreenY;

        // Translate the image to move the viewport to the expected screen location.
        mRenderData.transform.postTranslate(viewportTransX, viewportTransY);

        updateVisibleImagePadding();

        mRenderStub.setTransformation(mRenderData.transform);
    }

    /**
     * Updates the given point such that it refers to a coordinate within the bounds provided.
     *
     * @param point The point to adjust, the original object is modified.
     * @param bounds The bounds used to constrain the point.
     */
    private void constrainPointToBounds(PointF point, RectF bounds) {
        if (point.x < bounds.left) {
            point.x = bounds.left;
        } else if (point.x > bounds.right) {
            point.x = bounds.right;
        }

        if (point.y < bounds.top) {
            point.y = bounds.top;
        } else if (point.y > bounds.bottom) {
            point.y = bounds.bottom;
        }
    }

    /** Returns a region which defines the set of valid cursor positions in image space. */
    private RectF getImageBounds() {
        // The set of valid cursor positions includes any point on the image as well as the padding.
        // Padding is additional space added to the image which is the larger value of:
        //   - Potential overlap of the System UI and image content
        //   - Actual amount of padding already being used
        //
        // By expanding the area, we allow the user to move the cursor 'under' the System UI which
        // pulls the content out from under it and allows it to be visible.  Once the System UI has
        // been dismissed or changes size, we use the actual padding value instead which prevents
        // the desktop image from 'snapping' back to pre-System UI state.
        RectF systemUiOverlap = getSystemUiOverlap();
        float[] padding = {Math.max(mVisibleImagePadding.left, systemUiOverlap.left),
                Math.max(mVisibleImagePadding.top + mCursorOffsetScreenY, systemUiOverlap.top),
                Math.max(mVisibleImagePadding.right, systemUiOverlap.right),
                Math.max(mVisibleImagePadding.bottom - mCursorOffsetScreenY,
                        systemUiOverlap.bottom)};
        Matrix screenToImage = new Matrix();
        mRenderData.transform.invert(screenToImage);
        screenToImage.mapVectors(padding);

        return new RectF(-padding[0], -padding[1], mRenderData.imageWidth + padding[2],
                mRenderData.imageHeight + padding[3]);
    }

    /** Returns a region which defines the set of valid viewport center values in image space. */
    private RectF getViewportBounds() {
        // The region of allowable viewport values is the imageBound rect, inset by the size of the
        // viewport itself.  This prevents over and under panning of the viewport while still
        // allowing the user to see and interact with all pixels one the desktop image.
        Matrix screenToImage = new Matrix();
        mRenderData.transform.invert(screenToImage);

        float[] screenVectors = {(float) mRenderData.screenWidth, (float) mRenderData.screenHeight};
        screenToImage.mapVectors(screenVectors);

        PointF letterboxPadding = getLetterboxPadding();
        float[] letterboxPaddingVectors = {letterboxPadding.x, letterboxPadding.y};
        screenToImage.mapVectors(letterboxPaddingVectors);

        // screenCenter values are 1/2 of a particular screen dimension mapped to image space.
        float screenCenterX = (screenVectors[0] / 2.0f) - letterboxPaddingVectors[0];
        float screenCenterY = (screenVectors[1] / 2.0f) - letterboxPaddingVectors[1];
        RectF imageBounds = getImageBounds();
        imageBounds.inset(screenCenterX, screenCenterY);
        return imageBounds;
    }

    /**
     * Provides the amount of padding needed to center the image content on the screen.
     */
    private PointF getLetterboxPadding() {
        float[] imageVectors = {mRenderData.imageWidth, mRenderData.imageHeight};
        mRenderData.transform.mapVectors(imageVectors);

        // We want to letterbox when the image is smaller than the screen in a specific dimension.
        // Since we center the image, split the difference so it is equally distributed.
        float widthAdjust =
                Math.max(((float) mRenderData.screenWidth - imageVectors[0]) / 2.0f, 0.0f);
        float heightAdjust =
                Math.max(((float) mRenderData.screenHeight - imageVectors[1]) / 2.0f, 0.0f);

        return new PointF(widthAdjust, heightAdjust);
    }

    /**
     * Returns the amount of System UI along each edge of the screen which could overlap the remote
     * desktop image below it.  This is the maximum amount that could overlap, not the actual value.
     */
    private RectF getSystemUiOverlap() {
        // letterBox padding represents the space added to the image to center it on the screen.
        // Since it does not contain any interactable UI, we ignore it when calculating the overlap
        // between the System UI and the remote desktop image.
        // Note: Ignore negative padding (clamp to 0) since that means no overlap exists.
        PointF letterboxPadding = getLetterboxPadding();
        return new RectF(Math.max(mSystemUiScreenSize.left - letterboxPadding.x, 0.0f),
                Math.max(mSystemUiScreenSize.top - letterboxPadding.y + mCursorOffsetScreenY, 0.0f),
                Math.max(mSystemUiScreenSize.right - letterboxPadding.x, 0.0f),
                Math.max(mSystemUiScreenSize.bottom - letterboxPadding.y - mCursorOffsetScreenY,
                        0.0f));
    }

    /**
     * Calculates the amount of padding visible on each edge of the desktop image.
     */
    private void updateVisibleImagePadding() {
        PointF letterboxPadding = getLetterboxPadding();
        float[] imagePoints = {0.0f, 0.0f, mRenderData.imageWidth, mRenderData.imageHeight};
        mRenderData.transform.mapPoints(imagePoints);

        mVisibleImagePadding.set(Math.max(imagePoints[0] - letterboxPadding.x, 0.0f),
                Math.max(imagePoints[1] - letterboxPadding.y, 0.0f),
                Math.max(mRenderData.screenWidth - imagePoints[2] - letterboxPadding.x, 0.0f),
                Math.max(mRenderData.screenHeight - imagePoints[3] - letterboxPadding.y, 0.0f));
    }
}

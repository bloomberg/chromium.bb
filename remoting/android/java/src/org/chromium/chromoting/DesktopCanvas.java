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
     * Maximum allowed zoom level - see {@link #repositionImageWithZoom()}.
     */
    private static final float MAX_ZOOM_FACTOR = 100.0f;

    private final RenderStub mRenderStub;
    private final RenderData mRenderData;

    /**
     * Represents the actual center of the viewport.  This value needs to be a pair of floats so the
     * desktop image can be positioned with sub-pixel accuracy for smoother panning animations at
     * high zoom levels.
     */
    private PointF mViewportPosition = new PointF();

    /**
     * Represents the desired center of the viewport.  This value may not represent the actual
     * center of the viewport as adjustments are made to ensure as much of the desktop is visible as
     * possible.  This value needs to be a pair of floats so the desktop image can be positioned
     * with sub-pixel accuracy for smoother panning animations at high zoom levels.
     */
    private PointF mCursorPosition = new PointF();

    /**
     * Represents the amount of space, in pixels, used by System UI.
     */
    private Rect mSystemUiOffsetPixels = new Rect();

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
     * Sets the offset values used to calculate the space used by System UI.
     *
     * @param left The space used by System UI on the left edge of the screen.
     * @param top The space used by System UI on the top edge of the screen.
     * @param right The space used by System UI on the right edge of the screen.
     * @param bottom The space used by System UI on the bottom edge of the screen.
     */
    public void setSystemUiOffsetValues(int left, int top, int right, int bottom) {
        mSystemUiOffsetPixels.set(left, top, right, bottom);
    }

    /** Called to indicate that no System UI is visible. */
    public void clearSystemUiOffsets() {
        mSystemUiOffsetPixels.setEmpty();
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
        float viewportTransY = ((float) mRenderData.screenHeight / 2) - viewportPosition[1];

        // Translate the image so the viewport center is displayed in the middle of the screen.
        mRenderData.transform.postTranslate(viewportTransX, viewportTransY);

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

    /** Returns a region which defines the set of valid cursor values in image space. */
    private RectF getImageBounds() {
        return new RectF(0, 0, mRenderData.imageWidth, mRenderData.imageHeight);
    }

    /** Returns a region which defines the set of valid viewport center values in image space. */
    private RectF getViewportBounds() {
        float[] screenVectors = {(float) mRenderData.screenWidth, (float) mRenderData.screenHeight};
        Matrix screenToImage = new Matrix();
        mRenderData.transform.invert(screenToImage);
        screenToImage.mapVectors(screenVectors);

        float xAdjust = 0.0f;
        if (mRenderData.imageWidth < screenVectors[0]) {
            // Image is narrower than the screen, so adjust the bounds to center it.
            xAdjust = (screenVectors[0] - mRenderData.imageWidth) / 2.0f;
        }

        float yAdjust = 0.0f;
        if (mRenderData.imageHeight < screenVectors[1]) {
            // Image is shorter than the screen, so adjust the bounds to center it.
            yAdjust = (screenVectors[1] - mRenderData.imageHeight) / 2.0f;
        }

        // screenCenter values are 1/2 of a particular screen dimension mapped to image space.
        float screenCenterX = screenVectors[0] / 2.0f;
        float screenCenterY = screenVectors[1] / 2.0f;
        return new RectF(screenCenterX - xAdjust, screenCenterY - yAdjust,
                mRenderData.imageWidth - screenCenterX + xAdjust,
                mRenderData.imageHeight - screenCenterY + yAdjust);
    }
}

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.PointF;
import android.graphics.RectF;

/**
 * This class is responsible for transforming the desktop image matrix.
 */
public class DesktopCanvas {
    /**
     * Maximum allowed zoom level - see {@link #repositionImageWithZoom()}.
     */
    private static final float MAX_ZOOM_FACTOR = 100.0f;

    private final DesktopViewInterface mViewer;
    private final RenderData mRenderData;

    /**
     * Represents the desired center of the viewport.  This value may not represent the actual
     * center of the viewport as adjustments are made to ensure as much of the desktop is visible as
     * possible.  This value needs to be a pair of floats so the desktop image can be positioned
     * with sub-pixel accuracy for smoother panning animations at high zoom levels.
     */
    private PointF mViewportPosition = new PointF();

    /**
     * Represents the amount of vertical space in pixels used by the soft input device and
     * accompanying system UI.
     */
    private int mInputMethodOffsetY = 0;

    /**
     * Represents the amount of horizontal space in pixels used by the soft input device and
     * accompanying system UI.
     */
    private int mInputMethodOffsetX = 0;

    public DesktopCanvas(DesktopViewInterface viewer, RenderData renderData) {
        mViewer = viewer;
        mRenderData = renderData;
    }

    /**
     * Returns the desired center position of the viewport.  Note that this may not represent the
     * true center of the viewport as other calculations are done to maximize the viewable area.
     *
     * @return A point representing the desired position of the viewport.
     */
    public PointF getViewportPosition() {
        return new PointF(mViewportPosition.x, mViewportPosition.y);
    }

    /**
     * Sets the desired center position of the viewport.
     *
     * @param newX The new x coordinate value for the desired center position.
     * @param newY The new y coordinate value for the desired center position.
     */
    public void setViewportPosition(float newX, float newY) {
        mViewportPosition.set(newX, newY);
    }

    /**
     * Sets the offset values used to calculate the space used by the current soft input method.
     *
     * @param offsetX The space used by the soft input method UI on the right edge of the screen.
     * @param offsetY The space used by the soft input method UI on the bottom edge of the screen.
     */
    public void setInputMethodOffsetValues(int offsetX, int offsetY) {
        mInputMethodOffsetX = offsetX;
        mInputMethodOffsetY = offsetY;
    }

    /** Repositions the image by zooming it such that the complete image fits on the screen. */
    public void resizeImageToFitScreen() {
        synchronized (mRenderData) {
            // Protect against being called before the image has been initialized.
            if (mRenderData.imageWidth == 0 || mRenderData.imageHeight == 0) {
                return;
            }

            float screenToImageScale = 1.0f;
            float[] imageSize = {mRenderData.imageWidth, mRenderData.imageHeight};
            mRenderData.transform.mapVectors(imageSize);

            // If the image is smaller than the screen in both dimensions, then we want
            // to scale it up to fit.
            boolean scaleImageUp = imageSize[0] < mRenderData.screenWidth
                    && imageSize[1] < mRenderData.screenHeight;

            // If the image is larger than the screen in any dimension, we want to
            // shrink it to fit.
            boolean scaleImageDown = imageSize[0] > mRenderData.screenWidth
                    || imageSize[1] > mRenderData.screenHeight;

            if (scaleImageUp || scaleImageDown) {
                // Displayed image is too small or too large to fit the screen dimensions.
                // Apply the minimum scale needed to fit both the width and height.
                screenToImageScale =
                        Math.min((float) mRenderData.screenWidth / mRenderData.imageWidth,
                                 (float) mRenderData.screenHeight / mRenderData.imageHeight);
                mRenderData.transform.setScale(screenToImageScale, screenToImageScale);
            }
        }

        repositionImage();
    }

    /**
     * Repositions the image by translating it (without affecting the zoom level) to place the
     * viewport close to the center of the screen.
     */
    public void repositionImage() {
        synchronized (mRenderData) {
            float adjustedScreenWidth = mRenderData.screenWidth - mInputMethodOffsetX;
            float adjustedScreenHeight = mRenderData.screenHeight - mInputMethodOffsetY;

            // The goal of the code below is to position the viewport as close to the desired center
            // position as possible whilst keeping as much of the desktop in view as possible.
            // To achieve these goals, we first position the desktop image at the desired center
            // point and then re-position it to maximize the viewable area.

            // Map the current viewport position to screen coordinates.
            float[] viewportPosition = {mViewportPosition.x, mViewportPosition.y};
            mRenderData.transform.mapPoints(viewportPosition);

            // Translate so the viewport is displayed in the middle of the screen.
            mRenderData.transform.postTranslate(
                    (float) adjustedScreenWidth / 2 - viewportPosition[0],
                    (float) adjustedScreenHeight / 2 - viewportPosition[1]);

            // Get the coordinates of the desktop rectangle (top-left/bottom-right corners) in
            // screen coordinates. Order is: left, top, right, bottom.
            RectF rectScreen = new RectF(0, 0, mRenderData.imageWidth, mRenderData.imageHeight);
            mRenderData.transform.mapRect(rectScreen);

            float leftDelta = rectScreen.left;
            float rightDelta = rectScreen.right - mRenderData.screenWidth + mInputMethodOffsetX;
            float topDelta = rectScreen.top;
            float bottomDelta = rectScreen.bottom - mRenderData.screenHeight + mInputMethodOffsetY;
            float xAdjust = 0;
            float yAdjust = 0;

            if (rectScreen.right - rectScreen.left < adjustedScreenWidth) {
                // Image is narrower than the screen, so center it.
                xAdjust = -(rightDelta + leftDelta) / 2;
            } else if (leftDelta > 0 && rightDelta > 0) {
                // Panning the image left will show more of it.
                xAdjust = -Math.min(leftDelta, rightDelta);
            } else if (leftDelta < 0 && rightDelta < 0) {
                // Pan the image right.
                xAdjust = Math.min(-leftDelta, -rightDelta);
            }

            // Apply similar logic for yAdjust.
            if (rectScreen.bottom - rectScreen.top < adjustedScreenHeight) {
                yAdjust = -(bottomDelta + topDelta) / 2;
            } else if (topDelta > 0 && bottomDelta > 0) {
                yAdjust = -Math.min(topDelta, bottomDelta);
            } else if (topDelta < 0 && bottomDelta < 0) {
                yAdjust = Math.min(-topDelta, -bottomDelta);
            }

            mRenderData.transform.postTranslate(xAdjust, yAdjust);

            mViewer.transformationChanged();
        }
    }

    /**
     * Repositions the image by translating and zooming it, to keep the zoom level within sensible
     * limits. The minimum zoom level is chosen to avoid black space around all 4 sides. The
     * maximum zoom level is set arbitrarily, so that the user can zoom out again in a reasonable
     * time, and to prevent arithmetic overflow problems from displaying the image.
     */
    public void repositionImageWithZoom() {
        synchronized (mRenderData) {
            // Avoid division by zero in case this gets called before the image size is initialized.
            if (mRenderData.imageWidth == 0 || mRenderData.imageHeight == 0) {
                return;
            }

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
        }

        repositionImage();
    }
}

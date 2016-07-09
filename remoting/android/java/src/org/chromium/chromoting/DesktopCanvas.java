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

    private final AbstractDesktopView mViewer;
    private final RenderData mRenderData;

    /**
     * Represents the desired center of the viewport.  This value may not represent the actual
     * center of the viewport as adjustments are made to ensure as much of the desktop is visible as
     * possible.  This value needs to be a pair of floats so the desktop image can be positioned
     * with sub-pixel accuracy for smoother panning animations at high zoom levels.
     */
    private PointF mViewportPosition = new PointF();

    /**
     * Represents the amount of space, in pixels, used by system UI.
     */
    private Rect mSystemUiOffsetPixels = new Rect();

    public DesktopCanvas(AbstractDesktopView viewer, RenderData renderData) {
        mViewer = viewer;
        mRenderData = renderData;
    }

    /**
     * Shifts the viewport by the passed in deltas (in image coordinates).
     *
     * @param useScreenCenter Determines whether to use the desired viewport position or the actual
     *                        center of the screen for positioning.
     * @param deltaX The distance (in image coordinates) to move the viewport along the x-axis.
     * @param deltaY The distance (in image coordinates) to move the viewport along the y-axis.
     * @return A point representing the new center position of the viewport.
     */
    public PointF moveViewportCenter(boolean useScreenCenter, float deltaX, float deltaY) {
        PointF viewportCenter;
        synchronized (mRenderData) {
            RectF bounds = new RectF(0, 0, mRenderData.imageWidth, mRenderData.imageHeight);
            if (useScreenCenter) {
                viewportCenter = getTrueViewportCenter();
            } else {
                viewportCenter = new PointF(mViewportPosition.x, mViewportPosition.y);
            }

            viewportCenter.offset(deltaX, deltaY);
            if (viewportCenter.x < bounds.left) {
                viewportCenter.x = bounds.left;
            } else if (viewportCenter.x > bounds.right) {
                viewportCenter.x = bounds.right;
            }

            if (viewportCenter.y < bounds.top) {
                viewportCenter.y = bounds.top;
            } else if (viewportCenter.y > bounds.bottom) {
                viewportCenter.y = bounds.bottom;
            }

            mViewportPosition.set(viewportCenter);
        }

        return viewportCenter;
    }

    /**
     * Returns the current size of the viewport.  This size includes the offset calculations for
     * any visible system UI.
     *
     * @return A point representing the current size of the viewport.
     */
    private PointF getViewportSize() {
        float adjustedScreenWidth, adjustedScreenHeight;
        synchronized (mRenderData) {
            adjustedScreenWidth = mRenderData.screenWidth - mSystemUiOffsetPixels.right;
            adjustedScreenHeight = mRenderData.screenHeight - mSystemUiOffsetPixels.bottom;
        }

        return new PointF(adjustedScreenWidth, adjustedScreenHeight);
    }

    /**
     * Returns the true center position of the viewport (in image coordinates).
     *
     * @return A point representing the true center position of the viewport.
     */
    private PointF getTrueViewportCenter() {
        synchronized (mRenderData) {
            PointF viewportSize = getViewportSize();

            // Find the center point of the viewport on the screen.
            float[] viewportPosition = {((float) viewportSize.x / 2), ((float) viewportSize.y / 2)};

            // Convert the screen position to an image position.
            Matrix screenToImage = new Matrix();
            mRenderData.transform.invert(screenToImage);
            screenToImage.mapPoints(viewportPosition);
            return new PointF(viewportPosition[0], viewportPosition[1]);
        }
    }

    /**
     * Sets the offset values used to calculate the space used by system UI.
     *
     * @param left The space used by system UI on the left edge of the screen.
     * @param top The space used by system UI on the top edge of the screen.
     * @param right The space used by system UI on the right edge of the screen.
     * @param bottom The space used by system UI on the bottom edge of the screen.
     */
    public void setSystemUiOffsetValues(int left, int top, int right, int bottom) {
        synchronized (mRenderData) {
            mSystemUiOffsetPixels.set(left, top, right, bottom);
        }
    }

    /** Repositions the image by zooming it such that the image is displayed without borders. */
    public void resizeImageToFitScreen() {
        synchronized (mRenderData) {
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

        repositionImage(false);
    }

    /**
     * Repositions the image by translating it (without affecting the zoom level).
     *
     * @param centerViewport Determines whether the viewport will be translated to the desired
     *                       center position before being adjusted to fit the screen boundaries.
     */
    public void repositionImage(boolean centerViewport) {
        PointF viewportSize = getViewportSize();
        synchronized (mRenderData) {
            // The goal of the code below is to position the viewport as close to the desired center
            // position as possible whilst keeping as much of the desktop in view as possible.
            // To achieve these goals, we first position the desktop image at the desired center
            // point and then re-position it to maximize the viewable area.
            if (centerViewport) {
                // Map the current viewport position to screen coordinates.
                float[] viewportPosition = {mViewportPosition.x, mViewportPosition.y};
                mRenderData.transform.mapPoints(viewportPosition);

                // Translate so the viewport is displayed in the middle of the screen.
                mRenderData.transform.postTranslate(
                        ((float) viewportSize.x / 2) - viewportPosition[0],
                        ((float) viewportSize.y / 2) - viewportPosition[1]);
            }

            // Get the coordinates of the desktop rectangle (top-left/bottom-right corners) in
            // screen coordinates. Order is: left, top, right, bottom.
            RectF rectScreen = new RectF(0, 0, mRenderData.imageWidth, mRenderData.imageHeight);
            mRenderData.transform.mapRect(rectScreen);

            float leftDelta = rectScreen.left;
            float rightDelta =
                    rectScreen.right - mRenderData.screenWidth + mSystemUiOffsetPixels.right;
            float topDelta = rectScreen.top;
            float bottomDelta =
                    rectScreen.bottom - mRenderData.screenHeight + mSystemUiOffsetPixels.bottom;
            float xAdjust = 0;
            float yAdjust = 0;

            if (rectScreen.right - rectScreen.left < viewportSize.x) {
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
            if (rectScreen.bottom - rectScreen.top < viewportSize.y) {
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
     *
     * @param centerViewport Determines whether the viewport will be translated to the desired
     *                       center position before being adjusted to fit the screen boundaries.
     */
    public void repositionImageWithZoom(boolean centerViewport) {
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

        repositionImage(centerViewport);
    }
}

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.Matrix;
import android.graphics.Point;

/**
 * This class stores data that needs to be accessed on both the display thread and the
 * event-processing thread.
 */
public class RenderData {
    /** Stores pan and zoom configuration and converts image coordinates to screen coordinates. */
    public Matrix transform = new Matrix();

    public int screenWidth = 0;
    public int screenHeight = 0;
    public int imageWidth = 0;
    public int imageHeight = 0;

    /** Determines whether the local cursor should be drawn. */
    public boolean drawCursor = false;

    /**
     * Specifies the position, in image coordinates, at which the cursor image will be drawn.
     * This will normally be at the location of the most recently injected motion event.
     */
    private Point mCursorPosition = new Point();

    /**
     * Returns the position of the rendered cursor.
     *
     * @return A point representing the current position.
     */
    public Point getCursorPosition() {
        return new Point(mCursorPosition);
    }

    /**
     * Sets the position of the cursor which is used for rendering.
     *
     * @param newX The new value of the x coordinate.
     * @param newY The new value of the y coordinate
     * @return True if the cursor position has changed.
     */
    public boolean setCursorPosition(int newX, int newY) {
        boolean cursorMoved = false;
        if (newX != mCursorPosition.x) {
            mCursorPosition.x = newX;
            cursorMoved = true;
        }
        if (newY != mCursorPosition.y) {
            mCursorPosition.y = newY;
            cursorMoved = true;
        }

        return cursorMoved;
    }
}

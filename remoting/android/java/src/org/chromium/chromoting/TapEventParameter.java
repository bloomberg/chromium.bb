// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

/**
 * {@link Event} parameter for tap events, represents both {@link pointerCount} and position of
 * the first touch point ({@link x} and {@link y}). {@link android.graphics.Point} and
 * {@link android.graphics.PointF} are both mutable, so this class uses two floats instead.
 */
public final class TapEventParameter {
    public final int pointerCount;
    public final float x;
    public final float y;

    public TapEventParameter(int pointerCount, float x, float y) {
        this.pointerCount = pointerCount;
        this.x = x;
        this.y = y;
    }
}

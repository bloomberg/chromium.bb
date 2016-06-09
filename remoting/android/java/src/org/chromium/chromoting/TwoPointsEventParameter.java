// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.view.MotionEvent;

/**
 * An {@link Event} parameter to represent two {@link MotionEvent}s with a {@link #dx} and a
 * {@link #dy} to indicate the distance between the points.
 *
 * {@link android.graphics.Point} and {@link android.graphics.PointF} are both mutable, so this
 * class uses two floats instead.
 */
public final class TwoPointsEventParameter {
    public final MotionEvent event1;
    public final MotionEvent event2;
    public final float dx;
    public final float dy;

    public TwoPointsEventParameter(MotionEvent event1,
                                   MotionEvent event2,
                                   float dx,
                                   float dy) {
        this.event1 = event1;
        this.event2 = event2;
        this.dx = dx;
        this.dy = dy;
    }
}

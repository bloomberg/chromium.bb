// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.Canvas;
import android.graphics.Point;

/** The parameter for an OnPaint event. */
public final class PaintEventParameter {
    public final Point cursorPosition;
    public final Canvas canvas;
    public final float scale;

    public PaintEventParameter(Point cursorPosition, Canvas canvas, float scale) {
        this.cursorPosition = cursorPosition;
        this.canvas = canvas;
        this.scale = scale;
    }
}

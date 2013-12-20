// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.os.SystemClock;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.InputDevice;
import android.view.MotionEvent;

import org.chromium.base.test.util.Feature;

/** Tests for {@link SwipePinchDetector}. */
public class SwipePinchDetectorTest extends InstrumentationTestCase {
    private SwipePinchDetector mDetector;
    private MotionEvent.PointerProperties[] mPointers;

    @Override
    public void setUp() {
        mDetector = new SwipePinchDetector(getInstrumentation().getTargetContext());
        MotionEvent.PointerProperties pointer0 = new MotionEvent.PointerProperties();
        pointer0.id = 0;
        MotionEvent.PointerProperties pointer1 = new MotionEvent.PointerProperties();
        pointer1.id = 1;
        mPointers = new MotionEvent.PointerProperties[] {pointer0, pointer1};
    }

    /** Verify that a simple swipe gesture is recognized as a swipe. */
    @SmallTest
    @Feature({"Chromoting"})
    public void testSwipeRecognition() throws Exception {
        final long eventTime = SystemClock.uptimeMillis();
        MotionEvent.PointerCoords p0 = new MotionEvent.PointerCoords();
        MotionEvent.PointerCoords p1 = new MotionEvent.PointerCoords();
        p1.x = 50;
        p1.y = 0;
        MotionEvent.PointerCoords[] pointerCoords = {p0, p1};
        MotionEvent event = MotionEvent.obtain(eventTime, eventTime,
                MotionEvent.ACTION_POINTER_DOWN, 2, mPointers, pointerCoords, 0, 0, 1, 1, 0, 0,
                InputDevice.SOURCE_TOUCHSCREEN , 0);
        mDetector.onTouchEvent(event);
        assertFalse(mDetector.isSwiping());
        assertFalse(mDetector.isPinching());

        // Any distance greater than the touch-slop threshold should work.
        p0.y += 100;
        p1.y += 100;

        event = MotionEvent.obtain(eventTime, eventTime, MotionEvent.ACTION_MOVE, 2, mPointers,
                pointerCoords, 0, 0, 1, 1, 0, 0, InputDevice.SOURCE_TOUCHSCREEN , 0);
        mDetector.onTouchEvent(event);
        assertTrue(mDetector.isSwiping());
        assertFalse(mDetector.isPinching());
    }
}

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.MotionEvent;
import android.view.ViewConfiguration;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;

/** Tests for {@link TapGestureDetector}. */
public class TapGestureDetectorTest extends InstrumentationTestCase {
    private static class MockListener implements TapGestureDetector.OnTapListener {
        int mTapCount = -1;
        int mLongPressCount = -1;

        @Override
        public boolean onTap(int pointerCount) {
            assertEquals(-1, mTapCount);
            mTapCount = pointerCount;
            return true;
        }

        @Override
        public void onLongPress(int pointerCount) {
            assertEquals(-1, mLongPressCount);
            mLongPressCount = pointerCount;
        }

        /** Resets the mock listener to its initial state. */
        public void reset() {
            mTapCount = -1;
            mLongPressCount = -1;
        }

        public void assertTapDetected(int expectedCount) {
            assertEquals(expectedCount, mTapCount);
            assertEquals(-1, mLongPressCount);
        }

        public void assertLongPressDetected(int expectedCount) {
            assertEquals(expectedCount, mLongPressCount);
            assertEquals(-1, mTapCount);
        }

        public void assertNothingDetected() {
            assertEquals(-1, mTapCount);
            assertEquals(-1, mLongPressCount);
        }
    }

    private TapGestureDetector mDetector;
    private MockListener mListener;
    private TouchEventGenerator mEventGenerator;

    /** Injects movement of a single finger (keeping other fingers in place). */
    private void injectMoveEvent(int id, float x, float y) {
        MotionEvent event = mEventGenerator.obtainMoveEvent(id, x, y);
        mDetector.onTouchEvent(event);
        event.recycle();
    }

    /** Injects a finger-down event (keeping other fingers in place). */
    private void injectDownEvent(int id, float x, float y) {
        MotionEvent event = mEventGenerator.obtainDownEvent(id, x, y);
        mDetector.onTouchEvent(event);
        event.recycle();
    }

    /** Injects a finger-up event (keeping other fingers in place). */
    private void injectUpEvent(int id) {
        MotionEvent event = mEventGenerator.obtainUpEvent(id);
        mDetector.onTouchEvent(event);
        event.recycle();
    }

    @Override
    public void setUp() {
        mListener = new MockListener();
        mDetector = new TapGestureDetector(getInstrumentation().getTargetContext(), mListener);
        mEventGenerator = new TouchEventGenerator();
    }

    /** Verifies that a simple down/up is detected as a tap. */
    @SmallTest
    @Feature({"Chromoting"})
    public void testOneFingerDownUp() throws Exception {
        injectDownEvent(0, 0, 0);
        injectUpEvent(0);
        mListener.assertTapDetected(1);
    }

    /** Verifies that a simple multi-finger down/up is detected as a tap. */
    @SmallTest
    @Feature({"Chromoting"})
    public void testMultipleFingerDownUp() throws Exception {
        injectDownEvent(0, 0, 0);
        injectDownEvent(1, 100, 100);
        injectDownEvent(2, 200, 200);
        injectUpEvent(0);
        injectUpEvent(1);
        injectUpEvent(2);
        mListener.assertTapDetected(3);
    }

    /** Verifies that a multi-finger tap is detected when lifting the fingers in reverse order. */
    @SmallTest
    @Feature({"Chromoting"})
    public void testMultipleFingerDownUpReversed() throws Exception {
        injectDownEvent(0, 0, 0);
        injectDownEvent(1, 100, 100);
        injectDownEvent(2, 200, 200);
        injectUpEvent(2);
        injectUpEvent(1);
        injectUpEvent(0);
        mListener.assertTapDetected(3);
    }

    /** Verifies that small movement of multiple fingers is still detected as a tap. */
    @SmallTest
    @Feature({"Chromoting"})
    public void testMultipleFingerSmallMovements() throws Exception {
        injectDownEvent(0, 0, 0);
        injectDownEvent(1, 100, 100);
        injectDownEvent(2, 200, 200);
        injectMoveEvent(0, 1, 1);
        injectMoveEvent(1, 101, 101);
        injectMoveEvent(2, 202, 202);
        injectUpEvent(0);
        injectUpEvent(1);
        injectUpEvent(2);
        mListener.assertTapDetected(3);
    }

    /** Verifies that large motion of a finger prevents a tap being detected. */
    @SmallTest
    @Feature({"Chromoting"})
    public void testLargeMotion() throws Exception {
        injectDownEvent(0, 0, 0);
        injectDownEvent(1, 100, 100);
        injectDownEvent(2, 200, 200);
        injectMoveEvent(1, 300, 300);
        injectUpEvent(0);
        injectUpEvent(1);
        injectUpEvent(2);
        mListener.assertNothingDetected();
    }

    /** Verifies that a long-press is detected. */
    @SmallTest
    @Feature({"Chromoting"})
    public void testLongPress() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            public void run() {
                // Ensure the gesture-detector is created on the UI thread, so that it uses the
                // Handler for the UI thread for LongPress notifications.
                mDetector = new TapGestureDetector(
                        getInstrumentation().getTargetContext(), mListener);

                injectDownEvent(0, 0, 0);
            }
        });

        Thread.sleep(2 * ViewConfiguration.getLongPressTimeout());

        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            public void run() {
                injectUpEvent(0);
            }
        });

        mListener.assertLongPressDetected(1);
    }
}

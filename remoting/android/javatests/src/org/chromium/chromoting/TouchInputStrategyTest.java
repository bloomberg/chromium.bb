// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import android.graphics.Point;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.view.MotionEvent;

import org.chromium.base.test.util.Feature;
import org.chromium.chromoting.jni.TouchEventData;

import java.util.LinkedList;
import java.util.Queue;

/** Tests for {@link TouchInputStrategy}. */
public class TouchInputStrategyTest extends InstrumentationTestCase {
    // Tests are run using a screen which is smaller than the size of the remote desktop and is
    // translated to the middle of the remote desktop area.  This allows us to verify that the
    // remote events which are 'injected' are correctly mapped and represent the remote coordinates.
    private static final int SCREEN_SIZE_PX = 100;
    private static final int REMOTE_DESKTOP_SIZE_PX = 300;
    private static final int TRANSLATE_OFFSET_PX = 100;

    private static class MouseData {
        MouseData(int xValue, int yValue, int buttonPressed, boolean buttonDown) {
            x = xValue;
            y = yValue;
            button = buttonPressed;
            isDown = buttonDown;
        }

        public int x;
        public int y;
        public int button;
        public boolean isDown;
    }

    private static class TouchData {
        TouchData(TouchEventData.EventType type, TouchEventData[] data) {
            eventType = type;
            eventData = data;
        }

        public TouchEventData.EventType eventType;
        public TouchEventData[] eventData;
    }

    private static class MockRemoteInputInjector implements TouchInputStrategy.RemoteInputInjector {
        private static final float COMPARISON_DELTA = 0.01f;

        private Queue<MouseData> mInjectedMouseEvents;
        private Queue<TouchData> mInjectedTouchEvents;

        MockRemoteInputInjector() {
            mInjectedMouseEvents = new LinkedList<MouseData>();
            mInjectedTouchEvents = new LinkedList<TouchData>();
        }

        @Override
        public void injectMouseEvent(int x, int y, int button, boolean buttonDown) {
            mInjectedMouseEvents.add(new MouseData(x, y, button, buttonDown));
        }

        @Override
        public void injectTouchEvent(TouchEventData.EventType eventType, TouchEventData[] data) {
            mInjectedTouchEvents.add(new TouchData(eventType, data));
        }

        public void assertTapInjected(int expectedX, int expectedY) {
            assertEquals(2, mInjectedTouchEvents.size());
            assertTrue(mInjectedMouseEvents.isEmpty());

            TouchData downTouchData = mInjectedTouchEvents.remove();
            assertEquals(TouchEventData.EventType.TOUCH_EVENT_START, downTouchData.eventType);
            assertEquals(1, downTouchData.eventData.length);
            assertEquals(expectedX, downTouchData.eventData[0].getTouchPointX(), COMPARISON_DELTA);
            assertEquals(expectedY, downTouchData.eventData[0].getTouchPointY(), COMPARISON_DELTA);

            TouchData upTouchData = mInjectedTouchEvents.remove();
            assertEquals(TouchEventData.EventType.TOUCH_EVENT_END, upTouchData.eventType);
            assertEquals(1, upTouchData.eventData.length);
            assertEquals(expectedX, upTouchData.eventData[0].getTouchPointX(), COMPARISON_DELTA);
            assertEquals(expectedY, upTouchData.eventData[0].getTouchPointY(), COMPARISON_DELTA);
        }

        public void assertRightClickInjected(int expectedX, int expectedY) {
            assertEquals(2, mInjectedMouseEvents.size());
            assertTrue(mInjectedTouchEvents.isEmpty());

            MouseData downMouseData = mInjectedMouseEvents.remove();
            assertEquals(expectedX, downMouseData.x);
            assertEquals(expectedY, downMouseData.y);
            assertEquals(TouchInputHandlerInterface.BUTTON_RIGHT, downMouseData.button);
            assertTrue(downMouseData.isDown);

            MouseData upMouseData = mInjectedMouseEvents.remove();
            assertEquals(expectedX, upMouseData.x);
            assertEquals(expectedY, upMouseData.y);
            assertEquals(TouchInputHandlerInterface.BUTTON_RIGHT, upMouseData.button);
            assertFalse(upMouseData.isDown);
        }

        public void assertTouchEventInjected(TouchEventData.EventType eventType) {
            // This method is called to verify the correct event type was added, no verification is
            // done for the position of each event.
            assertTouchEventInjected(eventType, -1, -1);
        }

        public void assertTouchEventInjected(
                TouchEventData.EventType eventType, int expectedX, int expectedY) {
            assertEquals(1, mInjectedTouchEvents.size());

            TouchData touchData = mInjectedTouchEvents.remove();
            assertEquals(eventType, touchData.eventType);
            if (expectedX >= 0) {
                assertEquals(expectedX, touchData.eventData[0].getTouchPointX(), COMPARISON_DELTA);
            }
            if (expectedY >= 0) {
                assertEquals(expectedY, touchData.eventData[0].getTouchPointY(), COMPARISON_DELTA);
            }
        }

        public void assertTouchMoveEventInjected(int fingerNum, int stepSizeX, int stepSizeY,
                int expectedMoveCount) {
            TouchData touchData;
            Point[] initialLocations = new Point[fingerNum];
            // Verify the correct number of START events were injected.
            for (int i = 0; i < fingerNum; i++) {
                touchData = mInjectedTouchEvents.remove();
                assertEquals(1, touchData.eventData.length);
                assertEquals(TouchEventData.EventType.TOUCH_EVENT_START, touchData.eventType);
                initialLocations[i] = new Point((int) touchData.eventData[0].getTouchPointX(),
                        (int) touchData.eventData[0].getTouchPointY());
            }

            // Verify the correct number of MOVE events were injected.
            for (int i = 0; i < expectedMoveCount; i++) {
                touchData = mInjectedTouchEvents.remove();
                assertEquals(fingerNum, touchData.eventData.length);
                assertEquals(TouchEventData.EventType.TOUCH_EVENT_MOVE, touchData.eventType);

                // These tests send a single event for each finger that is moved.  E.G. If we are
                // injecting a pan event with two fingers, then every two TouchEvents represents one
                // 'frame' of the motion sequence.  Here we determine which iteration this event
                // represents so we can use that to accurately compare the locations with the
                // expected values.
                int eventSequenceNum = i / fingerNum;
                for (int j = 0; j < fingerNum; j++) {
                    // We inject one finger at a time which means that finger will have the correct
                    // value but other fingers may still be stepSize behind it.  We add a little bit
                    // of slop here to ensure that the position values are moving towards the target
                    // value and to allow for simpler validation.
                    assertEquals(initialLocations[j].x + (stepSizeX * eventSequenceNum),
                            touchData.eventData[j].getTouchPointX(), stepSizeX + COMPARISON_DELTA);
                    assertEquals(initialLocations[j].y + (stepSizeY * eventSequenceNum),
                            touchData.eventData[j].getTouchPointY(), stepSizeY + COMPARISON_DELTA);
                }
            }
        }

        public void assertNothingInjected() {
            assertTrue(mInjectedMouseEvents.isEmpty());
            assertTrue(mInjectedTouchEvents.isEmpty());
        }
    }

    private RenderData mRenderData;
    private TouchInputStrategy mInputStrategy;
    private MockRemoteInputInjector mRemoteInputInjector;
    private TouchEventGenerator mEventGenerator;

    /** Injects movement of a single finger (keeping other fingers in place). */
    private void injectMoveEvent(int id, float x, float y) {
        MotionEvent event = mEventGenerator.obtainMoveEvent(id, x, y);
        mInputStrategy.onMotionEvent(event);
        event.recycle();
    }

    /** Injects a finger-down event (keeping other fingers in place). */
    private void injectDownEvent(int id, float x, float y) {
        MotionEvent event = mEventGenerator.obtainDownEvent(id, x, y);
        mInputStrategy.onMotionEvent(event);
        event.recycle();
    }

    /** Injects a finger-up event (keeping other fingers in place). */
    private void injectUpEvent(int id) {
        MotionEvent event = mEventGenerator.obtainUpEvent(id);
        mInputStrategy.onMotionEvent(event);
        event.recycle();
    }

    @Override
    public void setUp() {
        mRenderData = new RenderData();
        mRemoteInputInjector = new MockRemoteInputInjector();
        mInputStrategy = new TouchInputStrategy(mRenderData);
        mInputStrategy.setRemoteInputInjectorForTest(mRemoteInputInjector);
        mEventGenerator = new TouchEventGenerator();

        mRenderData.screenWidth = SCREEN_SIZE_PX;
        mRenderData.screenHeight = SCREEN_SIZE_PX;
        mRenderData.imageWidth = REMOTE_DESKTOP_SIZE_PX;
        mRenderData.imageHeight = REMOTE_DESKTOP_SIZE_PX;
        mRenderData.transform.postTranslate(-TRANSLATE_OFFSET_PX, -TRANSLATE_OFFSET_PX);
    }

    @SmallTest
    @Feature({"Chromoting"})
    public void testOnTapWithNoEvents() throws Exception {
        assertFalse(mInputStrategy.onTap(TouchInputHandlerInterface.BUTTON_LEFT));
        mRemoteInputInjector.assertNothingInjected();
    }

    @SmallTest
    @Feature({"Chromoting"})
    public void testOneFingerTap() throws Exception {
        injectDownEvent(0, 0, 0);
        injectUpEvent(0);
        mRemoteInputInjector.assertNothingInjected();

        assertTrue(mInputStrategy.onTap(TouchInputHandlerInterface.BUTTON_LEFT));

        mRemoteInputInjector.assertTapInjected(TRANSLATE_OFFSET_PX, TRANSLATE_OFFSET_PX);
    }

    @SmallTest
    @Feature({"Chromoting"})
    public void testLifoTwoFingerTap() throws Exception {
        // Verify that the right click coordinates occur at the point of the first tap when the
        // initial finger is lifted up last.
        injectDownEvent(0, 0, 0);
        injectDownEvent(1, 25, 25);
        injectUpEvent(1);
        injectUpEvent(0);
        mRemoteInputInjector.assertNothingInjected();

        assertTrue(mInputStrategy.onTap(TouchInputHandlerInterface.BUTTON_RIGHT));

        mRemoteInputInjector.assertRightClickInjected(TRANSLATE_OFFSET_PX, TRANSLATE_OFFSET_PX);
    }

    @SmallTest
    @Feature({"Chromoting"})
    public void testFifoTwoFingerTap() throws Exception {
        // Verify that the right click coordinates occur at the point of the first tap when the
        // initial finger is lifted up first.
        injectDownEvent(0, 0, 0);
        injectDownEvent(1, 25, 25);
        injectUpEvent(0);
        injectUpEvent(1);
        mRemoteInputInjector.assertNothingInjected();

        assertTrue(mInputStrategy.onTap(TouchInputHandlerInterface.BUTTON_RIGHT));

        mRemoteInputInjector.assertRightClickInjected(TRANSLATE_OFFSET_PX, TRANSLATE_OFFSET_PX);
    }

    @SmallTest
    @Feature({"Chromoting"})
    public void testThreeFingerTap() throws Exception {
        injectDownEvent(0, 0, 0);
        injectDownEvent(1, 25, 25);
        injectDownEvent(2, 50, 50);
        injectUpEvent(2);
        injectUpEvent(1);
        injectUpEvent(0);
        mRemoteInputInjector.assertNothingInjected();

        assertFalse(mInputStrategy.onTap(TouchInputHandlerInterface.BUTTON_MIDDLE));
        mRemoteInputInjector.assertNothingInjected();
    }

    @SmallTest
    @Feature({"Chromoting"})
    public void testOneFingerTapSequence() throws Exception {
        int tapSequenceCount = 10;
        for (int i = 0; i < tapSequenceCount; i++) {
            injectDownEvent(0, i, i);
            injectUpEvent(0);
            mRemoteInputInjector.assertNothingInjected();

            assertTrue(mInputStrategy.onTap(TouchInputHandlerInterface.BUTTON_LEFT));

            int remoteOffsetPx = TRANSLATE_OFFSET_PX + i;
            mRemoteInputInjector.assertTapInjected(remoteOffsetPx, remoteOffsetPx);
        }
    }

    @SmallTest
    @Feature({"Chromoting"})
    public void testInvalidThenValidTap() throws Exception {
        // First an invalid tap, verify it is ignored.
        injectDownEvent(0, 0, 0);
        injectDownEvent(1, 25, 25);
        injectDownEvent(2, 50, 50);
        injectUpEvent(2);
        injectUpEvent(1);
        injectUpEvent(0);
        mRemoteInputInjector.assertNothingInjected();

        assertFalse(mInputStrategy.onTap(TouchInputHandlerInterface.BUTTON_MIDDLE));
        mRemoteInputInjector.assertNothingInjected();

        // Next a valid tap, verify it is handled.
        injectDownEvent(0, 0, 0);
        injectUpEvent(0);
        mRemoteInputInjector.assertNothingInjected();

        assertTrue(mInputStrategy.onTap(TouchInputHandlerInterface.BUTTON_LEFT));

        mRemoteInputInjector.assertTapInjected(TRANSLATE_OFFSET_PX, TRANSLATE_OFFSET_PX);
    }

    @SmallTest
    @Feature({"Chromoting"})
    public void testOnPressAndHoldWithNoEvents() throws Exception {
        assertFalse(mInputStrategy.onPressAndHold(TouchInputHandlerInterface.BUTTON_LEFT));
        mRemoteInputInjector.assertNothingInjected();
    }

    @SmallTest
    @Feature({"Chromoting"})
    public void testOneFingerLongPress() throws Exception {
        injectDownEvent(0, 0, 0);
        mRemoteInputInjector.assertNothingInjected();

        assertTrue(mInputStrategy.onPressAndHold(TouchInputHandlerInterface.BUTTON_LEFT));
        mRemoteInputInjector.assertTouchEventInjected(TouchEventData.EventType.TOUCH_EVENT_START,
                TRANSLATE_OFFSET_PX, TRANSLATE_OFFSET_PX);

        injectUpEvent(0);
        mRemoteInputInjector.assertTouchEventInjected(
                TouchEventData.EventType.TOUCH_EVENT_END, TRANSLATE_OFFSET_PX, TRANSLATE_OFFSET_PX);
    }

    @SmallTest
    @Feature({"Chromoting"})
    public void testOneFingerLongPressThenPan() throws Exception {
        injectDownEvent(0, 0, 0);
        mRemoteInputInjector.assertNothingInjected();

        assertTrue(mInputStrategy.onPressAndHold(TouchInputHandlerInterface.BUTTON_LEFT));
        mRemoteInputInjector.assertTouchEventInjected(TouchEventData.EventType.TOUCH_EVENT_START,
                TRANSLATE_OFFSET_PX, TRANSLATE_OFFSET_PX);

        int panEventCount = 50;
        for (int i = 0; i <= 50; i++) {
            injectMoveEvent(0, 0, i);
            mRemoteInputInjector.assertTouchEventInjected(
                    TouchEventData.EventType.TOUCH_EVENT_MOVE);
        }

        injectUpEvent(0);
        mRemoteInputInjector.assertTouchEventInjected(TouchEventData.EventType.TOUCH_EVENT_END,
                TRANSLATE_OFFSET_PX, TRANSLATE_OFFSET_PX + panEventCount);
    }

    @SmallTest
    @Feature({"Chromoting"})
    public void testTwoFingerLongPress() throws Exception {
        injectDownEvent(0, 0, 0);
        injectDownEvent(1, 1, 1);
        mRemoteInputInjector.assertNothingInjected();

        assertFalse(mInputStrategy.onPressAndHold(TouchInputHandlerInterface.BUTTON_RIGHT));
        mRemoteInputInjector.assertNothingInjected();

        injectUpEvent(0);
        injectUpEvent(1);
        mRemoteInputInjector.assertNothingInjected();
    }

    @SmallTest
    @Feature({"Chromoting"})
    public void testOneFingerPan() throws Exception {
        injectDownEvent(0, 0, 0);

        // Inject a few move events to simulate a pan.
        injectMoveEvent(0, 1, 1);
        injectMoveEvent(0, 2, 2);
        injectMoveEvent(0, 3, 3);
        mRemoteInputInjector.assertNothingInjected();

        injectUpEvent(0);
        mRemoteInputInjector.assertNothingInjected();
    }

    @SmallTest
    @Feature({"Chromoting"})
    public void testVerticalTwoFingerPan() throws Exception {
        final int fingerOnePosX = 0;
        final int fingerTwoPosX = 10;
        injectDownEvent(0, fingerOnePosX, 0);
        injectDownEvent(1, fingerTwoPosX, 0);

        final int eventNum = 10;
        for (int i = 0; i < eventNum; i++) {
            injectMoveEvent(0, fingerOnePosX, i);
            injectMoveEvent(1, fingerTwoPosX, i);
        }
        mRemoteInputInjector.assertNothingInjected();

        mInputStrategy.onScroll(0.0f, 0.0f);
        mRemoteInputInjector.assertTouchMoveEventInjected(2, 0, 1, eventNum * 2);

        // Verify events are sent in realtime now.
        for (int i = eventNum; i < eventNum + 5; i++) {
            injectMoveEvent(0, fingerOnePosX, i);
            mRemoteInputInjector.assertTouchEventInjected(
                    TouchEventData.EventType.TOUCH_EVENT_MOVE);

            injectMoveEvent(1, fingerTwoPosX, i);
            mRemoteInputInjector.assertTouchEventInjected(
                    TouchEventData.EventType.TOUCH_EVENT_MOVE);
        }

        injectUpEvent(0);
        mRemoteInputInjector.assertTouchEventInjected(
                TouchEventData.EventType.TOUCH_EVENT_END);

        injectUpEvent(1);
        mRemoteInputInjector.assertTouchEventInjected(
                TouchEventData.EventType.TOUCH_EVENT_END);
    }

    @SmallTest
    @Feature({"Chromoting"})
    public void testHorizontalTwoFingerPan() throws Exception {
        final int fingerOnePosY = 0;
        final int fingerTwoPosY = 10;
        injectDownEvent(0, 0, fingerOnePosY);
        injectDownEvent(1, 0, fingerTwoPosY);

        final int eventNum = 10;
        for (int i = 0; i < eventNum; i++) {
            injectMoveEvent(0, i, fingerOnePosY);
            injectMoveEvent(1, i, fingerTwoPosY);
        }
        mRemoteInputInjector.assertNothingInjected();

        mInputStrategy.onScroll(0.0f, 0.0f);
        mRemoteInputInjector.assertTouchMoveEventInjected(2, 1, 0, eventNum * 2);

        // Verify events are sent in realtime now.
        for (int i = eventNum; i < eventNum + 5; i++) {
            injectMoveEvent(0, i, fingerOnePosY);
            mRemoteInputInjector.assertTouchEventInjected(
                    TouchEventData.EventType.TOUCH_EVENT_MOVE);

            injectMoveEvent(1, i, fingerTwoPosY);
            mRemoteInputInjector.assertTouchEventInjected(
                    TouchEventData.EventType.TOUCH_EVENT_MOVE);
        }

        injectUpEvent(0);
        mRemoteInputInjector.assertTouchEventInjected(
                TouchEventData.EventType.TOUCH_EVENT_END);

        injectUpEvent(1);
        mRemoteInputInjector.assertTouchEventInjected(
                TouchEventData.EventType.TOUCH_EVENT_END);
    }

    @SmallTest
    @Feature({"Chromoting"})
    public void testCancelledTwoFingerPan() throws Exception {
        final int fingerOnePosX = 0;
        final int fingerTwoPosX = 10;
        injectDownEvent(0, fingerOnePosX, 0);
        injectDownEvent(1, fingerTwoPosX, 0);

        final int eventNum = 10;
        for (int i = 0; i < eventNum; i++) {
            injectMoveEvent(0, fingerOnePosX, i);
            injectMoveEvent(1, fingerTwoPosX, i);
        }
        mRemoteInputInjector.assertNothingInjected();

        mInputStrategy.onScroll(0.0f, 0.0f);
        mRemoteInputInjector.assertTouchMoveEventInjected(2, 0, 1, eventNum * 2);

        // Verify events are sent in realtime now.
        for (int i = eventNum; i < eventNum + 5; i++) {
            injectMoveEvent(0, fingerOnePosX, i);
            mRemoteInputInjector.assertTouchEventInjected(
                    TouchEventData.EventType.TOUCH_EVENT_MOVE);

            injectMoveEvent(1, fingerTwoPosX, i);
            mRemoteInputInjector.assertTouchEventInjected(
                    TouchEventData.EventType.TOUCH_EVENT_MOVE);
        }

        // Once a third finger goes down, no more events should be sent.
        injectDownEvent(2, 0, 0);
        mRemoteInputInjector.assertNothingInjected();

        injectMoveEvent(0, 0, 0);
        injectMoveEvent(1, 0, 0);
        injectMoveEvent(2, 0, 0);
        mRemoteInputInjector.assertNothingInjected();

        injectUpEvent(2);
        mRemoteInputInjector.assertNothingInjected();

        injectMoveEvent(0, 5, 5);
        injectMoveEvent(1, 5, 5);
        mRemoteInputInjector.assertNothingInjected();
    }

    @SmallTest
    @Feature({"Chromoting"})
    public void testTooManyEventsCancelsGesture() throws Exception {
        final int fingerOnePosX = 0;
        final int fingerTwoPosX = 10;
        injectDownEvent(0, fingerOnePosX, 0);
        injectDownEvent(1, fingerTwoPosX, 0);

        for (int i = 0; i < 10000; i++) {
            injectMoveEvent(0, fingerOnePosX, i % 10);
            injectMoveEvent(1, fingerTwoPosX, i % 10);
        }
        mRemoteInputInjector.assertNothingInjected();

        mInputStrategy.onScroll(0.0f, 0.0f);
        mRemoteInputInjector.assertNothingInjected();
    }
}

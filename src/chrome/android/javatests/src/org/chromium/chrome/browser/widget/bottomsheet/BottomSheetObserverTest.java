// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.flags.ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.MathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;

/** This class tests the functionality of the {@link BottomSheetObserver}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE) // Bottom sheet is only used on phones.
@CommandLineFlags.Add({DISABLE_FIRST_RUN_EXPERIENCE})
public class BottomSheetObserverTest {
    /** An observer used to record events that occur with respect to the bottom sheet. */
    public static class TestSheetObserver extends EmptyBottomSheetObserver {
        /** A {@link CallbackHelper} that can wait for the bottom sheet to be closed. */
        public final CallbackHelper mClosedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the bottom sheet to be opened. */
        public final CallbackHelper mOpenedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the onOffsetChanged event. */
        public final CallbackHelper mOffsetChangedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the onSheetContentChanged event. */
        public final CallbackHelper mContentChangedCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the sheet to be in its full state. */
        public final CallbackHelper mFullCallbackHelper = new CallbackHelper();

        /** A {@link CallbackHelper} that can wait for the sheet to be hidden. */
        public final CallbackHelper mHiddenCallbackHelper = new CallbackHelper();

        /** The last value that the onOffsetChanged event sent. */
        private float mLastOffsetChangedValue;

        @Override
        public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
            mLastOffsetChangedValue = heightFraction;
            mOffsetChangedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetOpened(@StateChangeReason int reason) {
            mOpenedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetClosed(@StateChangeReason int reason) {
            mClosedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetContentChanged(BottomSheetContent newContent) {
            mContentChangedCallbackHelper.notifyCalled();
        }

        @Override
        public void onSheetStateChanged(int newState) {
            if (newState == BottomSheetController.SheetState.HIDDEN) {
                mHiddenCallbackHelper.notifyCalled();
            } else if (newState == BottomSheetController.SheetState.FULL) {
                mFullCallbackHelper.notifyCalled();
            }
        }

        /** @return The last value passed in to {@link #onSheetOffsetChanged(float)}. */
        public float getLastOffsetChangedValue() {
            return mLastOffsetChangedValue;
        }
    }

    @Rule
    public ChromeTabbedActivityTestRule mTestRule = new ChromeTabbedActivityTestRule();
    private TestSheetObserver mObserver;
    private TestBottomSheetContent mSheetContent;
    private BottomSheetController mBottomSheetController;
    private BottomSheetTestSupport mTestSupport;

    @Before
    public void setUp() throws Exception {
        BottomSheetTestSupport.setSmallScreen(false);
        mTestRule.startMainActivityOnBlankPage();
        mBottomSheetController =
                mTestRule.getActivity().getRootUiCoordinatorForTesting().getBottomSheetController();
        mTestSupport = new BottomSheetTestSupport(mBottomSheetController);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mSheetContent = new TestBottomSheetContent(
                    mTestRule.getActivity(), BottomSheetContent.ContentPriority.HIGH, false);
            mBottomSheetController.requestShowContent(mSheetContent, false);
        });
        mObserver = new TestSheetObserver();
        mBottomSheetController.addObserver(mObserver);
    }

    /** Test that the onSheetClosed event is triggered if the sheet is closed without animation. */
    @Test
    @MediumTest
    public void testCloseEventCalled_noAnimation() throws TimeoutException {
        runCloseEventTest(false, true);
    }

    /**
     * Test that the onSheetClosed event is triggered if the sheet is closed without animation and
     * without a peeking state.
     */
    @Test
    @MediumTest
    public void testCloseEventCalled_noAnimationNoPeekState() throws TimeoutException {
        runCloseEventTest(false, false);
    }

    /** Test that the onSheetClosed event is triggered if the sheet is closed with animation. */
    @Test
    @MediumTest
    public void testCloseEventCalled_withAnimation() throws TimeoutException {
        runCloseEventTest(true, true);
    }

    /**
     * Test that the onSheetClosed event is triggered if the sheet is closed with animation but
     * without a peeking state.
     */
    @Test
    @MediumTest
    public void testCloseEventCalled_withAnimationNoPeekState() throws TimeoutException {
        runCloseEventTest(true, false);
    }

    /**
     * Run different versions of the onSheetClosed event test.
     * @param animationEnabled Whether to run the test with animation.
     * @param peekStateEnabled Whether the sheet's content has a peek state.
     */
    private void runCloseEventTest(boolean animationEnabled, boolean peekStateEnabled)
            throws TimeoutException {
        CallbackHelper hiddenHelper = mObserver.mHiddenCallbackHelper;
        int initialHideEvents = hiddenHelper.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetState(BottomSheetController.SheetState.FULL, false));

        mSheetContent.setPeekHeight(peekStateEnabled ? BottomSheetContent.HeightMode.DEFAULT
                                                     : BottomSheetContent.HeightMode.DISABLED);

        CallbackHelper closedCallbackHelper = mObserver.mClosedCallbackHelper;

        int initialOpenedCount = mObserver.mOpenedCallbackHelper.getCallCount();

        int closedCallbackCount = closedCallbackHelper.getCallCount();

        int targetState = peekStateEnabled ? BottomSheetController.SheetState.PEEK
                                           : BottomSheetController.SheetState.HIDDEN;
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetState(targetState, animationEnabled));

        closedCallbackHelper.waitForCallback(closedCallbackCount, 1);

        if (targetState == BottomSheetController.SheetState.HIDDEN) {
            hiddenHelper.waitForCallback(initialHideEvents, 1);
        }

        assertEquals(initialOpenedCount, mObserver.mOpenedCallbackHelper.getCallCount());
        assertEquals("Close event should have only been called once.",
                closedCallbackCount + 1, closedCallbackHelper.getCallCount());
    }

    /** Test that the onSheetOpened event is triggered if the sheet is opened without animation. */
    @Test
    @MediumTest
    public void testOpenedEventCalled_noAnimation() throws TimeoutException {
        runOpenEventTest(false, true);
    }

    /**
     * Test that the onSheetOpened event is triggered if the sheet is opened without animation and
     * without a peeking state.
     */
    @Test
    @MediumTest
    public void testOpenedEventCalled_noAnimationNoPeekState() throws TimeoutException {
        runOpenEventTest(false, false);
    }

    /** Test that the onSheetOpened event is triggered if the sheet is opened with animation. */
    @Test
    @MediumTest
    public void testOpenedEventCalled_withAnimation() throws TimeoutException {
        runOpenEventTest(true, true);
    }

    /**
     * Test that the onSheetOpened event is triggered if the sheet is opened with animation and
     * without a peek state.
     */
    @Test
    @MediumTest
    public void testOpenedEventCalled_withAnimationNoPeekState() throws TimeoutException {
        runOpenEventTest(true, false);
    }

    /**
     * Run different versions of the onSheetOpened event test.
     * @param animationEnabled Whether to run the test with animation.
     * @param peekStateEnabled Whether the sheet's content has a peek state.
     */
    private void runOpenEventTest(boolean animationEnabled, boolean peekStateEnabled)
            throws TimeoutException {
        mSheetContent.setPeekHeight(peekStateEnabled ? BottomSheetContent.HeightMode.DEFAULT
                                                     : BottomSheetContent.HeightMode.DISABLED);

        CallbackHelper fullCallbackHelper = mObserver.mFullCallbackHelper;
        int initialFullCount = fullCallbackHelper.getCallCount();
        CallbackHelper openedCallbackHelper = mObserver.mOpenedCallbackHelper;
        int openedCallbackCount = openedCallbackHelper.getCallCount();
        CallbackHelper closedCallbackHelper = mObserver.mClosedCallbackHelper;
        int initialClosedCount = closedCallbackHelper.getCallCount();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetState(mTestSupport.getOpeningState(), false));

        assertNotEquals("Sheet should not be hidden.", mBottomSheetController.getSheetState(),
                BottomSheetController.SheetState.HIDDEN);
        if (!peekStateEnabled) {
            assertNotEquals("Sheet should be above the peeking state when peek is disabled.",
                    mBottomSheetController.getSheetState(), BottomSheetController.SheetState.PEEK);
        }

        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mTestSupport.setSheetState(
                                BottomSheetController.SheetState.FULL, animationEnabled));

        openedCallbackHelper.waitForCallback(openedCallbackCount, 1);
        fullCallbackHelper.waitForCallback(initialFullCount, 1);

        assertEquals("Open event should have only been called once.",
                openedCallbackCount + 1, openedCallbackHelper.getCallCount());

        assertEquals(initialClosedCount, closedCallbackHelper.getCallCount());
    }

    /**
     * Test the onOffsetChanged event.
     */
    @Test
    @MediumTest
    public void testOffsetChangedEvent() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetState(BottomSheetController.SheetState.FULL, false));
        CallbackHelper callbackHelper = mObserver.mOffsetChangedCallbackHelper;

        float hiddenHeight =
                mTestSupport.getHiddenRatio() * mBottomSheetController.getContainerHeight();
        float fullHeight =
                mTestSupport.getFullRatio() * mBottomSheetController.getContainerHeight();

        // The sheet's half state is not necessarily 50% of the way to the top.
        float midPeekFull = (hiddenHeight + fullHeight) / 2f;

        // When in the hidden state, the transition value should be 0.
        int callbackCount = callbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetOffsetFromBottom(hiddenHeight, StateChangeReason.NONE));
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(0f, mObserver.getLastOffsetChangedValue(), MathUtils.EPSILON);

        // When in the full state, the transition value should be 1.
        callbackCount = callbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetOffsetFromBottom(fullHeight, StateChangeReason.NONE));
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(1f, mObserver.getLastOffsetChangedValue(), MathUtils.EPSILON);

        // Halfway between peek and full should send 0.5.
        callbackCount = callbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetOffsetFromBottom(midPeekFull, StateChangeReason.NONE));
        callbackHelper.waitForCallback(callbackCount, 1);
        assertEquals(0.5f, mObserver.getLastOffsetChangedValue(), MathUtils.EPSILON);
    }

    @Test
    @MediumTest
    public void testWrapContentBehavior() throws TimeoutException {
        // We make sure the height of the wrapped content is smaller than sheetContainerHeight.
        int wrappedContentHeight = (int) mBottomSheetController.getContainerHeight() / 2;
        assertTrue(wrappedContentHeight > 0);

        // Show content that should be wrapped.
        CallbackHelper callbackHelper = mObserver.mContentChangedCallbackHelper;
        int callCount = callbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mTestSupport.showContent(
                    new TestBottomSheetContent(mTestRule.getActivity(),
                            BottomSheetContent.ContentPriority.HIGH, false) {
                        private final ViewGroup mContentView;

                        {
                            // We wrap the View in a FrameLayout as we need something to read the
                            // hard coded height in the layout params. There is no way to create a
                            // View with a specific height on its own as View::onMeasure will by
                            // default set its height/width to be the minimum height/width of its
                            // background (if any) or expand as much as it can.
                            mContentView = new FrameLayout(mTestRule.getActivity());
                            View child = new View(mTestRule.getActivity());
                            child.setLayoutParams(new ViewGroup.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT, wrappedContentHeight));
                            mContentView.addView(child);
                        }

                        @Override
                        public View getContentView() {
                            return mContentView;
                        }

                        @Override
                        public float getFullHeightRatio() {
                            return HeightMode.WRAP_CONTENT;
                        }
                    });
        });
        callbackHelper.waitForCallback(callCount);

        // HALF state is forbidden when wrapping the content.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTestSupport.setSheetState(BottomSheetController.SheetState.HALF, false));
        assertEquals(BottomSheetController.SheetState.FULL, mBottomSheetController.getSheetState());

        // Check the offset.
        assertEquals(wrappedContentHeight + mTestSupport.getToolbarShadowHeight(),
                mBottomSheetController.getCurrentOffset(), MathUtils.EPSILON);
    }
}

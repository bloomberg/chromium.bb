// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.scrim;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

import java.util.concurrent.TimeoutException;

/** This class tests the behavior of the scrim component. */
@RunWith(BaseJUnit4ClassRunner.class)
public class ScrimTest extends DummyUiActivityTestCase {
    private ScrimCoordinator mScrimCoordinator;
    private FrameLayout mParent;
    private View mAnchorView;

    private final CallbackHelper mStatusBarCallbackHelper = new CallbackHelper();
    private final ScrimCoordinator.StatusBarScrimDelegate mScrimDelegate =
            scrimFraction -> mStatusBarCallbackHelper.notifyCalled();

    private final CallbackHelper mScrimClickCallbackHelper = new CallbackHelper();
    private final CallbackHelper mVisibilityChangeCallbackHelper = new CallbackHelper();
    private final Runnable mClickDelegate = () -> mScrimClickCallbackHelper.notifyCalled();
    private final Callback<Boolean> mVisibilityChangeCallback =
            (v) -> mVisibilityChangeCallbackHelper.notifyCalled();

    @Before
    public void setUp() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mParent = new FrameLayout(getActivity());
            getActivity().setContentView(mParent);
            mParent.getLayoutParams().width = ViewGroup.LayoutParams.MATCH_PARENT;
            mParent.getLayoutParams().height = ViewGroup.LayoutParams.MATCH_PARENT;

            mAnchorView = new View(getActivity());
            mParent.addView(mAnchorView);

            mScrimCoordinator =
                    new ScrimCoordinator(getActivity(), mScrimDelegate, mParent, Color.RED);
        });
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testVisibility() throws TimeoutException {
        showScrim(buildModel(true, false, true, Color.RED), false);

        assertEquals("Scrim should be completely visible.", 1.0f,
                mScrimCoordinator.getViewForTesting().getAlpha(), MathUtils.EPSILON);

        int callCount = mVisibilityChangeCallbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> mScrimCoordinator.hideScrim(false));
        mVisibilityChangeCallbackHelper.waitForCallback(callCount, 1);
        assertScrimVisibility(false);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testColor_default() throws TimeoutException {
        showScrim(buildModel(true, false, true, Color.RED), false);

        assertScrimColor(Color.RED);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testColor_custom() throws TimeoutException {
        showScrim(buildModel(false, false, true, Color.GREEN), false);

        assertScrimColor(Color.GREEN);

        ThreadUtils.runOnUiThreadBlocking(() -> mScrimCoordinator.hideScrim(false));

        CriteriaHelper.pollUiThread(()
                                            -> mScrimCoordinator.getViewForTesting() == null,
                "Scrim should be null after being hidden.", CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testHierarchy_behindAnchor() throws TimeoutException {
        showScrim(buildModel(true, false, false, Color.RED), false);

        View scrimView = mScrimCoordinator.getViewForTesting();
        assertEquals("The parent view of the scrim is incorrect.", mParent, scrimView.getParent());
        assertTrue("The scrim should be positioned behind the anchor.",
                mParent.indexOfChild(scrimView) < mParent.indexOfChild(mAnchorView));
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testHierarchy_inFrontOfAnchor() throws TimeoutException {
        showScrim(buildModel(true, false, true, Color.RED), false);

        View scrimView = mScrimCoordinator.getViewForTesting();
        assertEquals("The parent view of the scrim is incorrect.", mParent, scrimView.getParent());
        assertTrue("The scrim should be positioned in front of the anchor.",
                mParent.indexOfChild(scrimView) > mParent.indexOfChild(mAnchorView));
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testObserver_clickEvent() throws TimeoutException {
        showScrim(buildModel(true, false, true, Color.RED), false);

        int callCount = mScrimClickCallbackHelper.getCallCount();
        ScrimMediator mediator = mScrimCoordinator.getMediatorForTesting();
        ScrimView scrimView = mScrimCoordinator.getViewForTesting();
        ThreadUtils.runOnUiThreadBlocking(() -> mediator.onClick(scrimView));
        mScrimClickCallbackHelper.waitForCallback(callCount, 1);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testAnimation_running() throws TimeoutException {
        // The showScrim method includes checks for animation state.
        showScrim(buildModel(true, false, true, Color.RED), true);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testAnimation_canceled() throws TimeoutException {
        showScrim(buildModel(true, false, true, Color.RED), true);

        ThreadUtils.runOnUiThreadBlocking(() -> mScrimCoordinator.setAlpha(0.5f));

        assertFalse("Animations should not be running.", mScrimCoordinator.areAnimationsRunning());
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testAffectsStatusBar_enabled() throws TimeoutException {
        int callCount = mStatusBarCallbackHelper.getCallCount();
        showScrim(buildModel(true, true, true, Color.RED), false);
        mStatusBarCallbackHelper.waitForCallback(callCount, 1);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testAffectsStatusBar_disabled() throws TimeoutException {
        int callCount = mStatusBarCallbackHelper.getCallCount();
        showScrim(buildModel(true, false, true, Color.RED), false);

        ThreadUtils.runOnUiThreadBlocking(() -> mScrimCoordinator.setAlpha(0.5f));

        assertEquals("Scrim alpha should be 0.5f.", 0.5f,
                mScrimCoordinator.getViewForTesting().getAlpha(), MathUtils.EPSILON);

        assertEquals("No events to the status bar delegate should have occurred", callCount,
                mStatusBarCallbackHelper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testCustomDrawable() throws TimeoutException {
        ColorDrawable customDrawable = new ColorDrawable(Color.BLUE);
        PropertyModel model =
                new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                        .with(ScrimProperties.TOP_MARGIN, 0)
                        .with(ScrimProperties.AFFECTS_STATUS_BAR, false)
                        .with(ScrimProperties.ANCHOR_VIEW, mAnchorView)
                        .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, false)
                        .with(ScrimProperties.CLICK_DELEGATE, mClickDelegate)
                        .with(ScrimProperties.VISIBILITY_CALLBACK, mVisibilityChangeCallback)
                        .with(ScrimProperties.BACKGROUND_COLOR, Color.RED)
                        .with(ScrimProperties.BACKGROUND_DRAWABLE, customDrawable)
                        .with(ScrimProperties.GESTURE_DETECTOR, null)
                        .build();

        showScrim(model, false);

        assertEquals("Scrim should be using a custom background.", customDrawable,
                mScrimCoordinator.getViewForTesting().getBackground());

        ThreadUtils.runOnUiThreadBlocking(() -> mScrimCoordinator.hideScrim(false));

        CriteriaHelper.pollUiThread(()
                                            -> mScrimCoordinator.getViewForTesting() == null,
                "Scrim should be null after being hidden.", CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testTopMargin() throws TimeoutException {
        int topMargin = 100;
        PropertyModel model =
                new PropertyModel.Builder(ScrimProperties.REQUIRED_KEYS)
                        .with(ScrimProperties.TOP_MARGIN, topMargin)
                        .with(ScrimProperties.AFFECTS_STATUS_BAR, false)
                        .with(ScrimProperties.ANCHOR_VIEW, mAnchorView)
                        .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, false)
                        .with(ScrimProperties.CLICK_DELEGATE, mClickDelegate)
                        .with(ScrimProperties.VISIBILITY_CALLBACK, mVisibilityChangeCallback)
                        .build();

        showScrim(model, false);

        View scrimView = mScrimCoordinator.getViewForTesting();
        assertEquals("Scrim top margin is incorrect.", topMargin,
                ((ViewGroup.MarginLayoutParams) scrimView.getLayoutParams()).topMargin);
    }

    @Test
    @SmallTest
    @Feature({"Scrim"})
    public void testOldScrimHidden() throws TimeoutException {
        showScrim(buildModel(true, false, true, Color.RED), false);

        assertScrimVisibility(true);

        View oldScrim = mScrimCoordinator.getViewForTesting();

        showScrim(buildModel(true, false, true, Color.BLUE), false);

        View newScrim = mScrimCoordinator.getViewForTesting();

        assertNotEquals("The view should have changed.", oldScrim, newScrim);
        assertEquals("The old scrim should be gone.", View.GONE, oldScrim.getVisibility());
    }

    /**
     * Build a model to show the scrim with.
     * @param requiredKeysOnly Whether the model should be built with only the required keys.
     * @param affectsStatusBar Whether the status bar should be affected by the scrim.
     * @param showInFrontOfAnchor Whether the scrim shows in front of the anchor view.
     * @param color The color to use for the overlay. If only using required keys, this value is
     *              ignored.
     * @return A model to pass to the scrim coordinator.
     */
    private PropertyModel buildModel(boolean requiredKeysOnly, boolean affectsStatusBar,
            boolean showInFrontOfAnchor, @ColorInt int color) {
        PropertyModel model =
                new PropertyModel
                        .Builder(requiredKeysOnly ? ScrimProperties.REQUIRED_KEYS
                                                  : ScrimProperties.ALL_KEYS)
                        .with(ScrimProperties.TOP_MARGIN, 0)
                        .with(ScrimProperties.AFFECTS_STATUS_BAR, affectsStatusBar)
                        .with(ScrimProperties.ANCHOR_VIEW, mAnchorView)
                        .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, showInFrontOfAnchor)
                        .with(ScrimProperties.CLICK_DELEGATE, mClickDelegate)
                        .with(ScrimProperties.VISIBILITY_CALLBACK, mVisibilityChangeCallback)
                        .build();

        if (!requiredKeysOnly) {
            model.set(ScrimProperties.BACKGROUND_COLOR, color);
            model.set(ScrimProperties.BACKGROUND_DRAWABLE, null);
            model.set(ScrimProperties.GESTURE_DETECTOR, null);
        }

        return model;
    }

    /**
     * Show the scrim and wait for a visibility change.
     * @param model The model to show the scrim with.
     * @param animate Whether the scrim should animate. If false, alpha is immediately set to 100%.
     */
    private void showScrim(PropertyModel model, boolean animate) throws TimeoutException {
        int callCount = mVisibilityChangeCallbackHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mScrimCoordinator.showScrim(model);

            // Animations are disabled for these types of tests, so just make sure the animation was
            // created then continue as if we weren't running animation.
            if (animate) {
                assertNotNull(
                        "Animations should be running.", mScrimCoordinator.areAnimationsRunning());
            }

            mScrimCoordinator.setAlpha(1.0f);
        });

        mVisibilityChangeCallbackHelper.waitForCallback(callCount, 1);
        assertScrimVisibility(true);
    }

    /** Assert that the scrim background is a specific color. */
    private void assertScrimColor(@ColorInt int color) {
        assertEquals("Scrim color was incorrect.", color,
                ((ColorDrawable) mScrimCoordinator.getViewForTesting().getBackground()).getColor());
    }

    /**
     * Assert that the scrim is the desired visibility.
     * @param visible Whether the scrim should be visible.
     */
    private void assertScrimVisibility(final boolean visible) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            if (visible) {
                assertEquals("The scrim should be visible.", View.VISIBLE,
                        mScrimCoordinator.getViewForTesting().getVisibility());
            } else {
                assertNull("The scrim should be null after being hidden.",
                        mScrimCoordinator.getViewForTesting());
            }
        });
    }
}

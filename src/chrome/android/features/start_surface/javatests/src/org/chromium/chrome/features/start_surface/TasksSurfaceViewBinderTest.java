// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_MARGIN;

import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.UiThreadTest;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/** Tests for {@link TasksSurfaceViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TasksSurfaceViewBinderTest extends BlankUiTestActivityTestCase {
    private ViewGroup mParentView;
    private ViewGroup mTasksSurfaceView;
    private PropertyModel mPropertyModel;
    @SuppressWarnings({"FieldCanBeLocal", "unused"})
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Note that the specific type of the parent view and tasks surface view do not matter
            // for the TasksSurfaceViewBinder.
            mParentView = new FrameLayout(getActivity());
            mTasksSurfaceView = new FrameLayout(getActivity());
            getActivity().setContentView(mParentView);

            mPropertyModel = new PropertyModel(StartSurfaceProperties.ALL_KEYS);
            mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(mPropertyModel,
                    new TasksSurfaceViewBinder.ViewHolder(mParentView, mTasksSurfaceView),
                    TasksSurfaceViewBinder::bind);
        });
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetShowAndHideOverview() {
        assertFalse(mPropertyModel.get(IS_SHOWING_OVERVIEW));
        assertNull(mTasksSurfaceView.getParent());

        mPropertyModel.set(BOTTOM_BAR_HEIGHT, 10);
        mPropertyModel.set(TOP_MARGIN, 20);

        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        assertEquals(mTasksSurfaceView.getVisibility(), View.VISIBLE);
        assertNotNull(mTasksSurfaceView.getParent());
        MarginLayoutParams layoutParams = (MarginLayoutParams) mTasksSurfaceView.getLayoutParams();
        assertEquals(10, layoutParams.bottomMargin);
        assertEquals(20, layoutParams.topMargin);

        mPropertyModel.set(IS_SHOWING_OVERVIEW, false);
        assertEquals(mTasksSurfaceView.getVisibility(), View.GONE);
        assertNotNull(mTasksSurfaceView.getParent());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetBottomBarHeight() {
        mPropertyModel.set(BOTTOM_BAR_HEIGHT, 10);
        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        MarginLayoutParams layoutParams = (MarginLayoutParams) mTasksSurfaceView.getLayoutParams();
        assertEquals(10, layoutParams.bottomMargin);

        mPropertyModel.set(BOTTOM_BAR_HEIGHT, 20);
        layoutParams = (MarginLayoutParams) mTasksSurfaceView.getLayoutParams();
        assertEquals(20, layoutParams.bottomMargin);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetTopBarHeight() {
        mPropertyModel.set(TOP_MARGIN, 10);
        mPropertyModel.set(IS_SHOWING_OVERVIEW, true);
        MarginLayoutParams layoutParams = (MarginLayoutParams) mTasksSurfaceView.getLayoutParams();
        assertEquals("Top margin isn't initialized correctly.", 10, layoutParams.topMargin);

        mPropertyModel.set(TOP_MARGIN, 20);
        layoutParams = (MarginLayoutParams) mTasksSurfaceView.getLayoutParams();
        assertEquals("Wrong top margin.", 20, layoutParams.topMargin);
    }
}

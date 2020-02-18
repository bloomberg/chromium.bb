// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.annotation.SuppressLint;
import android.content.res.ColorStateList;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.toolbar.ToolbarColors;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for {@link TabGroupUiToolbarViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabGroupUiToolbarViewBinderTest extends DummyUiActivityTestCase {
    private ImageView mLeftButton;
    private ImageView mRightButton;
    private ViewGroup mContainerView;
    private View mMainContent;

    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMCP;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        ViewGroup parentView = new FrameLayout(getActivity());
        TabGroupUiToolbarView toolbarView =
                (TabGroupUiToolbarView) LayoutInflater.from(getActivity())
                        .inflate(R.layout.bottom_tab_strip_toolbar, parentView, false);
        mLeftButton = toolbarView.findViewById(R.id.toolbar_left_button);
        mRightButton = toolbarView.findViewById(R.id.toolbar_right_button);
        mContainerView = toolbarView.findViewById(R.id.toolbar_container_view);
        mMainContent = toolbarView.findViewById(R.id.main_content);

        mModel = new PropertyModel(TabStripToolbarViewProperties.ALL_KEYS);
        mMCP = PropertyModelChangeProcessor.create(
                mModel, toolbarView, TabGroupUiToolbarViewBinder::bind);
    }

    @Override
    public void tearDownTest() throws Exception {
        mMCP.destroy();
        super.tearDownTest();
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetLeftButtonOnClickListener() {
        AtomicBoolean leftButtonClicked = new AtomicBoolean();
        leftButtonClicked.set(false);
        mLeftButton.performClick();
        assertFalse(leftButtonClicked.get());

        mModel.set(TabStripToolbarViewProperties.LEFT_BUTTON_ON_CLICK_LISTENER,
                (View view) -> leftButtonClicked.set(true));

        mLeftButton.performClick();
        assertTrue(leftButtonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetRightButtonOnClickListener() {
        AtomicBoolean rightButtonClicked = new AtomicBoolean();
        rightButtonClicked.set(false);
        mRightButton.performClick();
        assertFalse(rightButtonClicked.get());

        mModel.set(TabStripToolbarViewProperties.RIGHT_BUTTON_ON_CLICK_LISTENER,
                (View view) -> rightButtonClicked.set(true));

        mRightButton.performClick();
        assertTrue(rightButtonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetMainContentVisibility() {
        View contentView = new View(getActivity());
        mContainerView.addView(contentView);
        contentView.setVisibility(View.GONE);

        mModel.set(TabStripToolbarViewProperties.IS_MAIN_CONTENT_VISIBLE, true);
        assertEquals(View.VISIBLE, contentView.getVisibility());

        mModel.set(TabStripToolbarViewProperties.IS_MAIN_CONTENT_VISIBLE, false);
        assertEquals(View.INVISIBLE, contentView.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetLeftButtonDrawable() {
        int expandLessDrawableId = R.drawable.ic_expand_less_black_24dp;
        int expandMoreDrawableId = R.drawable.ic_expand_more_black_24dp;

        mModel.set(TabStripToolbarViewProperties.LEFT_BUTTON_DRAWABLE_ID, expandLessDrawableId);
        Drawable expandLessDrawable = mLeftButton.getDrawable();
        mModel.set(TabStripToolbarViewProperties.LEFT_BUTTON_DRAWABLE_ID, expandMoreDrawableId);
        Drawable expandMoreDrawable = mLeftButton.getDrawable();

        assertNotEquals(expandLessDrawable, expandMoreDrawable);
    }

    @Test
    @UiThreadTest
    @SmallTest
    @SuppressLint("NewApi") // Used on K+ because getImageTintList is only supported on API >= 21.
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.LOLLIPOP)
    public void testSetTint() {
        ColorStateList tint = ToolbarColors.getThemedToolbarIconTint(getActivity(), true);
        Assert.assertNotEquals(tint, mLeftButton.getImageTintList());
        Assert.assertNotEquals(tint, mRightButton.getImageTintList());

        mModel.set(TabStripToolbarViewProperties.TINT, tint);

        Assert.assertEquals(tint, mLeftButton.getImageTintList());
        Assert.assertEquals(tint, mRightButton.getImageTintList());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetPrimaryColor() {
        int colorGrey = R.color.modern_grey_300;
        int colorBlue = R.color.modern_blue_300;

        mModel.set(TabStripToolbarViewProperties.PRIMARY_COLOR, colorGrey);
        int greyDrawableId = ((ColorDrawable) mMainContent.getBackground()).getColor();
        mModel.set(TabStripToolbarViewProperties.PRIMARY_COLOR, colorBlue);
        int blueDrawableId = ((ColorDrawable) mMainContent.getBackground()).getColor();

        assertNotEquals(greyDrawableId, blueDrawableId);
    }
}

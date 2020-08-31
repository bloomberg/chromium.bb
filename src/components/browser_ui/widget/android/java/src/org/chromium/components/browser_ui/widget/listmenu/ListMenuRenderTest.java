// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.listmenu;

import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.app.Activity;
import android.view.View;
import android.widget.LinearLayout.LayoutParams;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.List;

/**
 * Render tests for {@link BasicListMenu}.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
public class ListMenuRenderTest extends DummyUiActivityTestCase {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("chrome/test/data/android/render_tests");

    private View mView;

    public ListMenuRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForDummyUiActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Activity activity = getActivity();
            ModelList data = new ModelList();
            data.add(BasicListMenu.buildMenuListItem(
                    R.string.test_primary_1, 0, R.drawable.ic_check_googblue_24dp));
            data.add(BasicListMenu.buildMenuListItem(
                    R.string.test_primary_1, 0, R.drawable.ic_check_googblue_24dp, false));
            data.add(BasicListMenu.buildMenuListItemWithEndIcon(
                    R.string.test_primary_1, 0, R.drawable.ic_check_googblue_24dp, true));
            data.add(BasicListMenu.buildMenuListItemWithEndIcon(
                    R.string.test_primary_1, 0, R.drawable.ic_check_googblue_24dp, false));
            data.add(BasicListMenu.buildMenuListItem(R.string.test_primary_1, 0, 0));
            data.add(BasicListMenu.buildMenuListItem(R.string.test_primary_1, 0, 0, false));
            data.add(BasicListMenu.buildMenuDivider());

            BasicListMenu listMenu = new BasicListMenu(activity, data, null);
            mView = listMenu.getContentView();
            mView.setBackground(ApiCompatibilityUtils.getDrawable(
                    activity.getResources(), R.drawable.popup_bg_tinted));
            int width = activity.getResources().getDimensionPixelSize(R.dimen.list_menu_width);
            activity.setContentView(mView, new LayoutParams(width, WRAP_CONTENT));
        });
    }

    @Override
    public void tearDownTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { NightModeTestUtils.tearDownNightModeForDummyUiActivity(); });
        super.tearDownTest();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_BasicListMenu() throws IOException {
        mRenderTestRule.render(mView, "basic_list_menu");
    }
}

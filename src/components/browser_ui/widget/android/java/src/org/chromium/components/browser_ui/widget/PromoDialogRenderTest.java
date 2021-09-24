// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.app.Activity;
import android.content.DialogInterface;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.test.DummyUiComponentsActivityTestCase;
import org.chromium.components.browser_ui.widget.PromoDialog.DialogParams;
import org.chromium.components.browser_ui.widget.test.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.List;

/**
 * Render tests for {@link PromoDialog}.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
public class PromoDialogRenderTest extends DummyUiComponentsActivityTestCase {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public RenderTestRule mRenderTestRule = RenderTestRule.Builder.withPublicCorpus().build();

    private static final String LONG_STRING = "A very very very very very very very very very"
            + "very very very very long string";

    public PromoDialogRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForDummyUiActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    private View getDialogLayout(DialogParams dialogParams) throws Exception {
        Activity activity = getActivity();
        PromoDialog dialog = TestThreadUtils.runOnUiThreadBlocking(() -> {
            PromoDialog testDialog = new PromoDialog(activity) {
                @Override
                protected DialogParams getDialogParams() {
                    return dialogParams;
                }

                @Override
                public void onDismiss(DialogInterface dialog) {}
            };
            testDialog.onCreate(null);
            return testDialog;
        });
        View dialogLayout = TestThreadUtils.runOnUiThreadBlocking(
                () -> dialog.getWindow().getDecorView().findViewById(R.id.promo_dialog_layout));
        return dialogLayout;
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "PromoDialog"})
    public void testBasic() throws Exception {
        DialogParams params = new DialogParams();
        params.vectorDrawableResource = R.drawable.promo_dialog_test_vector;
        params.headerStringResource = R.string.promo_dialog_test_header;
        params.subheaderStringResource = R.string.promo_dialog_test_subheader;
        params.primaryButtonStringResource = R.string.promo_dialog_test_primary_button;
        params.secondaryButtonStringResource = R.string.promo_dialog_test_secondary_button;
        params.footerStringResource = R.string.promo_dialog_test_footer;
        View layout = getDialogLayout(params);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ((ViewGroup) (layout.getParent())).removeView(layout);
            getActivity().setContentView(layout);
        });
        mRenderTestRule.render(layout, "promo_dialog_basic");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "PromoDialog"})
    public void testBasic_StackButton() throws Exception {
        DialogParams params = new DialogParams();
        params.vectorDrawableResource = R.drawable.promo_dialog_test_vector;
        params.headerStringResource = R.string.promo_dialog_test_header;
        params.subheaderStringResource = R.string.promo_dialog_test_subheader;
        params.primaryButtonCharSequence = "A very very very very very very very very very"
                + "very very very very long string";
        params.secondaryButtonStringResource = R.string.promo_dialog_test_secondary_button;
        params.footerStringResource = R.string.promo_dialog_test_footer;
        View layout = getDialogLayout(params);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ((ViewGroup) (layout.getParent())).removeView(layout);
            getActivity().setContentView(layout);
        });
        mRenderTestRule.render(layout, "promo_dialog_basic_stack_button");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "PromoDialog"})
    public void testBasic_Landscape() throws Exception {
        DialogParams params = new DialogParams();
        params.vectorDrawableResource = R.drawable.promo_dialog_test_vector;
        params.headerStringResource = R.string.promo_dialog_test_header;
        params.subheaderStringResource = R.string.promo_dialog_test_subheader;
        params.primaryButtonStringResource = R.string.promo_dialog_test_primary_button;
        params.secondaryButtonStringResource = R.string.promo_dialog_test_secondary_button;
        params.footerStringResource = R.string.promo_dialog_test_footer;
        View layout = getDialogLayout(params);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ((ViewGroup) (layout.getParent())).removeView(layout);
            getActivity().setContentView(layout, new LayoutParams(1600, 1000));
        });

        mRenderTestRule.render(layout, "promo_dialog_basic_landscape");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "PromoDialog"})
    public void testBasic_StackButton_Landscape() throws Exception {
        DialogParams params = new DialogParams();
        params.vectorDrawableResource = R.drawable.promo_dialog_test_vector;
        params.headerStringResource = R.string.promo_dialog_test_header;
        params.subheaderStringResource = R.string.promo_dialog_test_subheader;
        params.primaryButtonCharSequence = LONG_STRING;
        params.secondaryButtonStringResource = R.string.promo_dialog_test_secondary_button;
        params.footerStringResource = R.string.promo_dialog_test_footer;
        View layout = getDialogLayout(params);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ((ViewGroup) (layout.getParent())).removeView(layout);
            getActivity().setContentView(layout, new LayoutParams(1600, 1000));
        });

        mRenderTestRule.render(layout, "promo_dialog_basic_stack_button_landscape");
    }
}

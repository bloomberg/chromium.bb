// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import android.app.Activity;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.StyleSpan;
import android.view.LayoutInflater;
import android.view.ViewGroup.LayoutParams;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.util.List;
/**
 * Render tests for Message Banner.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
public class MessageBannerRenderTest extends DummyUiActivityTestCase {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public RenderTestRule mRenderTestRule = RenderTestRule.Builder.withPublicCorpus().build();

    public MessageBannerRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForDummyUiActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Messages"})
    public void testBasic() throws Exception {
        Activity activity = getActivity();
        Drawable drawable = ApiCompatibilityUtils.getDrawable(
                activity.getResources(), android.R.drawable.ic_delete);
        PropertyModel model = new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                      .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                              MessageIdentifier.TEST_MESSAGE)
                                      .with(MessageBannerProperties.ICON, drawable)
                                      .with(MessageBannerProperties.TITLE, "Primary Title")
                                      .with(MessageBannerProperties.DESCRIPTION, "Secondary Title")
                                      .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Action")
                                      .build();
        MessageBannerView view = (MessageBannerView) LayoutInflater.from(activity).inflate(
                R.layout.message_banner_view, null, false);
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        LayoutParams params = new LayoutParams(LayoutParams.MATCH_PARENT,
                activity.getResources().getDimensionPixelSize(R.dimen.message_banner_height));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { getActivity().setContentView(view, params); });
        mRenderTestRule.render(view, "message_banner_basic");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Messages"})
    public void testBasic_withSecondaryIcon() throws Exception {
        Activity activity = getActivity();
        Drawable drawable = ApiCompatibilityUtils.getDrawable(
                activity.getResources(), android.R.drawable.ic_delete);
        Drawable drawable2 = ApiCompatibilityUtils.getDrawable(
                activity.getResources(), android.R.drawable.ic_btn_speak_now);
        PropertyModel model = new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                      .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                              MessageIdentifier.TEST_MESSAGE)
                                      .with(MessageBannerProperties.ICON, drawable)
                                      .with(MessageBannerProperties.TITLE, "Primary Title")
                                      .with(MessageBannerProperties.DESCRIPTION, "Secondary Title")
                                      .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Action")
                                      .with(MessageBannerProperties.SECONDARY_ICON, drawable2)
                                      .build();
        MessageBannerView view = (MessageBannerView) LayoutInflater.from(activity).inflate(
                R.layout.message_banner_view, null, false);
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        LayoutParams params = new LayoutParams(LayoutParams.MATCH_PARENT,
                activity.getResources().getDimensionPixelSize(R.dimen.message_banner_height));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { getActivity().setContentView(view, params); });
        mRenderTestRule.render(view, "message_banner_basic_with_secondary_icon");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Messages"})
    public void testBasic_withSpannableDescription() throws Exception {
        Activity activity = getActivity();
        Drawable drawable = ApiCompatibilityUtils.getDrawable(
                activity.getResources(), android.R.drawable.ic_delete);
        SpannableString spannable = new SpannableString("Dummy Spannable Description!");
        StyleSpan boldSpan = new StyleSpan(Typeface.BOLD);
        spannable.setSpan(boldSpan, 0, spannable.length(), Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        PropertyModel model = new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                      .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                              MessageIdentifier.TEST_MESSAGE)
                                      .with(MessageBannerProperties.ICON, drawable)
                                      .with(MessageBannerProperties.TITLE, "Primary Title")
                                      .with(MessageBannerProperties.DESCRIPTION, spannable)
                                      .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Action")
                                      .build();
        MessageBannerView view = (MessageBannerView) LayoutInflater.from(activity).inflate(
                R.layout.message_banner_view, null, false);
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        LayoutParams params = new LayoutParams(LayoutParams.MATCH_PARENT,
                activity.getResources().getDimensionPixelSize(R.dimen.message_banner_height));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { getActivity().setContentView(view, params); });
        mRenderTestRule.render(view, "message_banner_basic_with_spannable_description");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest", "Messages"})
    public void testBasic_multilineDescriptionMaxLines() throws Exception {
        Activity activity = getActivity();
        final String multilineDescription = "Line 1\nLine 2\nLine 3\nLine 4";
        PropertyModel model =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.TEST_MESSAGE)
                        .with(MessageBannerProperties.TITLE, "Primary Title")
                        .with(MessageBannerProperties.DESCRIPTION, multilineDescription)
                        .with(MessageBannerProperties.DESCRIPTION_MAX_LINES, 2)
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "Action")
                        .build();
        MessageBannerView view = (MessageBannerView) LayoutInflater.from(activity).inflate(
                R.layout.message_banner_view, null, false);
        PropertyModelChangeProcessor.create(model, view, MessageBannerViewBinder::bind);
        LayoutParams params =
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { getActivity().setContentView(view, params); });
        mRenderTestRule.render(view, "message_banner_basic_with_multiline_description");
    }
}
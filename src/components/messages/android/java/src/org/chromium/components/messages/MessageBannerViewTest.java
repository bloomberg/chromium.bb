// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.DummyUiActivity;

/**
 * Instrumentation tests for MessageBannerView.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class MessageBannerViewTest {
    private static final String SECONDARY_ACTION_TEXT = "SecondaryActionText";

    @ClassRule
    public static DisableAnimationsTestRule sDisableAnimationsRule =
            new DisableAnimationsTestRule();

    @ClassRule
    public static BaseActivityTestRule<DummyUiActivity> sActivityTestRule =
            new BaseActivityTestRule<>(DummyUiActivity.class);

    private static Activity sActivity;
    private static ViewGroup sContentView;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    Runnable mSecondaryActionCallback;

    MessageBannerView mMessageBannerView;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sActivity = sActivityTestRule.getActivity();
            sContentView = new FrameLayout(sActivity);
            sActivity.setContentView(sContentView);
        });
    }

    @Before
    public void setupTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sContentView.removeAllViews();
            mMessageBannerView = (MessageBannerView) LayoutInflater.from(sActivity).inflate(
                    R.layout.message_banner_view, sContentView, false);
            sContentView.addView(mMessageBannerView);
        });
    }

    /**
     * Tests that clicking on secondary button opens a menu with an item with SECONDARY_ACTION_TEXT.
     * Clicking on this item triggers ON_SECONDARY_ACTION callback invocation.
     */
    @Test
    @MediumTest
    public void testSecondaryActionMenu() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(MessageBannerProperties.SINGLE_ACTION_MESSAGE_KEYS)
                        .with(MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID,
                                android.R.drawable.ic_menu_add)
                        .with(MessageBannerProperties.SECONDARY_ACTION_TEXT, SECONDARY_ACTION_TEXT)
                        .with(MessageBannerProperties.ON_SECONDARY_ACTION, mSecondaryActionCallback)
                        .build();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModelChangeProcessor.create(
                    propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
        });
        onView(withId(R.id.message_secondary_button)).perform(click());
        onView(withText(SECONDARY_ACTION_TEXT)).perform(click());
        Mockito.verify(mSecondaryActionCallback).run();
    }
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;

import android.app.Activity;
import android.content.res.Resources;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.ForegroundColorSpan;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.components.browser_ui.modaldialog.test.R;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

/**
 * Tests for {@link ModalDialogView}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ModalDialogViewTest {
    @ClassRule
    public static DisableAnimationsTestRule disableAnimationsRule = new DisableAnimationsTestRule();
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;
    private static Resources sResources;
    private static FrameLayout sContentView;
    private ModalDialogView mModalDialogView;
    private TextView mCustomTextView1;
    private TextView mCustomTextView2;
    private PropertyModel.Builder mModelBuilder;

    @BeforeClass
    public static void setupSuite() {
        activityTestRule.launchActivity(null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sActivity = activityTestRule.getActivity();
            sResources = sActivity.getResources();
            sContentView = new FrameLayout(sActivity);
            sActivity.setContentView(sContentView);
        });
    }

    @Before
    public void setupTest() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            sContentView.removeAllViews();
            mModelBuilder = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS);
            mModalDialogView =
                    (ModalDialogView) LayoutInflater
                            .from(new ContextThemeWrapper(sActivity,
                                    R.style.Theme_Chromium_ModalDialog_TextPrimaryButton))
                            .inflate(R.layout.modal_dialog_view, null);
            sContentView.addView(mModalDialogView, MATCH_PARENT, WRAP_CONTENT);

            mCustomTextView1 = new TextView(sActivity);
            mCustomTextView1.setId(R.id.test_view_one);
            mCustomTextView2 = new TextView(sActivity);
            mCustomTextView2.setId(R.id.test_view_two);
        });
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testInitialStates() {
        // Verify that the default states are correct when properties are not set.
        createModel(mModelBuilder);
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.modal_dialog_scroll_view)).check(matches(not(isDisplayed())));
        onView(withId(R.id.message)).check(matches(not(isDisplayed())));
        onView(withId(R.id.custom)).check(matches(not(isDisplayed())));
        onView(withId(R.id.button_bar)).check(matches(not(isDisplayed())));
        onView(withId(R.id.positive_button)).check(matches(allOf(not(isDisplayed()), isEnabled())));
        onView(withId(R.id.negative_button)).check(matches(allOf(not(isDisplayed()), isEnabled())));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testTitle() {
        // Verify that the title set from builder is displayed.
        PropertyModel model = createModel(
                mModelBuilder.with(ModalDialogProperties.TITLE, sResources, R.string.title));
        onView(allOf(withId(R.id.title), withParent(withId(R.id.title_container))))
                .check(matches(allOf(isDisplayed(), withText(R.string.title))));
        onView(withId(R.id.title_container)).check(matches(isDisplayed()));
        onView(withId(R.id.modal_dialog_scroll_view)).check(matches(not(isDisplayed())));

        // Set an empty title and verify that title is not shown.
        TestThreadUtils.runOnUiThreadBlocking(() -> model.set(ModalDialogProperties.TITLE, ""));
        onView(allOf(withId(R.id.title), withParent(withId(R.id.title_container))))
                .check(matches(not(isDisplayed())));
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.modal_dialog_scroll_view)).check(matches(not(isDisplayed())));

        // Set a String title and verify that title is displayed.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.TITLE, "My Test Title"));
        onView(allOf(withId(R.id.title), withParent(withId(R.id.title_container))))
                .check(matches(allOf(isDisplayed(), withText("My Test Title"))));
        onView(withId(R.id.title_container)).check(matches(isDisplayed()));
        onView(withId(R.id.modal_dialog_scroll_view)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testTitle_Scrollable() {
        // Verify that the title set from builder is displayed.
        PropertyModel model = createModel(
                mModelBuilder.with(ModalDialogProperties.TITLE, sResources, R.string.title)
                        .with(ModalDialogProperties.TITLE_SCROLLABLE, true));
        onView(allOf(withId(R.id.title), withParent(withId(R.id.scrollable_title_container))))
                .check(matches(allOf(isDisplayed(), withText(R.string.title))));
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.scrollable_title_container)).check(matches(isDisplayed()));
        onView(withId(R.id.modal_dialog_scroll_view)).check(matches(isDisplayed()));
        onView(withId(R.id.message)).check(matches(not(isDisplayed())));

        // Set title to not scrollable and verify that non-scrollable title is displayed.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.TITLE_SCROLLABLE, false));
        onView(allOf(withId(R.id.title), withParent(withId(R.id.title_container))))
                .check(matches(allOf(isDisplayed(), withText(R.string.title))));
        onView(withId(R.id.title_container)).check(matches(isDisplayed()));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.modal_dialog_scroll_view)).check(matches(not(isDisplayed())));
        onView(withId(R.id.message)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testTitleIcon() {
        // Verify that the icon set from builder is displayed.
        PropertyModel model = createModel(mModelBuilder.with(
                ModalDialogProperties.TITLE_ICON, sActivity, R.drawable.ic_business));
        onView(allOf(withId(R.id.title), withParent(withId(R.id.title_container))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.title_icon), withParent(withId(R.id.title_container))))
                .check(matches(isDisplayed()));
        onView(withId(R.id.title_container)).check(matches(isDisplayed()));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));

        // Set icon to null and verify that icon is not shown.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.TITLE_ICON, null));
        onView(allOf(withId(R.id.title), withParent(withId(R.id.title_container))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.title_icon), withParent(withId(R.id.title_container))))
                .check(matches(not(isDisplayed())));
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testMessage() {
        // Verify that the message set from builder is displayed.
        String msg = sResources.getString(R.string.more);
        PropertyModel model = createModel(mModelBuilder.with(ModalDialogProperties.MESSAGE, msg));
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.modal_dialog_scroll_view)).check(matches(isDisplayed()));
        onView(withId(R.id.message)).check(matches(allOf(isDisplayed(), withText(R.string.more))));

        // Set an empty message and verify that message is not shown.
        TestThreadUtils.runOnUiThreadBlocking(() -> model.set(ModalDialogProperties.MESSAGE, ""));
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.modal_dialog_scroll_view)).check(matches(not(isDisplayed())));
        onView(withId(R.id.message)).check(matches(not(isDisplayed())));

        // Use CharSequence for the message.
        SpannableStringBuilder sb = new SpannableStringBuilder(msg);
        sb.setSpan(new ForegroundColorSpan(0xffff0000), 0, 1, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        TestThreadUtils.runOnUiThreadBlocking(() -> model.set(ModalDialogProperties.MESSAGE, sb));
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.modal_dialog_scroll_view)).check(matches(isDisplayed()));
        onView(withId(R.id.message)).check(matches(allOf(isDisplayed(), withText(R.string.more))));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testCustomView() {
        // Verify custom view set from builder is displayed.
        PropertyModel model = createModel(
                mModelBuilder.with(ModalDialogProperties.CUSTOM_VIEW, mCustomTextView1));
        onView(withId(R.id.custom))
                .check(matches(allOf(isDisplayed(), withChild(withId(R.id.test_view_one)))));

        // Change custom view.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.CUSTOM_VIEW, mCustomTextView2));
        onView(withId(R.id.custom))
                .check(matches(allOf(isDisplayed(), not(withChild(withId(R.id.test_view_one))),
                        withChild(withId(R.id.test_view_two)))));

        // Set custom view to null.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.CUSTOM_VIEW, null));
        onView(withId(R.id.custom))
                .check(matches(allOf(not(isDisplayed()), not(withChild(withId(R.id.test_view_one))),
                        not(withChild(withId(R.id.test_view_two))))));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testButtonBar() {
        // Set text for both positive button and negative button.
        PropertyModel model = createModel(
                mModelBuilder
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, sResources, R.string.ok)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, sResources,
                                R.string.cancel));
        onView(withId(R.id.button_bar)).check(matches(isDisplayed()));
        onView(withId(R.id.positive_button))
                .check(matches(allOf(isDisplayed(), isEnabled(), withText(R.string.ok))));
        onView(withId(R.id.negative_button))
                .check(matches(allOf(isDisplayed(), isEnabled(), withText(R.string.cancel))));

        // Set positive button to be disabled state.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true));
        onView(withId(R.id.button_bar)).check(matches(isDisplayed()));
        onView(withId(R.id.positive_button))
                .check(matches(allOf(isDisplayed(), not(isEnabled()), withText(R.string.ok))));
        onView(withId(R.id.negative_button))
                .check(matches(allOf(isDisplayed(), isEnabled(), withText(R.string.cancel))));

        // Set positive button text to empty.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.POSITIVE_BUTTON_TEXT, ""));
        onView(withId(R.id.button_bar)).check(matches(isDisplayed()));
        onView(withId(R.id.positive_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.negative_button))
                .check(matches(allOf(isDisplayed(), isEnabled(), withText(R.string.cancel))));

        // Set negative button to be disabled state.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.NEGATIVE_BUTTON_DISABLED, true));
        onView(withId(R.id.button_bar)).check(matches(isDisplayed()));
        onView(withId(R.id.positive_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.negative_button))
                .check(matches(allOf(isDisplayed(), not(isEnabled()), withText(R.string.cancel))));

        // Set negative button text to empty.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, ""));
        onView(withId(R.id.button_bar)).check(matches(not(isDisplayed())));
        onView(withId(R.id.positive_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.negative_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testTouchFilter() {
        PropertyModel model = createModel(
                mModelBuilder
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, sResources, R.string.ok)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, sResources,
                                R.string.cancel)
                        .with(ModalDialogProperties.FILTER_TOUCH_FOR_SECURITY, true));
        onView(withId(R.id.positive_button)).check(matches(touchFilterEnabled()));
        onView(withId(R.id.negative_button)).check(matches(touchFilterEnabled()));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testTouchFilterDisabled() {
        PropertyModel model = createModel(
                mModelBuilder
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, sResources, R.string.ok)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, sResources,
                                R.string.cancel));
        onView(withId(R.id.positive_button)).check(matches(not(touchFilterEnabled())));
        onView(withId(R.id.negative_button)).check(matches(not(touchFilterEnabled())));
    }

    private static Matcher<View> touchFilterEnabled() {
        return new TypeSafeMatcher<View>() {
            @Override
            public void describeTo(Description description) {
                description.appendText("Touch filtering enabled");
            }
            @Override
            public boolean matchesSafely(View view) {
                return view.getFilterTouchesWhenObscured();
            }
        };
    }

    private PropertyModel createModel(PropertyModel.Builder modelBuilder) {
        return ModalDialogTestUtils.createModel(modelBuilder, mModalDialogView);
    }
}

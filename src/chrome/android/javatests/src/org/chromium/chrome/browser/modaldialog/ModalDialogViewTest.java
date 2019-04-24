// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isEnabled;
import static android.support.test.espresso.matcher.ViewMatchers.withChild;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withParent;
import static android.support.test.espresso.matcher.ViewMatchers.withText;
import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;

import android.content.res.Resources;
import android.support.test.filters.MediumTest;
import android.text.TextUtils;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.widget.FrameLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.io.IOException;
import java.util.Collections;

/**
 * Tests for {@link ModalDialogView}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ModalDialogViewTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("chrome/test/data/android/render_tests");

    private Resources mResources;
    private PropertyModel.Builder mModelBuilder;
    private FrameLayout mContentView;
    private ModalDialogView mModalDialogView;
    private ScrollView mCustomScrollView;
    private TextView mCustomTextView1;
    private TextView mCustomTextView2;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        ThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeActivity activity = mActivityTestRule.getActivity();
            mResources = activity.getResources();
            mModelBuilder = new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS);

            mContentView = new FrameLayout(activity);
            mModalDialogView = (ModalDialogView) LayoutInflater
                                       .from(new ContextThemeWrapper(
                                               activity, R.style.Theme_Chromium_ModalDialog))
                                       .inflate(R.layout.modal_dialog_view, null);
            activity.setContentView(mContentView);
            mContentView.addView(mModalDialogView, MATCH_PARENT, WRAP_CONTENT);

            mCustomScrollView = new ScrollView(activity);
            mCustomTextView1 = new TextView(activity);
            mCustomTextView1.setId(R.id.button_one);
            mCustomTextView2 = new TextView(activity);
            mCustomTextView2.setId(R.id.button_two);
        });
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog", "RenderTest"})
    public void testRender_TitleAndTitleIcon() throws IOException {
        createModel(mModelBuilder.with(ModalDialogProperties.TITLE, mResources, R.string.title)
                            .with(ModalDialogProperties.TITLE_ICON, mActivityTestRule.getActivity(),
                                    R.drawable.ic_add));
        mRenderTestRule.render(mModalDialogView, "title_and_title_icon");
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog", "RenderTest"})
    public void testRender_TitleAndMessage() throws IOException {
        createModel(
                mModelBuilder.with(ModalDialogProperties.TITLE, mResources, R.string.title)
                        .with(ModalDialogProperties.MESSAGE,
                                TextUtils.join("\n", Collections.nCopies(100, "Message")))
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mResources, R.string.ok)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, mResources,
                                R.string.cancel));
        mRenderTestRule.render(mModalDialogView, "title_and_message");
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog", "RenderTest"})
    public void testRender_ScrollableTitle() throws IOException {
        createModel(
                mModelBuilder.with(ModalDialogProperties.TITLE, mResources, R.string.title)
                        .with(ModalDialogProperties.TITLE_SCROLLABLE, true)
                        .with(ModalDialogProperties.MESSAGE,
                                TextUtils.join("\n", Collections.nCopies(100, "Message")))
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mResources, R.string.ok));
        mRenderTestRule.render(mModalDialogView, "scrollable_title");
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog", "RenderTest"})
    public void testRender_CustomView() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mCustomTextView1.setText(
                    TextUtils.join("\n", Collections.nCopies(100, "Custom Message")));
            mCustomScrollView.addView(mCustomTextView1);
        });
        createModel(
                mModelBuilder.with(ModalDialogProperties.TITLE, mResources, R.string.title)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mCustomScrollView)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mResources, R.string.ok));
        mRenderTestRule.render(mModalDialogView, "custom_view");
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
                mModelBuilder.with(ModalDialogProperties.TITLE, mResources, R.string.title));
        onView(allOf(withId(R.id.title), withParent(withId(R.id.title_container))))
                .check(matches(allOf(isDisplayed(), withText(R.string.title))));
        onView(withId(R.id.title_container)).check(matches(isDisplayed()));
        onView(withId(R.id.modal_dialog_scroll_view)).check(matches(not(isDisplayed())));

        // Set an empty title and verify that title is not shown.
        ThreadUtils.runOnUiThreadBlocking(() -> model.set(ModalDialogProperties.TITLE, ""));
        onView(allOf(withId(R.id.title), withParent(withId(R.id.title_container))))
                .check(matches(not(isDisplayed())));
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.modal_dialog_scroll_view)).check(matches(not(isDisplayed())));

        // Set a String title and verify that title is displayed.
        ThreadUtils.runOnUiThreadBlocking(
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
                mModelBuilder.with(ModalDialogProperties.TITLE, mResources, R.string.title)
                        .with(ModalDialogProperties.TITLE_SCROLLABLE, true));
        onView(allOf(withId(R.id.title), withParent(withId(R.id.scrollable_title_container))))
                .check(matches(allOf(isDisplayed(), withText(R.string.title))));
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.scrollable_title_container)).check(matches(isDisplayed()));
        onView(withId(R.id.modal_dialog_scroll_view)).check(matches(isDisplayed()));
        onView(withId(R.id.message)).check(matches(not(isDisplayed())));

        // Set title to not scrollable and verify that non-scrollable title is displayed.
        ThreadUtils.runOnUiThreadBlocking(
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
        PropertyModel model = createModel(mModelBuilder.with(ModalDialogProperties.TITLE_ICON,
                mActivityTestRule.getActivity(), R.drawable.ic_add));
        onView(allOf(withId(R.id.title), withParent(withId(R.id.title_container))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.title_icon), withParent(withId(R.id.title_container))))
                .check(matches(isDisplayed()));
        onView(withId(R.id.title_container)).check(matches(isDisplayed()));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));

        // Set icon to null and verify that icon is not shown.
        ThreadUtils.runOnUiThreadBlocking(() -> model.set(ModalDialogProperties.TITLE_ICON, null));
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
        PropertyModel model = createModel(
                mModelBuilder.with(ModalDialogProperties.MESSAGE, mResources, R.string.more));
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.modal_dialog_scroll_view)).check(matches(isDisplayed()));
        onView(withId(R.id.message)).check(matches(allOf(isDisplayed(), withText(R.string.more))));

        // Set an empty message and verify that message is not shown.
        ThreadUtils.runOnUiThreadBlocking(() -> model.set(ModalDialogProperties.MESSAGE, ""));
        onView(withId(R.id.title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.scrollable_title_container)).check(matches(not(isDisplayed())));
        onView(withId(R.id.modal_dialog_scroll_view)).check(matches(not(isDisplayed())));
        onView(withId(R.id.message)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testCustomView() {
        // Verify custom view set from builder is displayed.
        PropertyModel model = createModel(
                mModelBuilder.with(ModalDialogProperties.CUSTOM_VIEW, mCustomTextView1));
        onView(withId(R.id.custom))
                .check(matches(allOf(isDisplayed(), withChild(withId(R.id.button_one)))));

        // Change custom view.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.CUSTOM_VIEW, mCustomTextView2));
        onView(withId(R.id.custom))
                .check(matches(allOf(isDisplayed(), not(withChild(withId(R.id.button_one))),
                        withChild(withId(R.id.button_two)))));

        // Set custom view to null.
        ThreadUtils.runOnUiThreadBlocking(() -> model.set(ModalDialogProperties.CUSTOM_VIEW, null));
        onView(withId(R.id.custom))
                .check(matches(allOf(not(isDisplayed()), not(withChild(withId(R.id.button_one))),
                        not(withChild(withId(R.id.button_two))))));
    }

    @Test
    @MediumTest
    @Feature({"ModalDialog"})
    public void testButtonBar() {
        // Set text for both positive button and negative button.
        PropertyModel model = createModel(
                mModelBuilder
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, mResources, R.string.ok)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, mResources,
                                R.string.cancel));
        onView(withId(R.id.button_bar)).check(matches(isDisplayed()));
        onView(withId(R.id.positive_button))
                .check(matches(allOf(isDisplayed(), isEnabled(), withText(R.string.ok))));
        onView(withId(R.id.negative_button))
                .check(matches(allOf(isDisplayed(), isEnabled(), withText(R.string.cancel))));

        // Set positive button to be disabled state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true));
        onView(withId(R.id.button_bar)).check(matches(isDisplayed()));
        onView(withId(R.id.positive_button))
                .check(matches(allOf(isDisplayed(), not(isEnabled()), withText(R.string.ok))));
        onView(withId(R.id.negative_button))
                .check(matches(allOf(isDisplayed(), isEnabled(), withText(R.string.cancel))));

        // Set positive button text to empty.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.POSITIVE_BUTTON_TEXT, ""));
        onView(withId(R.id.button_bar)).check(matches(isDisplayed()));
        onView(withId(R.id.positive_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.negative_button))
                .check(matches(allOf(isDisplayed(), isEnabled(), withText(R.string.cancel))));

        // Set negative button to be disabled state.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.NEGATIVE_BUTTON_DISABLED, true));
        onView(withId(R.id.button_bar)).check(matches(isDisplayed()));
        onView(withId(R.id.positive_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.negative_button))
                .check(matches(allOf(isDisplayed(), not(isEnabled()), withText(R.string.cancel))));

        // Set negative button text to empty.
        ThreadUtils.runOnUiThreadBlocking(
                () -> model.set(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, ""));
        onView(withId(R.id.button_bar)).check(matches(not(isDisplayed())));
        onView(withId(R.id.positive_button)).check(matches(not(isDisplayed())));
        onView(withId(R.id.negative_button)).check(matches(not(isDisplayed())));
    }

    private PropertyModel createModel(PropertyModel.Builder modelBuilder) {
        return ThreadUtils.runOnUiThreadBlockingNoException(() -> {
            PropertyModel model = modelBuilder.build();
            PropertyModelChangeProcessor.create(
                    model, mModalDialogView, new ModalDialogViewBinder());
            return model;
        });
    }
}

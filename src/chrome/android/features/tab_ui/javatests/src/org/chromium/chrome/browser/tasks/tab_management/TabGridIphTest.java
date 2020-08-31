// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.Espresso.pressBack;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.RootMatchers.withDecorView;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withParent;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.closeFirstTabInTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.getSwipeToDismissAction;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.rotateDeviceToOrientation;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;

import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.graphics.drawable.Animatable;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.NoMatchingRootException;
import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.test.filters.MediumTest;
import android.support.test.uiautomator.UiDevice;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.features.start_surface.StartSurfaceLayout;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;

/** End-to-end tests for TabGridIph component. */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
@Features.DisableFeatures(ChromeFeatureList.CLOSE_TAB_SUGGESTIONS)
@DisabledTest(message = "crbug.com/1062659 This test suite is failing on several bots.")
public class TabGridIphTest {
    // clang-format on
    private ModalDialogManager mModalDialogManager;

    // Disable animations to reduce flakiness.
    @ClassRule
    public static DisableAnimationsTestRule sEnableAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Rule
    public ChromeRenderTestRule mRenderTestRule = new ChromeRenderTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        Layout layout = mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);
        CriteriaHelper.pollUiThread(mActivityTestRule.getActivity()
                                            .getTabModelSelector()
                                            .getTabModelFilterProvider()
                                            .getCurrentTabModelFilter()::isTabModelRestored);
        mModalDialogManager = TestThreadUtils.runOnUiThreadBlockingNoException(
                mActivityTestRule.getActivity()::getModalDialogManager);
    }

    @After
    public void tearDown() {
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
    }

    @Test
    @MediumTest
    public void testShowAndHideIphDialog() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(
                TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        // Check the IPH message card is showing and open the IPH dialog.
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        verifyIphDialogShowing(cta);

        // Exit by clicking the "OK" button.
        exitIphDialogByClickingButton(cta);
        verifyIphDialogHiding(cta);

        // Check the IPH message card is showing and open the IPH dialog.
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        verifyIphDialogShowing(cta);

        // Press back should dismiss the IPH dialog.
        pressBack();
        verifyIphDialogHiding(cta);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        // Check the IPH message card is showing and open the IPH dialog.
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        verifyIphDialogShowing(cta);

        // Click outside of the dialog area to close the IPH dialog.
        View dialogView = mModalDialogManager.getCurrentDialogForTest().get(
                ModalDialogProperties.CUSTOM_VIEW);
        int[] location = new int[2];
        // Get the position of the dialog view and click slightly above so that we essentially click
        // on the scrim.
        dialogView.getLocationOnScreen(location);
        UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
                .click(location[0], location[1] / 2);
        verifyIphDialogHiding(cta);
    }

    @Test
    @MediumTest
    public void testDismissIphItem() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(
                TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        // Restart chrome to verify that IPH message card is still there.
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        mActivityTestRule.startMainActivityFromLauncher();
        cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(
                TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        // Remove the message card and dismiss the feature by clicking close button.
        onView(allOf(withId(R.id.close_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());

        // Restart chrome to verify that IPH message card no longer shows.
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        mActivityTestRule.startMainActivityFromLauncher();
        cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);
        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderIph_Portrait() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(
                TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        mRenderTestRule.render(
                cta.findViewById(R.id.tab_grid_message_item), "iph_entrance_portrait");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderIph_Landscape() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(
                TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        mRenderTestRule.render(
                cta.findViewById(R.id.tab_grid_message_item), "iph_entrance_landscape");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderIphDialog_Portrait() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        verifyIphDialogShowing(cta);

        View iphDialogView = mModalDialogManager.getCurrentDialogForTest().get(
                ModalDialogProperties.CUSTOM_VIEW);
        // Freeze animation and wait until animation is really frozen.
        ChromeRenderTestRule.sanitize(iphDialogView);
        ImageView iphImageView = iphDialogView.findViewById(R.id.animation_drawable);
        Animatable iphAnimation = (Animatable) iphImageView.getDrawable();
        CriteriaHelper.pollUiThread(() -> !iphAnimation.isRunning());

        mRenderTestRule.render(iphDialogView, "iph_dialog_portrait");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderIphDialog_Landscape() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        rotateDeviceToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        // Scroll to the position of the IPH entrance so that it is completely showing for Espresso
        // click.
        onView(allOf(withParent(withId(R.id.compositor_view_holder)), withId(R.id.tab_list_view)))
                .perform(RecyclerViewActions.scrollToPosition(1));
        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        verifyIphDialogShowing(cta);

        View iphDialogView = mModalDialogManager.getCurrentDialogForTest().get(
                ModalDialogProperties.CUSTOM_VIEW);
        // Freeze animation and wait until animation is really frozen.
        ChromeRenderTestRule.sanitize(iphDialogView);
        ImageView iphImageView = iphDialogView.findViewById(R.id.animation_drawable);
        Animatable iphAnimation = (Animatable) iphImageView.getDrawable();
        CriteriaHelper.pollUiThread(() -> !iphAnimation.isRunning());

        mRenderTestRule.render(iphDialogView, "iph_dialog_landscape");
    }

    @Test
    @MediumTest
    public void testIphItemChangeWithLastTab() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        // Close the last tab in tab switcher and the IPH item should not be showing.
        closeFirstTabInTabSwitcher();
        CriteriaHelper.pollUiThread(() -> !TabSwitcherCoordinator.hasAppendedMessagesForTesting());
        verifyTabSwitcherCardCount(cta, 0);
        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());

        // Undo the closure of the last tab and the IPH item should reshow.
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        // Close the last tab in the tab switcher.
        closeFirstTabInTabSwitcher();
        CriteriaHelper.pollUiThread(() -> !TabSwitcherCoordinator.hasAppendedMessagesForTesting());
        verifyTabSwitcherCardCount(cta, 0);
        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());

        // Add the first tab to an empty tab switcher and the IPH item should show.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), cta, false, true);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
        CriteriaHelper.pollUiThread(TabSwitcherCoordinator::hasAppendedMessagesForTesting);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSwipeToDismiss_IPH() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        RecyclerView.ViewHolder viewHolder = ((RecyclerView) cta.findViewById(R.id.tab_list_view))
                                                     .findViewHolderForAdapterPosition(1);
        assertEquals(TabProperties.UiType.MESSAGE, viewHolder.getItemViewType());

        onView(allOf(withParent(withId(R.id.compositor_view_holder)), withId(R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(
                        1, getSwipeToDismissAction(true)));

        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }

    private void verifyIphDialogShowing(ChromeTabbedActivity cta) {
        // Verify IPH dialog view.
        onView(withId(R.id.iph_dialog))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;

                    String title = cta.getString(R.string.iph_drag_and_drop_title);
                    assertEquals(title, ((TextView) v.findViewById(R.id.title)).getText());

                    String description = cta.getString(R.string.iph_drag_and_drop_content);
                    assertEquals(
                            description, ((TextView) v.findViewById(R.id.description)).getText());
                });
        // Verify ModalDialog button.
        onView(withId(R.id.positive_button))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check(matches(withText(cta.getString(R.string.ok))));
    }

    private void verifyIphDialogHiding(ChromeTabbedActivity cta) {
        boolean isShowing = true;
        try {
            onView(withId(R.id.iph_dialog))
                    .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                    .check(matches(isDisplayed()));
        } catch (NoMatchingRootException e) {
            isShowing = false;
        } catch (Exception e) {
            assert false : "error when inspecting iph dialog.";
        }
        assertFalse(isShowing);
    }

    private void exitIphDialogByClickingButton(ChromeTabbedActivity cta) {
        onView(withId(R.id.positive_button))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());
    }
}

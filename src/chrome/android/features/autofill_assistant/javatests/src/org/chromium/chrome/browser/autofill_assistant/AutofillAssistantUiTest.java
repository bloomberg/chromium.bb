// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.RootMatchers.withDecorView;
import static android.support.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.mockito.Mockito.inOrder;

import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantActionsCarouselCoordinator;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantCarouselModel;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetails;
import org.chromium.chrome.browser.autofill_assistant.details.AssistantDetailsModel;
import org.chromium.chrome.browser.autofill_assistant.header.AssistantHeaderModel;
import org.chromium.chrome.browser.autofill_assistant.infobox.AssistantInfoBox;
import org.chromium.chrome.browser.autofill_assistant.infobox.AssistantInfoBoxModel;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayModel;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayState;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.Arrays;
import java.util.List;

/**
 * Instrumentation tests for autofill assistant UI.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantUiTest {
    private String mTestPage;
    private EmbeddedTestServer mTestServer;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    public Runnable mRunnableMock;

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mTestPage = mTestServer.getURL(UrlUtils.getIsolatedTestFilePath(
                "components/test/data/autofill_assistant/autofill_assistant_target_website.html"));
        LibraryLoader.getInstance().ensureInitialized();
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));
        mTestServer.stopAndDestroyServer();
    }

    /**
     * @see CustomTabsTestUtils#createMinimalCustomTabIntent(Context, String).
     */
    private Intent createMinimalCustomTabIntent() {
        return CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), mTestPage);
    }

    private CustomTabActivity getActivity() {
        return mCustomTabActivityTestRule.getActivity();
    }

    protected BottomSheetController initializeBottomSheet() {
        return AutofillAssistantUiTestUtil.getBottomSheetController(getActivity());
    }


    // TODO(crbug.com/806868): Add more UI details test and check, like payment request UI,
    // highlight chips and so on.
    @Test
    @MediumTest
    public void testStartAndAccept() throws Exception {
        InOrder inOrder = inOrder(mRunnableMock);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        BottomSheetController bottomSheetController =
                TestThreadUtils.runOnUiThreadBlocking(this::initializeBottomSheet);
        AssistantCoordinator assistantCoordinator = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> new AssistantCoordinator(getActivity(), bottomSheetController,
                                getActivity().getTabObscuringHandler(),
                                /* overlayCoordinator= */ null,
                                /* keyboardCoordinatorDelegate= */ null,
                                /* bottomSheetDelegate= */ null));

        // Bottom sheet is shown in the BottomSheet when creating the AssistantCoordinator.
        ViewGroup bottomSheetContent =
                bottomSheetController.getBottomSheetViewForTesting().findViewById(
                        R.id.autofill_assistant);
        Assert.assertNotNull(bottomSheetContent);

        // Disable bottom sheet content animations. This is a workaround for http://crbug/943483.
        TestThreadUtils.runOnUiThreadBlocking(() -> bottomSheetContent.setLayoutTransition(null));

        // Show and check status message.
        String testStatusMessage = "test message";
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> assistantCoordinator.getModel().getHeaderModel().set(
                                AssistantHeaderModel.STATUS_MESSAGE, testStatusMessage));
        onView(withId(R.id.status_message))
                .check(matches(allOf(isDisplayed(), withText(testStatusMessage))));

        // Show scrim.
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> assistantCoordinator.getModel().getOverlayModel().set(
                                AssistantOverlayModel.STATE, AssistantOverlayState.FULL));
        View scrim = getActivity().getScrim();
        Assert.assertTrue(scrim.isShown());

        // TODO(crbug.com/806868): Fix test of actions carousel. This is currently broken as chips
        // are displayed in the reversed order in the actions carousel and calling
        // View#performClick() does not work as chips in the actions carousel are wrapped into a
        // FrameLayout that does not react to clicks.
        // testChips(inOrder, assistantCoordinator.getModel().getActionsModel(),
        //        assistantCoordinator.getBottomBarCoordinator().getActionsCoordinator());

        // Show movie details.
        String movieTitle = "testTitle";
        String descriptionLine1 = "This is a fancy line1";
        String descriptionLine2 = "This is a fancy line2";
        String descriptionLine3 = "This is a fancy line3";
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> assistantCoordinator.getModel().getDetailsModel().set(
                                AssistantDetailsModel.DETAILS,
                                new AssistantDetails(movieTitle, /* titleMaxLines = */ 1,
                                        /* imageUrl = */ "",
                                        /* imageAccessibilityHint = */ "",
                                        /* imageClickthroughData = */ null,
                                        /* showImage = */ false,
                                        /* totalPriceLabel = */ "",
                                        /* totalPrice = */ "", descriptionLine1, descriptionLine2,
                                        descriptionLine3,
                                        /* priceAttribution = */ "",
                                        /* userApprovalRequired= */ false,
                                        /* highlightTitle= */ false, /* highlightLine1= */
                                        false, /* highlightLine2 = */ false,
                                        /* highlightLine3 = */ false,
                                        /* animatePlaceholders= */ false)));
        onView(withId(R.id.details_title))
                .check(matches(allOf(withText(movieTitle), withEffectiveVisibility(VISIBLE))));
        onView(withId(R.id.details_line1))
                .check(matches(
                        allOf(withText(descriptionLine1), withEffectiveVisibility(VISIBLE))));
        onView(withId(R.id.details_line2))
                .check(matches(
                        allOf(withText(descriptionLine2), withEffectiveVisibility(VISIBLE))));
        onView(withId(R.id.details_line3))
                .check(matches(
                        allOf(withText(descriptionLine3), withEffectiveVisibility(VISIBLE))));

        // Progress bar must be shown.
        Assert.assertTrue(bottomSheetContent.findViewById(R.id.progress_bar).isShown());

        // Disable progress bar.
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> assistantCoordinator.getModel().getHeaderModel().set(
                                AssistantHeaderModel.PROGRESS_VISIBLE, false));
        Assert.assertFalse(bottomSheetContent.findViewById(R.id.progress_bar).isShown());

        // Show info box content.
        String infoBoxExplanation = "InfoBox explanation.";
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> assistantCoordinator.getModel().getInfoBoxModel().set(
                                AssistantInfoBoxModel.INFO_BOX,
                                new AssistantInfoBox(
                                        /* imagePath = */ "", infoBoxExplanation)));
        TextView infoBoxExplanationView =
                bottomSheetContent.findViewById(R.id.info_box_explanation);
        onView(is(infoBoxExplanationView)).check(matches(withText(infoBoxExplanation)));
    }

    private void testChips(InOrder inOrder, AssistantCarouselModel carouselModel,
            AssistantActionsCarouselCoordinator carouselCoordinator) {
        List<AssistantChip> chips = Arrays.asList(
                new AssistantChip(AssistantChip.Type.CHIP_ASSISTIVE, AssistantChip.Icon.NONE,
                        "chip 0",
                        /* disabled= */ false, /* sticky= */ false, "", () -> {/* do nothing */}),
                new AssistantChip(AssistantChip.Type.CHIP_ASSISTIVE, AssistantChip.Icon.NONE,
                        "chip 1",
                        /* disabled= */ false, /* sticky= */ false, "", mRunnableMock));
        TestThreadUtils.runOnUiThreadBlocking(() -> carouselModel.setChips(chips));
        RecyclerView chipsViewContainer = carouselCoordinator.getView();
        Assert.assertEquals(2, chipsViewContainer.getAdapter().getItemCount());

        // Choose the second chip.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { chipsViewContainer.getChildAt(1).performClick(); });
        inOrder.verify(mRunnableMock).run();
    }

    @Test
    @MediumTest
    public void testTooltipBubble() throws Exception {
        InOrder inOrder = inOrder(mRunnableMock);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(createMinimalCustomTabIntent());
        BottomSheetController bottomSheetController =
                TestThreadUtils.runOnUiThreadBlocking(this::initializeBottomSheet);
        AssistantCoordinator assistantCoordinator = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> new AssistantCoordinator(getActivity(), bottomSheetController,
                                getActivity().getTabObscuringHandler(),
                                /* overlayCoordinator= */ null,
                                /* keyboardCoordinatorDelegate= */ null,
                                /* bottomSheetDelegate= */ null));

        // Bottom sheet is shown in the BottomSheet when creating the AssistantCoordinator.
        ViewGroup bottomSheetContent =
                bottomSheetController.getBottomSheetViewForTesting().findViewById(
                        R.id.autofill_assistant);
        Assert.assertNotNull(bottomSheetContent);

        // Disable bottom sheet content animations. This is a workaround for http://crbug/943483.
        TestThreadUtils.runOnUiThreadBlocking(() -> bottomSheetContent.setLayoutTransition(null));

        // Show and check the bubble message.
        String testBubbleMessage = "Bubble message.";
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> assistantCoordinator.getModel().getHeaderModel().set(
                                AssistantHeaderModel.BUBBLE_MESSAGE, testBubbleMessage));

        // Bubbles are opened as popups and espresso needs to be instructed to not match views in
        // the main window's root.
        onView(withText(testBubbleMessage))
                .inRoot(withDecorView(not(getActivity().getWindow().getDecorView())))
                .check(matches(isDisplayed()));
    }
}

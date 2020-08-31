// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.action.ViewActions.scrollTo;
import static android.support.test.espresso.action.ViewActions.typeText;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.checkElementExists;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.fullyCovers;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.getAbsoluteBoundingRect;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.tapElement;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntil;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.matcher.ViewMatchers.Visibility;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientSettingsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ClientSettingsProto.IntegrationTestSettings;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto.Rectangle;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementReferenceProto;
import org.chromium.chrome.browser.autofill_assistant.proto.FocusElementProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextInputProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextInputProto.InputType;
import org.chromium.chrome.browser.autofill_assistant.proto.TextInputSectionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.UserFormSectionProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Tests autofill assistant with accessibility enabled.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantAccessibilityIntegrationTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/components/test/data/autofill_assistant/html/"
            + "autofill_assistant_target_website.html";

    private void runScript(AutofillAssistantTestScript script) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script),
                        (ClientSettingsProto) ClientSettingsProto.newBuilder()
                                .setIntegrationTestSettings(
                                        IntegrationTestSettings.newBuilder()
                                                .setDisableHeaderAnimations(true)
                                                .setDisableCarouselChangeAnimations(true))
                                .setTalkbackSheetSizeFraction(0.5f)
                                .build());
        startAutofillAssistant(mTestRule.getActivity(), testService);
    }

    @Before
    public void setUp() {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestRule.startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(),
                mTestRule.getTestServer().getURL(TEST_PAGE)));
        mTestRule.getActivity().getScrim().disableAnimationForTesting(true);
    }

    @After
    public void tearDown() {
        AccessibilityUtil.setAccessibilityEnabledForTesting(null);
    }

    @Test
    @MediumTest
    public void testBottomSheetHasRestrictedFixedHeight() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        // Show an element on top that should not be covered by the bottom sheet.
        ElementReferenceProto element = (ElementReferenceProto) ElementReferenceProto.newBuilder()
                                                .addSelectors("#touch_area_one")
                                                .build();
        ElementAreaProto elementArea =
                (ElementAreaProto) ElementAreaProto.newBuilder()
                        .addTouchable(Rectangle.newBuilder().addElements(element))
                        .addTouchable(Rectangle.newBuilder().addElements(
                                ElementReferenceProto.newBuilder().addSelectors(
                                        "#touch_area_four")))
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setFocusElement(FocusElementProto.newBuilder()
                                                  .setElement(element)
                                                  .setTouchableElementArea(elementArea))
                         .build());

        // Create enough additional sections to fill up more than the height of the screen.
        List<UserFormSectionProto> additionalSections = new ArrayList<>();
        for (int i = 1; i <= 20; ++i) {
            additionalSections.add(
                    (UserFormSectionProto) UserFormSectionProto.newBuilder()
                            .setTextInputSection(TextInputSectionProto.newBuilder().addInputFields(
                                    TextInputProto.newBuilder()
                                            .setHint("Text input " + i)
                                            .setClientMemoryKey("input_" + i)
                                            .setInputType(InputType.INPUT_TEXT)))
                            .setTitle("Title " + i)
                            .build());
        }
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .addAllAdditionalAppendedSections(additionalSections)
                                         .setRequestTermsAndConditions(false))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AccessibilityUtil.setAccessibilityEnabledForTesting(true);
        runScript(script);
        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());

        onView(withId(R.id.control_container)).check(matches(isCompletelyDisplayed()));
        onView(withText("Title 1")).check(matches(isDisplayed()));
        onView(withText("Title 20")).check(matches(not(isDisplayed())));
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_one"), is(true));
        tapElement(mTestRule, "touch_area_one");
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_one"));

        // Typing text will show the soft keyboard, leading to resize of the Chrome window.
        onView(withText("Title 1")).perform(scrollTo(), click());
        waitUntilViewMatchesCondition(withContentDescription("Text input 1"),
                withEffectiveVisibility(Visibility.VISIBLE));
        onView(withContentDescription("Text input 1"))
                .perform(scrollTo(), typeText("Hello World!"));
        onView(withId(R.id.control_container)).check(matches(isCompletelyDisplayed()));
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_four"), is(true));
        tapElement(mTestRule, "touch_area_four");
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_four"));
    }

    @Test
    @MediumTest
    public void testBottomSheetListensToAccessibilityChanges() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();

        // Show an element on top that may or may not be covered by the bottom sheet.
        ElementReferenceProto element = (ElementReferenceProto) ElementReferenceProto.newBuilder()
                                                .addSelectors("#touch_area_one")
                                                .build();
        ElementAreaProto elementArea =
                (ElementAreaProto) ElementAreaProto.newBuilder()
                        .addTouchable(Rectangle.newBuilder().addElements(element))
                        .addTouchable(Rectangle.newBuilder().addElements(
                                ElementReferenceProto.newBuilder().addSelectors(
                                        "#touch_area_four")))
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setFocusElement(FocusElementProto.newBuilder()
                                                  .setElement(element)
                                                  .setTouchableElementArea(elementArea))
                         .build());

        // Create enough additional sections to fill up more than the height of the screen.
        List<UserFormSectionProto> additionalSections = new ArrayList<>();
        for (int i = 1; i <= 20; ++i) {
            additionalSections.add(
                    (UserFormSectionProto) UserFormSectionProto.newBuilder()
                            .setTextInputSection(TextInputSectionProto.newBuilder().addInputFields(
                                    TextInputProto.newBuilder()
                                            .setHint("Text input " + i)
                                            .setClientMemoryKey("input_" + i)
                                            .setInputType(InputType.INPUT_TEXT)))
                            .setTitle("Title " + i)
                            .build());
        }
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .addAllAdditionalAppendedSections(additionalSections)
                                         .setRequestTermsAndConditions(false))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("autofill_assistant_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        runScript(script);
        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());

        // Without accessibility enabled, the bottom sheet fills the entire screen, tapping the
        // element is not possible.
        onView(withId(R.id.control_container)).check(matches(isCompletelyDisplayed()));
        onView(withText("Title 1")).check(matches(isDisplayed()));
        onView(withText("Title 20")).check(matches(not(isDisplayed())));
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_one"), is(true));
        onView(withId(R.id.bottom_sheet_content))
                .check(matches(fullyCovers(getAbsoluteBoundingRect(mTestRule, "touch_area_one"))));

        // Enabling accessibility restricts the height, the element can now be tapped and will be
        // removed.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> AccessibilityUtil.setAccessibilityEnabledForTesting(true));
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_one"), is(true));
        waitUntilViewMatchesCondition(withId(R.id.bottom_sheet_content),
                not(fullyCovers(getAbsoluteBoundingRect(mTestRule, "touch_area_one"))));
        tapElement(mTestRule, "touch_area_one");
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_one"));

        // Disabling accessibility again removes the height restriction so the bottom sheet will
        // fill the entire screen again, preventing the tap on the element.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> AccessibilityUtil.setAccessibilityEnabledForTesting(false));
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_four"), is(true));
        waitUntilViewMatchesCondition(withId(R.id.bottom_sheet_content),
                fullyCovers(getAbsoluteBoundingRect(mTestRule, "touch_area_four")));

        // Enabling accessibility again to make sure element can actually be removed.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> AccessibilityUtil.setAccessibilityEnabledForTesting(true));
        assertThat(checkElementExists(mTestRule.getWebContents(), "touch_area_four"), is(true));
        waitUntilViewMatchesCondition(withId(R.id.bottom_sheet_content),
                not(fullyCovers(getAbsoluteBoundingRect(mTestRule, "touch_area_four"))));
        tapElement(mTestRule, "touch_area_four");
        waitUntil(() -> !checkElementExists(mTestRule.getWebContents(), "touch_area_four"));
    }
}

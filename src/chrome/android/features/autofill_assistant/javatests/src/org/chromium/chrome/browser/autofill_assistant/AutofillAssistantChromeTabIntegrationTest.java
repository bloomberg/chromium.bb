// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntil;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilKeyboardMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewAssertionTrue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.support.test.InstrumentationRegistry;

import androidx.test.espresso.Espresso;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.ConfigureBottomSheetProto.PeekMode;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.StopProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TellProto;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.components.autofill_assistant.AutofillAssistantPreferencesUtil;
import org.chromium.components.autofill_assistant.R;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.test.util.UiRestriction;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * Tests autofill assistant in a normal Chrome tab.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantChromeTabIntegrationTest {
    private static final String TEST_PAGE_A = "autofill_assistant_target_website.html";
    private static final String TEST_PAGE_B = "form_target_website.html";

    private final ChromeTabbedActivityTestRule mTestRule = new ChromeTabbedActivityTestRule();
    private final AutofillAssistantChromeTabTestRule mTabTestRule =
            new AutofillAssistantChromeTabTestRule(mTestRule, TEST_PAGE_A);

    @Rule
    public final TestRule mRulesChain = RuleChain.outerRule(mTestRule).around(mTabTestRule);

    private ScrimCoordinator mScrimCoordinator;
    private OmniboxTestUtils mOmnibox;

    private String getURL(String page) {
        return mTabTestRule.getURL(page);
    }

    private void startAutofillAssistantOnTab(
            String pageToLoad, AutofillAssistantTestScript... scripts) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Arrays.asList(scripts));
        startAutofillAssistant(mTestRule.getActivity(), testService, getURL(pageToLoad));
    }

    @Before
    public void setUp() throws Exception {
        mScrimCoordinator =
                mTestRule.getActivity().getRootUiCoordinatorForTesting().getScrimCoordinator();
        mOmnibox = new OmniboxTestUtils(mTestRule.getActivity());
    }

    @Test
    @MediumTest
    // Restricted to phones due to https://crbug.com/429671
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void newTabButtonHidesAndRecoversAutofillAssistant() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Prompt")
                                            .setDisableForceExpandSheet(true)
                                            .addChoices(PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        startAutofillAssistantOnTab(TEST_PAGE_A, script);

        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        onView(is(mScrimCoordinator.getViewForTesting()))
                .check(matches(withEffectiveVisibility(Visibility.VISIBLE)));

        onView(withId(org.chromium.chrome.R.id.tab_switcher_button)).perform(click());
        waitUntilViewAssertionTrue(withText("Prompt"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);
        onView(is(mScrimCoordinator.getViewForTesting())).check(doesNotExist());

        Espresso.pressBack();
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        onView(is(mScrimCoordinator.getViewForTesting()))
                .check(matches(withEffectiveVisibility(Visibility.VISIBLE)));
    }

    @Test
    @MediumTest
    public void switchingTabHidesAutofillAssistant() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        int initialTabId =
                TabModelUtils.getCurrentTabId(mTestRule.getActivity().getCurrentTabModel());

        startAutofillAssistantOnTab(TEST_PAGE_A, script);

        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                mTestRule.getActivity(), getURL(TEST_PAGE_B), false);
        waitUntilViewAssertionTrue(withText("Prompt"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);

        ChromeTabUtils.switchTabInCurrentTabModel(mTestRule.getActivity(),
                TabModelUtils.getTabIndexById(
                        mTestRule.getActivity().getCurrentTabModel(), initialTabId));
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    public void closingTabResurfacesAutofillAssistant() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        startAutofillAssistantOnTab(TEST_PAGE_A, script);

        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());

        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                mTestRule.getActivity(), getURL(TEST_PAGE_B), false);
        waitUntilViewAssertionTrue(withText("Prompt"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);

        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mTestRule.getActivity());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    public void startingNewAutofillAssistantChangeTabResumeRunOnPreviousTab() {
        ArrayList<ActionProto> listA = new ArrayList<>();
        listA.add(ActionProto.newBuilder()
                          .setPrompt(PromptProto.newBuilder()
                                             .setMessage("Prompt A")
                                             .addChoices(PromptProto.Choice.newBuilder()))
                          .build());

        AutofillAssistantTestScript scriptA = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                listA);

        ArrayList<ActionProto> listB = new ArrayList<>();
        listB.add(ActionProto.newBuilder()
                          .setPrompt(PromptProto.newBuilder()
                                             .setMessage("Prompt B")
                                             .addChoices(PromptProto.Choice.newBuilder()))
                          .build());

        AutofillAssistantTestScript scriptB = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_B)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                listB);

        int initialTabId =
                TabModelUtils.getCurrentTabId(mTestRule.getActivity().getCurrentTabModel());

        startAutofillAssistantOnTab(TEST_PAGE_A, scriptA);

        waitUntilViewMatchesCondition(withText("Prompt A"), isCompletelyDisplayed());

        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                mTestRule.getActivity(), getURL(TEST_PAGE_B), false);
        waitUntilViewAssertionTrue(withText("Prompt A"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);

        startAutofillAssistantOnTab(TEST_PAGE_B, scriptB);
        waitUntilViewMatchesCondition(withText("Prompt B"), isCompletelyDisplayed());

        ChromeTabUtils.switchTabInCurrentTabModel(mTestRule.getActivity(),
                TabModelUtils.getTabIndexById(
                        mTestRule.getActivity().getCurrentTabModel(), initialTabId));
        waitUntilViewAssertionTrue(withText("Prompt B"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);
        waitUntilViewMatchesCondition(withText("Prompt A"), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    @FlakyTest(message = "https://crbug.com/1237330")
    public void switchingTabsRestoresBottomSheetState() {
        ArrayList<ActionProto> listA = new ArrayList<>();
        listA.add(ActionProto.newBuilder()
                          .setConfigureBottomSheet(
                                  org.chromium.chrome.browser.autofill_assistant.proto
                                          .ConfigureBottomSheetProto.newBuilder()
                                          .setPeekMode(PeekMode.HANDLE)
                                          .setExpand(false)
                                          .setCollapse(true)
                                          .setResizeTimeoutMs(1000))
                          .setActionDelayMs(500)
                          .build());
        listA.add(ActionProto.newBuilder()
                          .setPrompt(PromptProto.newBuilder()
                                             .setMessage("Prompt A")
                                             .setBrowseMode(true)
                                             .addChoices(PromptProto.Choice.newBuilder()))
                          .build());

        AutofillAssistantTestScript scriptA = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                listA);
        ArrayList<ActionProto> listB = new ArrayList<>();
        listB.add(ActionProto.newBuilder()
                          .setPrompt(PromptProto.newBuilder()
                                             .setMessage("Prompt B")
                                             .setBrowseMode(true)
                                             .addChoices(PromptProto.Choice.newBuilder()))
                          .build());

        AutofillAssistantTestScript scriptB = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_B)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                listB);

        int initialTabId =
                TabModelUtils.getCurrentTabId(mTestRule.getActivity().getCurrentTabModel());

        startAutofillAssistantOnTab(TEST_PAGE_A, scriptA);

        waitUntilViewMatchesCondition(withId(R.id.autofill_assistant), isDisplayed());
        waitUntilViewMatchesCondition(withText("Prompt A"), not(isDisplayed()));

        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                mTestRule.getActivity(), getURL(TEST_PAGE_B), false);
        waitUntilViewAssertionTrue(allOf(withText("Sticky"), isDescendantOfA(withId(R.id.header))),
                doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);

        startAutofillAssistantOnTab(TEST_PAGE_B, scriptB);
        waitUntilViewMatchesCondition(withText("Prompt B"), isCompletelyDisplayed());

        ChromeTabUtils.switchTabInCurrentTabModel(mTestRule.getActivity(),
                TabModelUtils.getTabIndexById(
                        mTestRule.getActivity().getCurrentTabModel(), initialTabId));
        waitUntilViewAssertionTrue(withText("Prompt B"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);
        waitUntilViewMatchesCondition(withId(R.id.autofill_assistant), isDisplayed());
        waitUntilViewMatchesCondition(withText("Prompt A"), not(isDisplayed()));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Flaky - https://crbug.com/1123958")
    public void switchTabBetweenDifferentPeekModes() {
        ArrayList<ActionProto> listA = new ArrayList<>();
        listA.add(ActionProto.newBuilder()
                          .setConfigureBottomSheet(
                                  org.chromium.chrome.browser.autofill_assistant.proto
                                          .ConfigureBottomSheetProto.newBuilder()
                                          .setPeekMode(PeekMode.HANDLE_HEADER)
                                          .setExpand(false)
                                          .setCollapse(true)
                                          .setResizeTimeoutMs(1000))
                          .setActionDelayMs(500)
                          .build());
        listA.add(ActionProto.newBuilder()
                          .setPrompt(PromptProto.newBuilder()
                                             .setMessage("Prompt message")
                                             .addChoices(PromptProto.Choice.newBuilder().setChip(
                                                     ChipProto.newBuilder()
                                                             .setText("Sticky")
                                                             .setSticky(true)
                                                             .setType(org.chromium.chrome.browser
                                                                              .autofill_assistant
                                                                              .proto.ChipType
                                                                              .HIGHLIGHTED_ACTION)))
                                             .setAllowInterrupt(false)
                                             .setDisableForceExpandSheet(true)
                                  /*.setBrowseMode(true)*/)
                          .build());

        AutofillAssistantTestScript scriptA = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                listA);
        ArrayList<ActionProto> listB = new ArrayList<>();
        listB.add(ActionProto.newBuilder()
                          .setPrompt(PromptProto.newBuilder()
                                             .setMessage("Prompt B")
                                             .setBrowseMode(true)
                                             .addChoices(PromptProto.Choice.newBuilder()))
                          .build());

        AutofillAssistantTestScript scriptB = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_B)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                listB);

        int initialTabId =
                TabModelUtils.getCurrentTabId(mTestRule.getActivity().getCurrentTabModel());

        startAutofillAssistantOnTab(TEST_PAGE_A, scriptA);
        waitUntilViewMatchesCondition(
                allOf(withText("Sticky"), isDescendantOfA(withId(R.id.header))),
                isCompletelyDisplayed());

        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                mTestRule.getActivity(), getURL(TEST_PAGE_B), false);
        waitUntilViewAssertionTrue(allOf(withText("Sticky"), isDescendantOfA(withId(R.id.header))),
                doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);

        startAutofillAssistantOnTab(TEST_PAGE_B, scriptB);
        waitUntilViewMatchesCondition(withText("Prompt B"), isCompletelyDisplayed());

        Espresso.pressBack();
        waitUntilViewMatchesCondition(
                withId(R.id.status_message), allOf(withText("Prompt B"), not(isDisplayed())));
        onView(withId(R.id.autofill_assistant)).check(matches(isDisplayed()));

        int secondTabId =
                TabModelUtils.getCurrentTabId(mTestRule.getActivity().getCurrentTabModel());

        ChromeTabUtils.switchTabInCurrentTabModel(mTestRule.getActivity(),
                TabModelUtils.getTabIndexById(
                        mTestRule.getActivity().getCurrentTabModel(), initialTabId));
        waitUntilViewAssertionTrue(withText("Prompt B"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);
        waitUntilViewMatchesCondition(
                allOf(withText("Sticky"), isDescendantOfA(withId(R.id.header))),
                isCompletelyDisplayed());

        ChromeTabUtils.switchTabInCurrentTabModel(mTestRule.getActivity(),
                TabModelUtils.getTabIndexById(
                        mTestRule.getActivity().getCurrentTabModel(), secondTabId));
        waitUntilViewMatchesCondition(withId(R.id.autofill_assistant), isDisplayed());
        onView(allOf(withId(R.id.status_message), withText("Prompt B")))
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Flaky - https://crbug.com/1115681")
    public void startingNewAutofillAssistantCloseTabResumesRunOnPreviousTab() {
        ArrayList<ActionProto> listA = new ArrayList<>();
        listA.add(ActionProto.newBuilder()
                          .setPrompt(PromptProto.newBuilder()
                                             .setMessage("Prompt A")
                                             .addChoices(PromptProto.Choice.newBuilder()))
                          .build());

        AutofillAssistantTestScript scriptA = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                listA);

        ArrayList<ActionProto> listB = new ArrayList<>();
        listB.add(ActionProto.newBuilder()
                          .setPrompt(PromptProto.newBuilder()
                                             .setMessage("Prompt B")
                                             .addChoices(PromptProto.Choice.newBuilder()))
                          .build());

        AutofillAssistantTestScript scriptB = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_B)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                listB);
        startAutofillAssistantOnTab(TEST_PAGE_A, scriptA);

        waitUntilViewMatchesCondition(withText("Prompt A"), isCompletelyDisplayed());

        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                mTestRule.getActivity(), getURL(TEST_PAGE_B), false);
        waitUntilViewAssertionTrue(withText("Prompt A"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);

        startAutofillAssistantOnTab(TEST_PAGE_B, scriptB);
        waitUntilViewMatchesCondition(withText("Prompt B"), isCompletelyDisplayed());

        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mTestRule.getActivity());
        waitUntilViewAssertionTrue(withText("Prompt B"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);
        waitUntilViewMatchesCondition(withText("Prompt A"), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1272997")
    public void interactingWithLocationBarHidesAutofillAssistant() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        startAutofillAssistantOnTab(TEST_PAGE_A, script);

        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(is(mScrimCoordinator.getViewForTesting()),
                withEffectiveVisibility(Visibility.VISIBLE));

        // Clicking location bar hides UI and shows the keyboard.
        mOmnibox.requestFocus();
        waitUntilViewMatchesCondition(withText("Prompt"), not(isCompletelyDisplayed()));

        // Closing keyboard brings it back.
        Espresso.closeSoftKeyboard();
        Espresso.pressBack();
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(is(mScrimCoordinator.getViewForTesting()),
                withEffectiveVisibility(Visibility.VISIBLE));

        // Committing URL shows error.
        mOmnibox.typeText(getURL(TEST_PAGE_B), true);
        waitUntilViewMatchesCondition(withText(containsString("Sorry")), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Flaky - https://crbug.com/1157506")
    public void interactingWithLocationBarDoesNotShowHiddenScrim() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Browse")
                                            .setBrowseMode(true)
                                            .addChoices(PromptProto.Choice.newBuilder().setChip(
                                                    ChipProto.newBuilder()
                                                            .setType(ChipType.HIGHLIGHTED_ACTION)
                                                            .setText("Continue"))))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        startAutofillAssistantOnTab(TEST_PAGE_A, script);

        // Browse mode hides the Scrim.
        waitUntilViewMatchesCondition(withText("Browse"), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(is(mScrimCoordinator.getViewForTesting()),
                not(withEffectiveVisibility(Visibility.VISIBLE)));

        // Clicking location bar hides UI and shows the keyboard.
        onView(withId(org.chromium.chrome.R.id.url_bar)).perform(click());
        waitUntilViewMatchesCondition(withText("Browse"), not(isDisplayed()));
        waitUntilKeyboardMatchesCondition(mTestRule, /* isShowing= */ true);

        // Closing keyboard brings back the UI but does not restore the Scrim.
        Espresso.pressBack();
        waitUntilViewMatchesCondition(withText("Browse"), isCompletelyDisplayed());
        waitUntil(() -> mScrimCoordinator.getViewForTesting() == null);

        // Running the next action brings back the Scrim.
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(is(mScrimCoordinator.getViewForTesting()),
                withEffectiveVisibility(Visibility.VISIBLE));
    }

    @Test
    @MediumTest
    public void switchingBackToTabWithStoppedAutofillAssistantShowsErrorMessage() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setTell(TellProto.newBuilder().setMessage("Shutdown"))
                         .build());
        list.add(ActionProto.newBuilder().setStop(StopProto.newBuilder()).build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath(TEST_PAGE_A)
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        startAutofillAssistantOnTab(TEST_PAGE_A, script);

        waitUntilViewMatchesCondition(withText("Shutdown"), isCompletelyDisplayed());

        ChromeTabUtils.fullyLoadUrlInNewTab(InstrumentationRegistry.getInstrumentation(),
                mTestRule.getActivity(), getURL(TEST_PAGE_B), false);
        waitUntilViewAssertionTrue(withText("Shutdown"), doesNotExist(), DEFAULT_MAX_TIME_TO_POLL);

        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mTestRule.getActivity());
        waitUntilViewMatchesCondition(withText("Shutdown"), isCompletelyDisplayed());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Flaky - https://crbug.com/1157339")
    // Restricted to phones due to https://crbug.com/429671
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void newTabButtonHidesAndRecoversOnboarding() {
        // Onboarding has not been accepted.
        AutofillAssistantPreferencesUtil.setInitialPreferences(false);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntil(
                ()
                        -> ChromeTabUtils.getUrlOnUiThread(mTestRule.getActivity().getActivityTab())
                                   .getSpec()
                                   .equals(getURL(TEST_PAGE_A)));
        waitUntilViewMatchesCondition(withId(R.id.button_init_ok), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(is(mScrimCoordinator.getViewForTesting()),
                withEffectiveVisibility(Visibility.VISIBLE));

        onView(withId(org.chromium.chrome.R.id.tab_switcher_button)).perform(click());
        waitUntilViewMatchesCondition(withId(R.id.button_init_ok), not(isDisplayed()));
        onView(is(mScrimCoordinator.getViewForTesting())).check(doesNotExist());

        Espresso.pressBack();
        waitUntilViewMatchesCondition(withId(R.id.button_init_ok), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(is(mScrimCoordinator.getViewForTesting()),
                withEffectiveVisibility(Visibility.VISIBLE));
    }

    @Test
    @MediumTest
    public void interactingWithLocationBarHidesOnboarding() {
        // Onboarding has not been accepted.
        AutofillAssistantPreferencesUtil.setInitialPreferences(false);
        startAutofillAssistantOnTab(TEST_PAGE_A);

        waitUntil(
                ()
                        -> ChromeTabUtils.getUrlOnUiThread(mTestRule.getActivity().getActivityTab())
                                   .getSpec()
                                   .equals(getURL(TEST_PAGE_A)));
        waitUntilViewMatchesCondition(withId(R.id.button_init_ok), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(is(mScrimCoordinator.getViewForTesting()),
                withEffectiveVisibility(Visibility.VISIBLE));

        // Clicking location bar hides UI and shows the keyboard.
        mOmnibox.requestFocus();
        waitUntilViewMatchesCondition(withId(R.id.button_init_ok), not(isDisplayed()));

        // Closing keyboard brings it back.
        Espresso.closeSoftKeyboard();
        Espresso.pressBack();
        waitUntilViewMatchesCondition(withId(R.id.button_init_ok), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(is(mScrimCoordinator.getViewForTesting()),
                withEffectiveVisibility(Visibility.VISIBLE));
    }
}

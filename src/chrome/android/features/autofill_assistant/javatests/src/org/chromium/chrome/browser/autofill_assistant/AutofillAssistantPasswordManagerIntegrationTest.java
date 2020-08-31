// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.is;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.getElementValue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementReferenceProto;
import org.chromium.chrome.browser.autofill_assistant.proto.GeneratePasswordForFormFieldProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SetFormFieldValueProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordChangeLauncher;
import org.chromium.chrome.browser.password_manager.PasswordManagerClientBridgeForTesting;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.Collections;

/**
 * Integration tests for password change flows.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantPasswordManagerIntegrationTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/components/test/data/autofill_assistant/html/"
            + "form_target_website.html";

    private WebContents getWebContents() {
        return mTestRule.getWebContents();
    }

    @Before
    public void setUp() throws Exception {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestRule.startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(),
                mTestRule.getTestServer().getURL(TEST_PAGE)));
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> PasswordManagerClientBridgeForTesting.setLeakDialogWasShownForTesting(
                                getWebContents(), true));
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> PasswordManagerClientBridgeForTesting.setLeakDialogWasShownForTesting(
                                getWebContents(), false));
    }

    /**
     * Helper function to start a password change flow.
     */
    private void startPasswordChangeFlow(AutofillAssistantTestScript script, String username) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        testService.scheduleForInjection();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> PasswordChangeLauncher.start(getWebContents().getTopLevelNativeWindow(),
                                getWebContents().getLastCommittedUrl(), username));
    }

    /**
     * Run a password change flow (fill a form with username, old password, new
     * password).
     */
    @Test
    @MediumTest
    public void testPasswordChangeFlow() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setSetFormValue(
                                SetFormFieldValueProto.newBuilder()
                                        .addValue(SetFormFieldValueProto.KeyPress.newBuilder()
                                                          .setUseUsername(true))
                                        .setElement(ElementReferenceProto.newBuilder().addSelectors(
                                                "#username")))
                        .build());
        // TODO(crbug.com/1057608): Implement Android wrapper for PasswordStore to add a
        // step and
        // verification for current password filling.
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setGeneratePasswordForFormField(
                                GeneratePasswordForFormFieldProto.newBuilder()
                                        .setMemoryKey("memory-key")
                                        .setElement(ElementReferenceProto.newBuilder().addSelectors(
                                                "#new-password")))
                        .build());

        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setSetFormValue(
                                SetFormFieldValueProto.newBuilder()
                                        .addValue(SetFormFieldValueProto.KeyPress.newBuilder()
                                                          .setClientMemoryKey("memory-key"))
                                        .setElement(ElementReferenceProto.newBuilder().addSelectors(
                                                "#new-password")))
                        .build());
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setSetFormValue(
                                SetFormFieldValueProto.newBuilder()
                                        .addValue(SetFormFieldValueProto.KeyPress.newBuilder()
                                                          .setClientMemoryKey("memory-key"))
                                        .setElement(ElementReferenceProto.newBuilder().addSelectors(
                                                "#password-conf")))
                        .build());

        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Password generation")))
                        .build(),
                list);

        String username = "test_username";
        startPasswordChangeFlow(script, username);

        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        assertThat(getElementValue(getWebContents(), "username"), is(username));
        String password = getElementValue(getWebContents(), "new-password");
        String confirmation_password = getElementValue(getWebContents(), "password-conf");
        assertThat(password.length(), greaterThan(0));
        assertThat(password, is(confirmation_password));
    }
}

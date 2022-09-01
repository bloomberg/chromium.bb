// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interaction_sequence_browser_util.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/interaction/interaction_test_util_mac.h"
#endif

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInteractionSequenceBrowserUtilTestId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kInteractionSequenceBrowserUtilTestId2);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(
    kInteractionSequenceBrowserUtilCustomEventId);
}  // namespace

class SettingsInteractiveUiTest : public InProcessBrowserTest {
 public:
  SettingsInteractiveUiTest() = default;
  ~SettingsInteractiveUiTest() override = default;
  SettingsInteractiveUiTest(const SettingsInteractiveUiTest&) = delete;
  void operator=(const SettingsInteractiveUiTest&) = delete;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();
  }

  // Helper function to wait for element in DOM visible.
  // This function will be implemented by a test util framework later.
  auto WaitFor(const InteractionSequenceBrowserUtil::DeepQuery& element,
               const ui::InteractionSequence::StepType type =
                   ui::InteractionSequence::StepType::kShown) {
    return ui::InteractionSequence::StepBuilder()
        .SetElementID(kInteractionSequenceBrowserUtilTestId)
        .SetStartCallback(base::BindLambdaForTesting(
            // FIXME: type has to be copied.
            [&, type](ui::InteractionSequence*,
                      ui::TrackedElement* tracked_elem) {
              auto* util = tracked_elem->AsA<TrackedElementWebPage>()->owner();

              InteractionSequenceBrowserUtil::StateChange state_change;
              state_change.where = element;
              if (type == ui::InteractionSequence::StepType::kShown) {
                state_change.test_function =
                    "(el, err) => el && el.offsetParent !== null";
              } else if (type == ui::InteractionSequence::StepType::kHidden) {
                state_change.test_function =
                    "(el, err) => !el || el.offsetParent === null";
              }
              state_change.event = kInteractionSequenceBrowserUtilCustomEventId;
              util->SendEventOnStateChange(state_change);
            }))
        .Build();
  }

  // Click has to set after a WaitFor step, or reset SetType.
  // This function will be implemented by a test util framework later.
  auto Click(const InteractionSequenceBrowserUtil::DeepQuery& element) {
    return ui::InteractionSequence::StepBuilder()
        .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                 kInteractionSequenceBrowserUtilCustomEventId)
        .SetElementID(kInteractionSequenceBrowserUtilTestId)
        .SetStartCallback(base::BindLambdaForTesting(
            [&](ui::InteractionSequence*, ui::TrackedElement* tracked_elem) {
              auto* util = tracked_elem->AsA<TrackedElementWebPage>()->owner();
              util->EvaluateAt(element, "el => el.click()");
            }))
        .Build();
  }
};

IN_PROC_BROWSER_TEST_F(SettingsInteractiveUiTest,
                       CheckQuestionMarkIsPresentUnderCookiesAndSiteData) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL cookie_setting_url("chrome://settings/privacy");
  const InteractionSequenceBrowserUtil::DeepQuery cookies_link_row = {
      "settings-ui", "settings-main", "settings-basic-page",
      "settings-privacy-page", "cr-link-row#cookiesLinkRow"};
  const InteractionSequenceBrowserUtil::DeepQuery
      cookies_setting_page_help_icon = {
          "settings-ui",
          "settings-main",
          "settings-basic-page",
          "settings-privacy-page",
          "settings-subpage",
          "div#headerLine cr-icon-button[iron-icon='cr:help-outline']"};

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
  util->LoadPage(cookie_setting_url);
  auto util2 = InteractionSequenceBrowserUtil::ForNextTabInContext(
      browser()->window()->GetElementContext(),
      kInteractionSequenceBrowserUtilTestId2);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          // Click on 'Cookies and other site data'.
          .AddStep(WaitFor(cookies_link_row))
          .AddStep(Click(cookies_link_row))
          // Click on the "?" mark (help icon) on top right corner.
          .AddStep(WaitFor(cookies_setting_page_help_icon))
          .AddStep(Click(cookies_setting_page_help_icon))
          // Verify the new page opened.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kInteractionSequenceBrowserUtilTestId2)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             auto* util =
                                 element->AsA<TrackedElementWebPage>()->owner();
                             auto* const contents = util->web_contents();
                             EXPECT_EQ(chrome::kCookiesSettingsHelpCenterURL,
                                       contents->GetURL());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

class ThemeSettingsInteractiveUiTest : public SettingsInteractiveUiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(::switches::kInstallAutogeneratedTheme,
                                    "121,0,0");
  }
};

IN_PROC_BROWSER_TEST_F(ThemeSettingsInteractiveUiTest,
                       CheckChromeThemeCanBeResetToDefault) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::CompletedCallback, completed);
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);

  const GURL appearance_setting_url("chrome://settings/appearance");
  const InteractionSequenceBrowserUtil::DeepQuery reset_to_default_btn = {
      "settings-ui", "settings-main", "settings-basic-page",
      "settings-appearance-page", "cr-button#useDefault"};

  auto util = InteractionSequenceBrowserUtil::ForExistingTabInBrowser(
      browser(), kInteractionSequenceBrowserUtilTestId);
  util->LoadPage(appearance_setting_url);

  auto sequence =
      ui::InteractionSequence::Builder()
          .SetCompletedCallback(completed.Get())
          .SetAbortedCallback(aborted.Get())
          .SetContext(browser()->window()->GetElementContext())
          // Verify the current theme is not set as default.
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*,
                               ui::TrackedElement* element) {
                             auto* theme_service =
                                 ThemeServiceFactory::GetForProfile(
                                     browser()->profile());
                             ASSERT_FALSE(theme_service->UsingDefaultTheme());
                           }))
                       .Build())
          // Click on "Reset to default" button.
          .AddStep(WaitFor(reset_to_default_btn))
          .AddStep(Click(reset_to_default_btn))
          // Verify the theme in use.
          .AddStep(WaitFor(reset_to_default_btn,
                           ui::InteractionSequence::StepType::kHidden))
          .AddStep(ui::InteractionSequence::StepBuilder()
                       .SetType(ui::InteractionSequence::StepType::kCustomEvent,
                                kInteractionSequenceBrowserUtilCustomEventId)
                       .SetElementID(kInteractionSequenceBrowserUtilTestId)
                       .SetStartCallback(base::BindLambdaForTesting(
                           [&](ui::InteractionSequence*, ui::TrackedElement*) {
                             auto* theme_service =
                                 ThemeServiceFactory::GetForProfile(
                                     browser()->profile());
                             EXPECT_TRUE(theme_service->UsingDefaultTheme());
                           }))
                       .Build())
          .Build();

  EXPECT_CALL_IN_SCOPE(completed, Run, sequence->RunSynchronouslyForTesting());
}

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/win/scoped_variant.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/accessibility/uia_accessibility_event_waiter.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

namespace autofill {

class AutofillAccessibilityWinBrowserTest : public InProcessBrowserTest {
 public:
  AutofillAccessibilityWinBrowserTest() = default;

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    content::WebContents* web_contents = GetWebContents();
    web_contents->SetAccessibilityMode(ui::kAXModeComplete);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableExperimentalUIAutomation);
  }

  content::WebContents* GetWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  HWND GetWebPageHwnd() const {
    return browser()
        ->window()
        ->GetNativeWindow()
        ->GetHost()
        ->GetAcceleratedWidget();
  }

  // Show drop down based on the element id.
  void ShowDropdown(const std::string& field_id) {
    std::string js("document.getElementById('" + field_id + "').focus();");
    ASSERT_TRUE(ExecuteScript(GetWebContents(), js));
    SendKeyToPage(GetWebContents(), ui::DomKey::ARROW_DOWN);
  }

  void SendKeyToPage(content::WebContents* web_contents, const ui::DomKey key) {
    ui::KeyboardCode key_code = ui::NonPrintableDomKeyToKeyboardCode(key);
    ui::DomCode code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
    SimulateKeyPress(web_contents, key, code, key_code, false, false, false,
                     false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillAccessibilityWinBrowserTest);
};

IN_PROC_BROWSER_TEST_F(AutofillAccessibilityWinBrowserTest,
                       AutofillPopupControllerFor) {
  content::AccessibilityNotificationWaiter waiter(
      GetWebContents(), ui::kAXModeComplete, ax::mojom::Event::kLoadComplete);
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/accessibility/input_datalist.html"));
  waiter.WaitForNotification();

  base::win::ScopedVariant result_variant;

  content::FindAccessibilityNodeCriteria find_criteria;
  find_criteria.role = ax::mojom::Role::kTextFieldWithComboBox;

  // The autofill popup of the form input element has not shown yet. The form
  // input element is the controller for the checkbox as indicated by the form
  // input element's |aria-controls| attribute.
  content::UiaGetPropertyValueVtArrayVtUnknownValidate(
      UIA_ControllerForPropertyId,
      FindAccessibilityNode(GetWebContents(), find_criteria), {"checkbox"});

  UiaAccessibilityWaiterInfo info = {
      GetWebPageHwnd(), base::ASCIIToUTF16("combobox"),
      base::ASCIIToUTF16("input"), ax::mojom::Event::kControlsChanged};

  std::unique_ptr<UiaAccessibilityEventWaiter> control_waiter =
      std::make_unique<UiaAccessibilityEventWaiter>(info);
  // Show popup and wait for UIA_ControllerForPropertyId event.
  ShowDropdown("datalist");
  control_waiter->Wait();

  // The focus should remain on the input element.
  EXPECT_EQ(content::GetFocusedAccessibilityNodeInfo(GetWebContents()).role,
            ax::mojom::Role::kTextFieldWithComboBox);

  // The autofill popup of the form input element is showing. The form input
  // element is the controller for the checkbox and autofill popup as
  // indicated by the form input element's |aria-controls| attribute and the
  // existing popup.
  content::UiaGetPropertyValueVtArrayVtUnknownValidate(
      UIA_ControllerForPropertyId,
      FindAccessibilityNode(GetWebContents(), find_criteria),
      {"checkbox", "Autofill"});

  control_waiter = std::make_unique<UiaAccessibilityEventWaiter>(info);
  // Hide popup and wait for UIA_ControllerForPropertyId event.
  SendKeyToPage(GetWebContents(), ui::DomKey::TAB);
  control_waiter->Wait();

  // The autofill popup of the form input element is hidden. The form
  // input element is the controller for the checkbox as indicated by the form
  // input element's |aria-controls| attribute.
  content::UiaGetPropertyValueVtArrayVtUnknownValidate(
      UIA_ControllerForPropertyId,
      FindAccessibilityNode(GetWebContents(), find_criteria), {"checkbox"});
}

}  // namespace autofill

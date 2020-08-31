// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

class ExtensionBlockedActionsBubbleTest
    : public SupportsTestDialog<extensions::ExtensionBrowserTest> {
 public:
  ExtensionBlockedActionsBubbleTest();
  ~ExtensionBlockedActionsBubbleTest() override;

  void SetUpOnMainThread() override;
  void ShowUi(const std::string& name) override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::AutoReset<bool> disable_toolbar_animations_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionBlockedActionsBubbleTest);
};

ExtensionBlockedActionsBubbleTest::ExtensionBlockedActionsBubbleTest()
    : disable_toolbar_animations_(
          &ToolbarActionsBar::disable_animations_for_testing_,
          true) {
  // This code path only works for the old toolbar. The new toolbar is
  // exercised in extensions_menu_view_browsertest.cc.
  scoped_feature_list_.InitAndDisableFeature(features::kExtensionsToolbarMenu);
}
ExtensionBlockedActionsBubbleTest::~ExtensionBlockedActionsBubbleTest() =
    default;

void ExtensionBlockedActionsBubbleTest::SetUpOnMainThread() {
  extensions::ExtensionBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
}

void ExtensionBlockedActionsBubbleTest::ShowUi(const std::string& name) {
  ASSERT_TRUE(embedded_test_server()->Start());

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Runs Script Everywhere",
           "description": "An extension that runs script everywhere",
           "manifest_version": 2,
           "version": "0.1",
           "content_scripts": [{
             "matches": ["*://*/*"],
             "js": ["script.js"],
             "run_at": "document_start"
           }]
         })");
  test_dir.WriteFile(FILE_PATH_LITERAL("script.js"),
                     "console.log('injected!');");

  const extensions::Extension* extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  extensions::ScriptingPermissionsModifier(profile(),
                                           base::WrapRefCounted(extension))
      .SetWithholdHostPermissions(true);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  {
    content::TestNavigationObserver observer(tab);
    GURL url = embedded_test_server()->GetURL("example.com", "/title1.html");
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  ToolbarActionsBar* const toolbar_actions_bar =
      ToolbarActionsBar::FromBrowserWindow(browser()->window());
  ASSERT_EQ(1u, toolbar_actions_bar->GetActions().size());
  auto* view_controller = static_cast<ExtensionActionViewController*>(
      toolbar_actions_bar->GetActions()[0]);
  EXPECT_TRUE(view_controller->HasBeenBlockedForTesting(tab));

  ExtensionActionTestHelper::Create(browser())->Press(0);

  EXPECT_TRUE(toolbar_actions_bar->is_showing_bubble());
}

IN_PROC_BROWSER_TEST_F(ExtensionBlockedActionsBubbleTest,
                       InvokeUi_ExtensionBlockedActionsBubble) {
  ShowAndVerifyUi();
}

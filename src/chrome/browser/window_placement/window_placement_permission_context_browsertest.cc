// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/display/screen_base.h"
#include "ui/display/test/scoped_screen_override.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/shell.h"
#include "ui/display/test/display_manager_test_api.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

constexpr char kGetScreens[] = R"(
  (async () => {
    try {
      const screenDetails = await self.getScreenDetails();
    } catch {
      return 'error';
    }
    return (await navigator.permissions.query({name:'window-placement'})).state;
  })();
)";

constexpr char kCheckPermission[] = R"(
  (async () => {
    return (await navigator.permissions.query({name:'window-placement'})).state;
  })();
)";

// Tests of WindowPlacementPermissionContext behavior.
class WindowPlacementPermissionContextTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kEnableBlinkFeatures, "WindowPlacement");
  }

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");

    // Window placement features are only available on secure contexts, and so
    // we need to create an HTTPS test server here to serve those pages rather
    // than using the default embedded_test_server().
    https_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    // Support sites like a.test, b.test, c.test etc
    https_test_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_test_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    net::test_server::RegisterDefaultHandlers(https_test_server_.get());
    content::SetupCrossSiteRedirector(https_test_server_.get());
    ASSERT_TRUE(https_test_server_->Start());
  }

  // Awaits expiry of the navigator.userActivation signal on the active tab.
  void WaitForUserActivationExpiry() {
    const std::string await_activation_expiry_script = R"(
      (async () => {
        while (navigator.userActivation.isActive)
          await new Promise(resolve => setTimeout(resolve, 1000));
        return navigator.userActivation.isActive;
      })();
    )";
    auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(false, EvalJs(tab, await_activation_expiry_script,
                            content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    EXPECT_FALSE(tab->HasRecentInteractiveInputEvent());
  }

  net::EmbeddedTestServer* https_test_server() {
    return https_test_server_.get();
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
};

class MultiscreenWindowPlacementPermissionContextTest
    : public WindowPlacementPermissionContextTest {
 public:
  void SetScreenInstance() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Use the default, see SetUpOnMainThread.
    WindowPlacementPermissionContextTest::SetScreenInstance();
#else
    screen_override_.emplace(&screen_);
    screen_.display_list().AddDisplay({1, gfx::Rect(100, 100, 801, 802)},
                                      display::DisplayList::Type::PRIMARY);
    screen_.display_list().AddDisplay({2, gfx::Rect(901, 100, 802, 802)},
                                      display::DisplayList::Type::NOT_PRIMARY);
    ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // This has to happen later than SetScreenInstance as the ash shell
    // does not exist yet.
    display::test::DisplayManagerTestApi(ash::Shell::Get()->display_manager())
        .UpdateDisplay("100+100-801x802,901+100-802x802");
    ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
#endif

    WindowPlacementPermissionContextTest::SetUpOnMainThread();
  }

 private:
  display::ScreenBase screen_;
  absl::optional<display::test::ScopedScreenOverride> screen_override_;
};

// Tests gesture requirements (a gesture is only needed to prompt the user).
IN_PROC_BROWSER_TEST_F(WindowPlacementPermissionContextTest, GestureToPrompt) {
  const GURL url(https_test_server()->GetURL("a.test", "/empty.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Auto-dismiss the permission request, iff the prompt is shown.
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  // Calling getScreenDetails() without a gesture or pre-existing permission
  // will not prompt the user, and leaves the permission in the default "prompt"
  // state.
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());
  EXPECT_EQ("error",
            EvalJs(tab, kGetScreens, content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ("prompt", EvalJs(tab, kCheckPermission,
                             content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Calling getScreenDetails() with a gesture will show the prompt, and
  // auto-accept.
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());
  EXPECT_EQ("granted", EvalJs(tab, kGetScreens));
  EXPECT_TRUE(tab->GetMainFrame()->HasTransientUserActivation());

  // Calling getScreenDetails() without a gesture, but with pre-existing
  // permission, will succeed, since it does not need to prompt the user.
  WaitForUserActivationExpiry();
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());
  EXPECT_EQ("granted",
            EvalJs(tab, kGetScreens, content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());
}

// Tests user activation after dimissing and denying the permission request.
IN_PROC_BROWSER_TEST_F(WindowPlacementPermissionContextTest, DismissAndDeny) {
  const GURL url(https_test_server()->GetURL("a.test", "/empty.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);

  // Dismiss the prompt after activation expires, expect no activation.
  ExecuteScriptAsync(tab, "getScreenDetails()");
  WaitForUserActivationExpiry();
  ASSERT_TRUE(permission_request_manager->IsRequestInProgress());
  permission_request_manager->Dismiss();
  EXPECT_EQ("prompt", EvalJs(tab, kCheckPermission,
                             content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());

  // Deny the prompt after activation expires, expect no activation.
  ExecuteScriptAsync(tab, "getScreenDetails()");
  WaitForUserActivationExpiry();
  ASSERT_TRUE(permission_request_manager->IsRequestInProgress());
  permission_request_manager->Deny();
  EXPECT_EQ("denied", EvalJs(tab, kCheckPermission,
                             content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());
}

// Tests user activation after accepting the permission request.
IN_PROC_BROWSER_TEST_F(WindowPlacementPermissionContextTest, Accept) {
  const GURL url(https_test_server()->GetURL("a.test", "/empty.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);

  // Accept the prompt after activation expires, expect an activation signal.
  ExecuteScriptAsync(tab, "getScreenDetails()");
  WaitForUserActivationExpiry();
  ASSERT_TRUE(permission_request_manager->IsRequestInProgress());
  permission_request_manager->Accept();
  EXPECT_EQ("granted", EvalJs(tab, kCheckPermission,
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(tab->GetMainFrame()->HasTransientUserActivation());
}

IN_PROC_BROWSER_TEST_F(WindowPlacementPermissionContextTest,
                       IFrameSameOriginAllow) {
  const GURL url(https_test_server()->GetURL("a.test", "/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* child = ChildFrameAt(tab->GetMainFrame(), 0);
  ASSERT_TRUE(child);
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());
  EXPECT_FALSE(child->GetMainFrame()->HasTransientUserActivation());

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);

  // Accept the prompt after activation expires, expect an activation signal.
  ExecuteScriptAsync(child, "getScreenDetails()");
  WaitForUserActivationExpiry();
  ASSERT_TRUE(permission_request_manager->IsRequestInProgress());
  permission_request_manager->Accept();
  EXPECT_EQ("granted", EvalJs(child, kCheckPermission,
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(tab->GetMainFrame()->HasTransientUserActivation());
  EXPECT_TRUE(child->GetMainFrame()->HasTransientUserActivation());
}

IN_PROC_BROWSER_TEST_F(WindowPlacementPermissionContextTest,
                       IFrameCrossOriginDeny) {
  const GURL url(https_test_server()->GetURL("a.test", "/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL subframe_url(https_test_server()->GetURL("b.test", "/title1.html"));
  content::NavigateIframeToURL(tab, /*iframe_id=*/"test", subframe_url);

  content::RenderFrameHost* child = ChildFrameAt(tab->GetMainFrame(), 0);
  ASSERT_TRUE(child);
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());
  EXPECT_FALSE(child->GetMainFrame()->HasTransientUserActivation());

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);

  // PermissionRequestManager will accept any window placement permission
  // dialogs that appear. However, the window-placement permission is not
  // explicitly allowed on the iframe, so requests made by the child frame will
  // be automatically denied before a prompt might be issued.
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_EQ("error", EvalJs(child, kGetScreens));
  EXPECT_EQ("denied", EvalJs(child, kCheckPermission,
                             content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

IN_PROC_BROWSER_TEST_F(WindowPlacementPermissionContextTest,
                       IFrameCrossOriginExplicitAllow) {
  const GURL url(https_test_server()->GetURL("a.test", "/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // See https://w3c.github.io/webappsec-permissions-policy/ for more
  // information on permissions policies and allowing cross-origin iframes
  // to have particular permissions.
  EXPECT_TRUE(ExecJs(tab, R"(const frame = document.getElementById('test');
    frame.setAttribute('allow', 'window-placement');)",
                     content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  GURL subframe_url(https_test_server()->GetURL("b.test", "/title1.html"));
  content::NavigateIframeToURL(tab, /*iframe_id=*/"test", subframe_url);

  content::RenderFrameHost* child = ChildFrameAt(tab->GetMainFrame(), 0);
  ASSERT_TRUE(child);
  EXPECT_FALSE(tab->GetMainFrame()->HasTransientUserActivation());
  EXPECT_FALSE(child->GetMainFrame()->HasTransientUserActivation());

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);

  // Accept the prompt after activation expires, expect an activation signal.
  ExecuteScriptAsync(child, "getScreenDetails()");
  WaitForUserActivationExpiry();
  ASSERT_TRUE(permission_request_manager->IsRequestInProgress());
  permission_request_manager->Accept();
  EXPECT_EQ("granted", EvalJs(child, kCheckPermission,
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(tab->GetMainFrame()->HasTransientUserActivation());
  EXPECT_TRUE(child->GetMainFrame()->HasTransientUserActivation());
}

// TODO(enne): Windows assumes that display::GetScreen() is a ScreenWin
// which is not true here.
#if !defined(OS_WIN)

// Verify that window.screen.isExtended returns true in a same-origin
// iframe without the window placement permission policy allowed.
IN_PROC_BROWSER_TEST_F(MultiscreenWindowPlacementPermissionContextTest,
                       IsExtendedSameOriginAllow) {
  const GURL url(https_test_server()->GetURL("a.test", "/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* child = ChildFrameAt(tab->GetMainFrame(), 0);
  ASSERT_TRUE(child);

  EXPECT_EQ(true, EvalJs(tab, R"(window.screen.isExtended)",
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ(true, EvalJs(child, R"(window.screen.isExtended)",
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// Verify that window.screen.isExtended returns false in a cross-origin
// iframe without the window placement permission policy allowed.
IN_PROC_BROWSER_TEST_F(MultiscreenWindowPlacementPermissionContextTest,
                       IsExtendedCrossOriginDeny) {
  const GURL url(https_test_server()->GetURL("a.test", "/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  GURL subframe_url(https_test_server()->GetURL("b.test", "/title1.html"));
  content::NavigateIframeToURL(tab, /*iframe_id=*/"test", subframe_url);

  content::RenderFrameHost* child = ChildFrameAt(tab->GetMainFrame(), 0);
  ASSERT_TRUE(child);

  EXPECT_EQ(true, EvalJs(tab, R"(window.screen.isExtended)",
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ(false, EvalJs(child, R"(window.screen.isExtended)",
                          content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

// Verify that window.screen.isExtended returns true in a cross-origin
// iframe with the window placement permission policy allowed.
IN_PROC_BROWSER_TEST_F(MultiscreenWindowPlacementPermissionContextTest,
                       IsExtendedCrossOriginAllow) {
  const GURL url(https_test_server()->GetURL("a.test", "/iframe.html"));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  auto* tab = browser()->tab_strip_model()->GetActiveWebContents();

  // See https://w3c.github.io/webappsec-permissions-policy/ for more
  // information on permissions policies and allowing cross-origin iframes
  // to have particular permissions.
  EXPECT_TRUE(ExecJs(tab, R"(const frame = document.getElementById('test');
    frame.setAttribute('allow', 'window-placement');)",
                     content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  GURL subframe_url(https_test_server()->GetURL("b.test", "/title1.html"));
  content::NavigateIframeToURL(tab, /*iframe_id=*/"test", subframe_url);

  content::RenderFrameHost* child = ChildFrameAt(tab->GetMainFrame(), 0);
  ASSERT_TRUE(child);

  EXPECT_EQ(true, EvalJs(tab, R"(window.screen.isExtended)",
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ(true, EvalJs(child, R"(window.screen.isExtended)",
                         content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

#endif  // !defined(OS_WIN)

}  // namespace

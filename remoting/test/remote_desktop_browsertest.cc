// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remote_desktop_browsertest.h"

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "content/public/test/test_utils.h"

using extensions::Extension;

namespace remoting {

RemoteDesktopBrowserTest::RemoteDesktopBrowserTest() {}

RemoteDesktopBrowserTest::~RemoteDesktopBrowserTest() {}

void RemoteDesktopBrowserTest::SetUp() {
  ParseCommandLine();
  ExtensionBrowserTest::SetUp();
}

// Change behavior of the default host resolver to avoid DNS lookup errors,
// so we can make network calls.
void RemoteDesktopBrowserTest::SetUpInProcessBrowserTestFixture() {
  // The resolver object lifetime is managed by sync_test_setup, not here.
  EnableDNSLookupForThisTest(
      new net::RuleBasedHostResolverProc(host_resolver()));
}

void RemoteDesktopBrowserTest::TearDownInProcessBrowserTestFixture() {
  DisableDNSLookupForThisTest();
}

void RemoteDesktopBrowserTest::VerifyInternetAccess() {
  GURL google_url("http://www.google.com");
  NavigateToURLAndWait(google_url);

  EXPECT_EQ(GetCurrentURL().host(), "www.google.com");
}

bool RemoteDesktopBrowserTest::HtmlElementVisible(const std::string& name) {
  _ASSERT_TRUE(HtmlElementExists(name));

  ExecuteScript(
      "function isElementVisible(name) {"
      "  var element = document.getElementById(name);"
      "  /* The existence of the element has already been ASSERTed. */"
      "  do {"
      "    if (element.hidden) {"
      "      return false;"
      "    }"
      "    element = element.parentNode;"
      "  } while (element != null);"
      "  return true;"
      "};");

  return ExecuteScriptAndExtractBool(
      "isElementVisible(\"" + name + "\")");
}

void RemoteDesktopBrowserTest::InstallChromotingApp() {
  base::FilePath install_dir(WebAppCrxPath());
  scoped_refptr<const Extension> extension(InstallExtensionWithUIAutoConfirm(
      install_dir, 1, browser()));

  EXPECT_FALSE(extension.get() == NULL);
}

void RemoteDesktopBrowserTest::UninstallChromotingApp() {
  UninstallExtension(ChromotingID());
  chromoting_id_.clear();
}

void RemoteDesktopBrowserTest::VerifyChromotingLoaded(bool expected) {
  const ExtensionSet* extensions = extension_service()->extensions();
  scoped_refptr<const extensions::Extension> extension;
  ExtensionSet::const_iterator iter;
  bool installed = false;

  for (iter = extensions->begin(); iter != extensions->end(); ++iter) {
    extension = *iter;
    // Is there a better way to recognize the chromoting extension
    // than name comparison?
    if (extension->name() == "Chromoting" ||
        extension->name() == "Chrome Remote Desktop") {
      installed = true;
      break;
    }
  }

  if (installed) {
    chromoting_id_ = extension->id();

    EXPECT_EQ(extension->GetType(),
        extensions::Manifest::TYPE_LEGACY_PACKAGED_APP);

    EXPECT_TRUE(extension->ShouldDisplayInAppLauncher());
  }

  EXPECT_EQ(installed, expected);
}

void RemoteDesktopBrowserTest::LaunchChromotingApp() {
  ASSERT_FALSE(ChromotingID().empty());

  const GURL chromoting_main = Chromoting_Main_URL();
  NavigateToURLAndWait(chromoting_main);

  EXPECT_EQ(GetCurrentURL(), chromoting_main);
}

void RemoteDesktopBrowserTest::Authorize() {
  // The chromoting extension should be installed.
  ASSERT_FALSE(ChromotingID().empty());

  // The chromoting main page should be loaded in the current tab
  // and isAuthenticated() should be false (auth dialog visible).
  ASSERT_EQ(GetCurrentURL(), Chromoting_Main_URL());
  ASSERT_FALSE(ExecuteScriptAndExtractBool(
      "remoting.OAuth2.prototype.isAuthenticated()"));

  ExecuteScriptAndWait("remoting.OAuth2.prototype.doAuthRedirect();");

  // Verify the active tab is at the "Google Accounts" login page.
  EXPECT_EQ(GetCurrentURL().host(), "accounts.google.com");
  EXPECT_TRUE(HtmlElementExists("Email"));
  EXPECT_TRUE(HtmlElementExists("Passwd"));
}

void RemoteDesktopBrowserTest::Authenticate() {
  // The chromoting extension should be installed.
  ASSERT_FALSE(ChromotingID().empty());

  // The active tab should have the "Google Accounts" login page loaded.
  ASSERT_EQ(GetCurrentURL().host(), "accounts.google.com");
  ASSERT_TRUE(HtmlElementExists("Email"));
  ASSERT_TRUE(HtmlElementExists("Passwd"));

  // Now log in using the username and password passed in from the command line.
  ExecuteScriptAndWait(
      "document.getElementById(\"Email\").value = \"" + username_ + "\";" +
      "document.getElementById(\"Passwd\").value = \"" + password_ +"\";" +
      "document.forms[\"gaia_loginform\"].submit();");

  EXPECT_EQ(GetCurrentURL().host(), "accounts.google.com");

  // TODO: Is there a better way to verify we are on the
  // "Request for Permission" page?
  EXPECT_TRUE(HtmlElementExists("submit_approve_access"));
}

void RemoteDesktopBrowserTest::Approve() {
  // The chromoting extension should be installed.
  ASSERT_FALSE(ChromotingID().empty());

  // The active tab should have the chromoting app loaded.
  ASSERT_EQ(GetCurrentURL().host(), "accounts.google.com");

  // Is there a better way to verify we are on the "Request for Permission"
  // page?
  ASSERT_TRUE(HtmlElementExists("submit_approve_access"));

  const GURL chromoting_main = Chromoting_Main_URL();
  ExecuteScriptAndWaitUntil(
      "lso.approveButtonAction();"
      "document.forms[\"connect-approve\"].submit();",
      chromoting_main);

  ASSERT_TRUE(GetCurrentURL() == chromoting_main);

  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      "remoting.OAuth2.prototype.isAuthenticated()"));
}

void RemoteDesktopBrowserTest::StartMe2Me() {
  // The chromoting extension should be installed.
  ASSERT_FALSE(ChromotingID().empty());

  // The active tab should have the chromoting app loaded.
  ASSERT_EQ(GetCurrentURL(), Chromoting_Main_URL());
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      "remoting.OAuth2.prototype.isAuthenticated()"));

  // The Me2Me host list should be hidden.
  ASSERT_FALSE(HtmlElementVisible("me2me-content"));
  // The Me2Me "Get Start" button should be visible.
  ASSERT_TRUE(HtmlElementVisible("get-started-me2me"));

  // Starting Me2Me.
  ExecuteScript("remoting.showMe2MeUiAndSave();");

  EXPECT_TRUE(HtmlElementVisible("me2me-content"));
  EXPECT_FALSE(HtmlElementVisible("me2me-first-run"));
}

void RemoteDesktopBrowserTest::Install() {
  // TODO: add support for installing unpacked extension (the v2 app needs it).
  if (!NoInstall()) {
    VerifyChromotingLoaded(false);
    InstallChromotingApp();
  }

  VerifyChromotingLoaded(true);
}

void RemoteDesktopBrowserTest::Cleanup() {
  // TODO: Remove this hack by blocking on the appropriate notification.
  // The browser may still be loading images embedded in the webapp. If we
  // uinstall it now those load will fail. Navigating away to avoid the load
  // failures.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));

  if (!NoCleanup()) {
    UninstallChromotingApp();
    VerifyChromotingLoaded(false);
  }
}

void RemoteDesktopBrowserTest::Auth() {
  Authorize();
  Authenticate();
  Approve();
}

void RemoteDesktopBrowserTest::ConnectToLocalHost() {
  // Wait until remoting.hostList.localHost_ is initialized.
  // This can take a while.
  // TODO: Instead of polling, can we register a callback to
  // remoting.hostList.setLocalHost_?
  while (ExecuteScriptAndExtractBool(
      "remoting.hostList.localHost_ == null")) {
  }

  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      "remoting.hostList.localHost_.hostName && "
      "remoting.hostList.localHost_.hostId && "
      "remoting.hostList.localHost_.status && "
      "remoting.hostList.localHost_.status == 'ONLINE'"));

  ClickOnControl("this-host-connect");
}

void RemoteDesktopBrowserTest::EnableDNSLookupForThisTest(
    net::RuleBasedHostResolverProc* host_resolver) {
  // mock_host_resolver_override_ takes ownership of the resolver.
  scoped_refptr<net::RuleBasedHostResolverProc> resolver =
      new net::RuleBasedHostResolverProc(host_resolver);
  resolver->AllowDirectLookup("*.google.com");
  // On Linux, we use Chromium's NSS implementation which uses the following
  // hosts for certificate verification. Without these overrides, running the
  // integration tests on Linux causes errors as we make external DNS lookups.
  resolver->AllowDirectLookup("*.thawte.com");
  resolver->AllowDirectLookup("*.geotrust.com");
  resolver->AllowDirectLookup("*.gstatic.com");
  resolver->AllowDirectLookup("*.googleapis.com");
  mock_host_resolver_override_.reset(
      new net::ScopedDefaultHostResolverProc(resolver.get()));
}

void RemoteDesktopBrowserTest::DisableDNSLookupForThisTest() {
  mock_host_resolver_override_.reset();
}

void RemoteDesktopBrowserTest::ParseCommandLine() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  // The test framework overrides any command line user-data-dir
  // argument with a /tmp/.org.chromium.Chromium.XXXXXX directory.
  // That happens in the ChromeTestLauncherDelegate, and affects
  // all unit tests (no opt out available). It intentionally erases
  // any --user-data-dir switch if present and appends a new one.
  // Re-override the default data dir if override-user-data-dir
  // is specified.
  if (command_line->HasSwitch(kOverrideUserDataDir)) {
    const base::FilePath& override_user_data_dir =
        command_line->GetSwitchValuePath(kOverrideUserDataDir);

    ASSERT_FALSE(override_user_data_dir.empty());

    command_line->AppendSwitchPath(switches::kUserDataDir,
                                   override_user_data_dir);
  }

  username_ = command_line->GetSwitchValueASCII(kUsername);
  password_ = command_line->GetSwitchValueASCII(kkPassword);
  me2me_pin_ = command_line->GetSwitchValueASCII(kMe2MePin);

  no_cleanup_ = command_line->HasSwitch(kNoCleanup);
  no_install_ = command_line->HasSwitch(kNoInstall);

  if (!no_install_) {
    webapp_crx_ = command_line->GetSwitchValuePath(kWebAppCrx);
    ASSERT_FALSE(webapp_crx_.empty());
  }
}

void RemoteDesktopBrowserTest::ExecuteScript(const std::string& script) {
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(), script));
}

void RemoteDesktopBrowserTest::ExecuteScriptAndWait(const std::string& script) {
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &browser()->tab_strip_model()->GetActiveWebContents()->
              GetController()));

  ExecuteScript(script);

  observer.Wait();
}

void RemoteDesktopBrowserTest::ExecuteScriptAndWaitUntil(
    const std::string& script,
    const GURL& target) {
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &browser()->tab_strip_model()->GetActiveWebContents()->
              GetController()));

  ExecuteScript(script);

  observer.Wait();

  // TODO: is there a better way to wait for all the redirections to complete?
  while (GetCurrentURL() != target) {
    content::WindowedNotificationObserver(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<content::NavigationController>(
            &browser()->tab_strip_model()->GetActiveWebContents()->
                GetController())).Wait();
  }
}

bool RemoteDesktopBrowserTest::ExecuteScriptAndExtractBool(
    const std::string& script) {
  bool result;
  _ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(" + script + ");",
      &result));

  return result;
}

int RemoteDesktopBrowserTest::ExecuteScriptAndExtractInt(
    const std::string& script) {
  int result;
  _ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(" + script + ");",
      &result));

  return result;
}

std::string RemoteDesktopBrowserTest::ExecuteScriptAndExtractString(
    const std::string& script) {
  std::string result;
  _ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(" + script + ");",
      &result));

  return result;
}

// Helper to navigate to a given url.
void RemoteDesktopBrowserTest::NavigateToURLAndWait(const GURL& url) {
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &browser()->tab_strip_model()->GetActiveWebContents()->
              GetController()));

  ui_test_utils::NavigateToURL(browser(), url);
  observer.Wait();
}

void RemoteDesktopBrowserTest::ClickOnControl(const std::string& name) {
  _ASSERT_TRUE(HtmlElementExists(name));
  _ASSERT_TRUE(HtmlElementVisible(name));

  ExecuteScript("document.getElementById(\"" + name + "\").click();");
}

void RemoteDesktopBrowserTest::EnterPin(const std::string& pin) {
  // Wait for the pin-form to be displayed. This can take a while.
  // We also need to dismiss the host-needs-update dialog if it comes up.
  // TODO 1: Instead of polling, can we register a callback to be called
  // when the pin-form is ready?
  // TODO 2: Instead of blindly dismiss the host-needs-update dialog,
  // we should verify that it only pops up at the right circumstance. That
  // probably belongs in a separate test case though.
  do {
    if (HtmlElementVisible("host-needs-update-connect-button")) {
      ClickOnControl("host-needs-update-connect-button");
    }
  } while (!HtmlElementVisible("pin-form"));

  ExecuteScript(
      "document.getElementById(\"pin-entry\").value = \"" + pin + "\";");

  ClickOnControl("pin-connect-button");

  WaitForConnection();
}

void RemoteDesktopBrowserTest::WaitForConnection() {
  // Wait until the client has connected to the server.
  // This can take a while.
  // TODO: Instead of polling, can we register a callback to
  // remoting.clientSession.onStageChange_?
  while (ExecuteScriptAndExtractBool(
      "remoting.clientSession == null")) {
  }
}

}  // namespace remoting

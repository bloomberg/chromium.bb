// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.cc"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"

using extensions::Extension;

namespace {
// Command line arguments specific to the chromoting browser tests.
const char kOverrideUserDataDir[] = "override-user-data-dir";
const char kNoCleanup[] = "no-cleanup";
const char kNoInstall[] = "no-install";
const char kWebAppCrx[] = "webapp-crx";
const char kUsername[] = "username";
const char kkPassword[] = "password";

// ASSERT_TRUE can only be used in void returning functions.
void _ASSERT_TRUE(bool condition) {
  ASSERT_TRUE(condition);
  return;
}

}

namespace remoting {

class RemoteDesktopBrowserTest : public ExtensionBrowserTest {
 public:
  virtual void SetUp() OVERRIDE {
    ParseCommandLine();
    ExtensionBrowserTest::SetUp();
  }

 protected:
  // Override InProcessBrowserTest. Change behavior of the default host
  // resolver to avoid DNS lookup errors, so we can make network calls.
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    // The resolver object lifetime is managed by sync_test_setup, not here.
    EnableDNSLookupForThisTest(
        new net::RuleBasedHostResolverProc(host_resolver()));
  }

  // Override InProcessBrowserTest.
  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    DisableDNSLookupForThisTest();
  }

  // Install the chromoting extension from a crx file.
  void InstallChromotingApp();

  // Uninstall the chromoting extension.
  void UninstallChromotingApp();

  // Test whether the chromoting extension is installed.
  void VerifyChromotingLoaded(bool expected);

  // Launch the chromoting app.
  void LaunchChromotingApp();

  // Verify the test has access to the internet (specifically google.com)
  void VerifyInternetAccess();

  void Authorize();

  void Authenticate();

  // Whether to perform the cleanup tasks (uninstalling chromoting, etc).
  // This is useful for diagnostic purposes.
  bool NoCleanup() { return no_cleanup_; }

  // Whether to install the chromoting extension before running the test cases.
  // This is useful for diagnostic purposes.
  bool NoInstall() { return no_install_; }

 private:
  void ParseCommandLine();

  // Change behavior of the default host resolver to allow DNS lookup
  // to proceed instead of being blocked by the test infrastructure.
  void EnableDNSLookupForThisTest(
    net::RuleBasedHostResolverProc* host_resolver);

  // We need to reset the DNS lookup when we finish, or the test will fail.
  void DisableDNSLookupForThisTest();

  // Helper to get the path to the crx file of the webapp to be tested.
  base::FilePath WebAppCrxPath() { return webapp_crx_; }

  // Helper to get the extension ID of the installed chromoting webapp.
  std::string ChromotingID() { return chromoting_id_; }

  // Helper to retrieve the current URL of the active tab in the browser.
  GURL GetCurrentURL() {
    return browser()->tab_strip_model()->GetActiveWebContents()->GetURL();
  }

  // Helper to execute a javascript code snippet on the current page.
  void ExecuteScript(const std::string& script) {
    ASSERT_TRUE(content::ExecuteScript(
        browser()->tab_strip_model()->GetActiveWebContents(), script));
  }

  // Helper to execute a javascript code snippet on the current page and
  // wait for page load to complete.
  void ExecuteScriptAndWait(const std::string& script) {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<content::NavigationController>(
            &browser()->tab_strip_model()->GetActiveWebContents()->
                GetController()));

    ExecuteScript(script);

    observer.Wait();
  }

  // Helper to execute a javascript code snippet on the current page and
  // extract the boolean result.
  bool ExecuteScriptAndExtractBool(const std::string& script) {
    bool result;
    // Using a private assert function because ASSERT_TRUE can only be used in
    // void returning functions.
    _ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send(" + script + ");",
        &result));

    return result;
  }

  // Helper to check whether a html element with the given name exists on
  // the current page.
  bool HtmlElementExists(const std::string& name) {
    return ExecuteScriptAndExtractBool(
        "document.getElementById(\"" + name + "\") != null");
  }

  // Helper to navigate to a given url.
  void NavigateToURLAndWait(const GURL& url) {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<content::NavigationController>(
            &browser()->tab_strip_model()->GetActiveWebContents()->
                GetController()));

    ui_test_utils::NavigateToURL(browser(), url);
    observer.Wait();
  }

  // This test needs to make live DNS requests for access to
  // GAIA and sync server URLs under google.com. We use a scoped version
  // to override the default resolver while the test is active.
  scoped_ptr<net::ScopedDefaultHostResolverProc> mock_host_resolver_override_;

  bool no_cleanup_;
  bool no_install_;
  std::string chromoting_id_;
  base::FilePath webapp_crx_;
  std::string username_;
  std::string password_;
};

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

  no_cleanup_ = command_line->HasSwitch(kNoCleanup);
  no_install_ = command_line->HasSwitch(kNoInstall);

  if (!no_install_) {
    webapp_crx_ = command_line->GetSwitchValuePath(kWebAppCrx);
    ASSERT_FALSE(webapp_crx_.empty());
  }
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

void RemoteDesktopBrowserTest::VerifyInternetAccess() {
  GURL google_url("http://www.google.com");
  NavigateToURLAndWait(google_url);

  EXPECT_EQ(GetCurrentURL().host(), "www.google.com");
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

void RemoteDesktopBrowserTest::LaunchChromotingApp() {
  ASSERT_FALSE(ChromotingID().empty());

  std::string url = "chrome-extension://" + ChromotingID() + "/main.html";
  const GURL chromoting_main(url);
  NavigateToURLAndWait(chromoting_main);

  EXPECT_EQ(GetCurrentURL(), chromoting_main);
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

void RemoteDesktopBrowserTest::Authorize() {
  // The chromoting extension should be installed.
  ASSERT_FALSE(ChromotingID().empty());

  // The chromoting main page should be loaded in the current tab
  // and isAuthenticated() should be false (auth dialog visible).
  std::string url = "chrome-extension://" + ChromotingID() + "/main.html";
  ASSERT_EQ(GetCurrentURL().spec(), url);
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

IN_PROC_BROWSER_TEST_F(RemoteDesktopBrowserTest, MANUAL_Launch) {
  VerifyInternetAccess();

  if (!NoInstall()) {
    VerifyChromotingLoaded(false);
    InstallChromotingApp();
  }

  VerifyChromotingLoaded(true);

  LaunchChromotingApp();

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

IN_PROC_BROWSER_TEST_F(RemoteDesktopBrowserTest, MANUAL_Auth) {
  VerifyInternetAccess();

  if (!NoInstall()) {
    VerifyChromotingLoaded(false);
    InstallChromotingApp();
  }

  VerifyChromotingLoaded(true);

  LaunchChromotingApp();

  Authorize();

  Authenticate();

  if (!NoCleanup()) {
    UninstallChromotingApp();
    VerifyChromotingLoaded(false);
  }
}

}  // namespace remoting

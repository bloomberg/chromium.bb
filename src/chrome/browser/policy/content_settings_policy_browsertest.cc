// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace policy {

namespace {
const char kURL[] = "http://example.com";
const char kCookieValue[] = "converted=true";
// Assigned to Philip J. Fry to fix eventually.
// TODO(maksims): use year 3000 when we get rid off the 32-bit
// versions. https://crbug.com/619828
const char kCookieOptions[] = ";expires=Wed Jan 01 2038 00:00:00 GMT";

bool IsJavascriptEnabled(content::WebContents* contents) {
  base::Value value =
      content::ExecuteScriptAndGetValue(contents->GetMainFrame(), "123");
  return value.is_int() && value.GetInt() == 123;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(PolicyTest, PRE_PRE_DefaultCookiesSetting) {
  // Verifies that cookies are deleted on shutdown. This test is split in 3
  // parts because it spans 2 browser restarts.

  Profile* profile = browser()->profile();
  GURL url(kURL);
  // No cookies at startup.
  EXPECT_TRUE(content::GetCookies(profile, url).empty());
  // Set a cookie now.
  std::string value = base::StrCat({kCookieValue, kCookieOptions});
  EXPECT_TRUE(content::SetCookie(profile, url, value));
  // Verify it was set.
  EXPECT_EQ(kCookieValue, GetCookies(profile, url));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, PRE_DefaultCookiesSetting) {
  // Verify that the cookie persists across restarts.
  EXPECT_EQ(kCookieValue, GetCookies(browser()->profile(), GURL(kURL)));
  // Now set the policy and the cookie should be gone after another restart.
  PolicyMap policies;
  policies.Set(key::kDefaultCookiesSetting, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(CONTENT_SETTING_SESSION_ONLY),
               nullptr);
  UpdateProviderPolicy(policies);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DefaultCookiesSetting) {
  // Verify that the cookie is gone.
  EXPECT_TRUE(GetCookies(browser()->profile(), GURL(kURL)).empty());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, PRE_PRE_WebsiteCookiesSetting) {
  // Verifies that cookies are deleted on shutdown. This test is split in 3
  // parts because it spans 2 browser restarts.

  Profile* profile = browser()->profile();
  GURL url(kURL);
  // No cookies at startup.
  EXPECT_TRUE(content::GetCookies(profile, url).empty());
  // Set a cookie now.
  std::string value = base::StrCat({kCookieValue, kCookieOptions});
  EXPECT_TRUE(content::SetCookie(profile, url, value));
  // Verify it was set.
  EXPECT_EQ(kCookieValue, GetCookies(profile, url));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, PRE_WebsiteCookiesSetting) {
  // Verify that the cookie persists across restarts.
  EXPECT_EQ(kCookieValue, GetCookies(browser()->profile(), GURL(kURL)));
  // Now set the policy and the cookie should be gone after another restart.
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetWebsiteSettingDefaultScope(
          GURL(kURL), GURL(kURL), ContentSettingsType::COOKIES, std::string(),
          std::make_unique<base::Value>(CONTENT_SETTING_SESSION_ONLY));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, WebsiteCookiesSetting) {
  // Verify that the cookie is gone.
  EXPECT_TRUE(GetCookies(browser()->profile(), GURL(kURL)).empty());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, Javascript) {
  // Verifies that Javascript can be disabled.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsJavascriptEnabled(contents));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS_CONSOLE));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS_DEVICES));

  // Disable Javascript via policy.
  PolicyMap policies;
  policies.Set(key::kJavascriptEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);
  // Reload the page.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  EXPECT_FALSE(IsJavascriptEnabled(contents));
  // Developer tools still work when javascript is disabled.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS_CONSOLE));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_DEV_TOOLS_DEVICES));
  // Javascript is always enabled for the internal pages.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL));
  EXPECT_TRUE(IsJavascriptEnabled(contents));

  // The javascript content setting policy overrides the javascript policy.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  EXPECT_FALSE(IsJavascriptEnabled(contents));
  policies.Set(key::kDefaultJavaScriptSetting, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(CONTENT_SETTING_ALLOW), nullptr);
  UpdateProviderPolicy(policies);
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  EXPECT_TRUE(IsJavascriptEnabled(contents));
}

class WebBluetoothPolicyTest : public PolicyTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(juncai): Remove this switch once Web Bluetooth is supported on Linux
    // and Windows.
    // https://crbug.com/570344
    // https://crbug.com/507419
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    PolicyTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(WebBluetoothPolicyTest, Block) {
  // Fake the BluetoothAdapter to say it's present.
  scoped_refptr<device::MockBluetoothAdapter> adapter =
      new testing::NiceMock<device::MockBluetoothAdapter>;
  EXPECT_CALL(*adapter, IsPresent()).WillRepeatedly(testing::Return(true));
  auto bt_global_values =
      device::BluetoothAdapterFactory::Get()->InitGlobalValuesForTesting();
  bt_global_values->SetLESupported(true);
  device::BluetoothAdapterFactory::SetAdapterForTesting(adapter);

  // Navigate to a secure context.
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("localhost", "/simple_page.html"));
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_THAT(
      web_contents->GetMainFrame()->GetLastCommittedOrigin().Serialize(),
      testing::StartsWith("http://localhost:"));

  // Set the policy to block Web Bluetooth.
  PolicyMap policies;
  policies.Set(key::kDefaultWebBluetoothGuardSetting, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(2), nullptr);
  UpdateProviderPolicy(policies);

  std::string rejection;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "navigator.bluetooth.requestDevice({filters: [{name: 'Hello'}]})"
      "  .then(() => { domAutomationController.send('Success'); },"
      "        reason => {"
      "      domAutomationController.send(reason.name + ': ' + reason.message);"
      "  });",
      &rejection));
  EXPECT_THAT(rejection, testing::MatchesRegex("NotFoundError: .*policy.*"));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, WebUsbDefault) {
  const auto kTestOrigin = url::Origin::Create(GURL("https://foo.com:443"));

  // Expect the default permission value to be 'ask'.
  auto* context = UsbChooserContextFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(context->CanRequestObjectPermission(kTestOrigin, kTestOrigin));

  // Update policy to change the default permission value to 'block'.
  PolicyMap policies;
  SetPolicy(&policies, key::kDefaultWebUsbGuardSetting,
            std::make_unique<base::Value>(2));
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(context->CanRequestObjectPermission(kTestOrigin, kTestOrigin));

  // Update policy to change the default permission value to 'ask'.
  SetPolicy(&policies, key::kDefaultWebUsbGuardSetting,
            std::make_unique<base::Value>(3));
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(context->CanRequestObjectPermission(kTestOrigin, kTestOrigin));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, WebUsbAllowDevicesForUrls) {
  const auto kTestOrigin = url::Origin::Create(GURL("https://foo.com:443"));
  scoped_refptr<device::FakeUsbDeviceInfo> device =
      base::MakeRefCounted<device::FakeUsbDeviceInfo>(0, 0, "Google", "Gizmo",
                                                      "123ABC");
  const auto& device_info = device->GetDeviceInfo();

  // Expect the default permission value to be empty.
  auto* context = UsbChooserContextFactory::GetForProfile(browser()->profile());
  EXPECT_FALSE(
      context->HasDevicePermission(kTestOrigin, kTestOrigin, device_info));

  // Update policy to add an entry to the permission value to allow
  // |kTestOrigin| to access the device described by |device_info|.
  PolicyMap policies;

  base::Value device_value(base::Value::Type::DICTIONARY);
  device_value.SetKey("vendor_id", base::Value(0));
  device_value.SetKey("product_id", base::Value(0));

  base::Value devices_value(base::Value::Type::LIST);
  devices_value.Append(std::move(device_value));

  base::Value urls_value(base::Value::Type::LIST);
  urls_value.Append(base::Value("https://foo.com"));

  base::Value entry(base::Value::Type::DICTIONARY);
  entry.SetKey("devices", std::move(devices_value));
  entry.SetKey("urls", std::move(urls_value));

  auto policy_value = std::make_unique<base::Value>(base::Value::Type::LIST);
  policy_value->Append(std::move(entry));

  SetPolicy(&policies, key::kWebUsbAllowDevicesForUrls,
            std::move(policy_value));
  UpdateProviderPolicy(policies);

  EXPECT_TRUE(
      context->HasDevicePermission(kTestOrigin, kTestOrigin, device_info));

  // Remove the policy to ensure that it can be dynamically updated.
  SetPolicy(&policies, key::kWebUsbAllowDevicesForUrls,
            std::make_unique<base::Value>(base::Value::Type::LIST));
  UpdateProviderPolicy(policies);

  EXPECT_FALSE(
      context->HasDevicePermission(kTestOrigin, kTestOrigin, device_info));
}

class ScrollToTextFragmentPolicyTest
    : public PolicyTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    // Set policies before the browser starts up.
    PolicyMap policies;
    policies.Set(key::kScrollToTextFragmentEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(IsScrollToTextFragmentEnabled()),
                 nullptr);
    UpdateProviderPolicy(policies);
    PolicyTest::CreatedBrowserMainParts(browser_main_parts);
  }

  bool IsScrollToTextFragmentEnabled() { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(ScrollToTextFragmentPolicyTest, RunPolicyTest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL target_text_url(embedded_test_server()->GetURL(
      "/scroll/scrollable_page_with_content.html#:~:text=text"));

  ui_test_utils::NavigateToURL(browser(), target_text_url);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(contents);
  ASSERT_TRUE(content::WaitForRenderFrameReady(contents->GetMainFrame()));

  content::RenderFrameSubmissionObserver frame_observer(contents);
  if (IsScrollToTextFragmentEnabled()) {
    frame_observer.WaitForScrollOffsetAtTop(false);
  } else {
    // Force a frame - if it were going to happen, the scroll would complete
    // before this forced frame makes its way through the pipeline.
    content::RunUntilInputProcessed(
        contents->GetMainFrame()->GetView()->GetRenderWidgetHost());
  }
  EXPECT_EQ(IsScrollToTextFragmentEnabled(),
            !contents->GetMainFrame()->GetView()->IsScrollOffsetAtTop());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ScrollToTextFragmentPolicyTest,
                         ::testing::Bool());

}  // namespace policy

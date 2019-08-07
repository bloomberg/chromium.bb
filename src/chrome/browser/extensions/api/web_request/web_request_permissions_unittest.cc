// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/url_constants.h"
#include "chromeos/login/login_state/scoped_test_public_session_login_state.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/api/web_request/web_request_permissions.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ipc/ipc_message.h"
#include "net/base/request_priority.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chromeos/login/login_state/login_state.h"
#endif  // defined(OS_CHROMEOS)

using extension_test_util::LoadManifestUnchecked;
using extensions::Extension;
using extensions::Manifest;
using extensions::PermissionsData;
using extensions::WebRequestInfo;
using extensions::WebRequestInfoInitParams;

class ExtensionWebRequestHelpersTestWithThreadsTest : public testing::Test {
 public:
  ExtensionWebRequestHelpersTestWithThreadsTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}

 protected:
  void SetUp() override;

 private:
  content::TestBrowserThreadBundle thread_bundle_;

 protected:
  net::TestURLRequestContext context;

  // This extension has Web Request permissions, but no host permission.
  scoped_refptr<Extension> permissionless_extension_;
  // This extension has Web Request permissions, and *.com a host permission.
  scoped_refptr<Extension> com_extension_;
  // This extension is the same as com_extension, except it's installed from
  // Manifest::EXTERNAL_POLICY_DOWNLOAD.
  scoped_refptr<Extension> com_policy_extension_;
  scoped_refptr<extensions::InfoMap> extension_info_map_;
};

void ExtensionWebRequestHelpersTestWithThreadsTest::SetUp() {
  testing::Test::SetUp();

  std::string error;
  permissionless_extension_ = LoadManifestUnchecked("permissions",
                                                    "web_request_no_host.json",
                                                    Manifest::INVALID_LOCATION,
                                                    Extension::NO_FLAGS,
                                                    "ext_id_1",
                                                    &error);
  ASSERT_TRUE(permissionless_extension_.get()) << error;
  com_extension_ =
      LoadManifestUnchecked("permissions",
                            "web_request_com_host_permissions.json",
                            Manifest::INVALID_LOCATION,
                            Extension::NO_FLAGS,
                            "ext_id_2",
                            &error);
  ASSERT_TRUE(com_extension_.get()) << error;
  com_policy_extension_ =
      LoadManifestUnchecked("permissions",
                            "web_request_com_host_permissions.json",
                            Manifest::EXTERNAL_POLICY_DOWNLOAD,
                            Extension::NO_FLAGS,
                            "ext_id_3",
                            &error);
  ASSERT_TRUE(com_policy_extension_.get()) << error;
  extension_info_map_ = new extensions::InfoMap;
  extension_info_map_->AddExtension(permissionless_extension_.get(),
                                    base::Time::Now(),
                                    false, // incognito_enabled
                                    false); // notifications_disabled
  extension_info_map_->AddExtension(
      com_extension_.get(),
      base::Time::Now(),
      false, // incognito_enabled
      false); // notifications_disabled
  extension_info_map_->AddExtension(
      com_policy_extension_.get(),
      base::Time::Now(),
      false, // incognito_enabled
      false); // notifications_disabled
}

// Ensures that requests to extension blacklist urls can't be intercepted by
// extensions.
TEST_F(ExtensionWebRequestHelpersTestWithThreadsTest,
       BlacklistUpdateUrlsHidden) {
  auto create_request_params = [](const std::string& url) {
    const int kRendererProcessId = 2;
    WebRequestInfoInitParams request;
    request.url = GURL(url);
    request.render_process_id = kRendererProcessId;
    return request;
  };

  WebRequestInfo request_1(create_request_params(
      "http://www.gstatic.com/chrome/extensions/blacklist"));
  EXPECT_TRUE(
      WebRequestPermissions::HideRequest(extension_info_map_.get(), request_1));

  WebRequestInfo request_2(create_request_params(
      "https://www.gstatic.com/chrome/extensions/blacklist"));
  EXPECT_TRUE(
      WebRequestPermissions::HideRequest(extension_info_map_.get(), request_2));
}

// Ensure requests made by the local NTP are hidden from extensions. Regression
// test for crbug.com/931013.
TEST_F(ExtensionWebRequestHelpersTestWithThreadsTest, LocalNTPRequests) {
  const GURL example_com("http://example.com");

  auto create_request_params =
      [&example_com](const url::Origin& initiator, content::ResourceType type,
                     extensions::WebRequestResourceType web_request_type,
                     bool is_browser_side_navigation) {
        WebRequestInfoInitParams info_params;
        info_params.url = example_com;
        info_params.initiator = initiator;
        info_params.render_process_id = -1;
        info_params.type = type;
        info_params.web_request_type = web_request_type;
        info_params.is_browser_side_navigation = is_browser_side_navigation;
        return info_params;
      };

  url::Origin ntp_origin =
      url::Origin::Create(GURL(chrome::kChromeSearchLocalNtpUrl));

  // Sub-resource browser initiated requests are hidden from extensions.
  WebRequestInfoInitParams info_params_1 =
      create_request_params(ntp_origin, content::ResourceType::kSubResource,
                            extensions::WebRequestResourceType::OTHER, false);
  EXPECT_TRUE(WebRequestPermissions::HideRequest(
      extension_info_map_.get(), WebRequestInfo(std::move(info_params_1))));

  // Sub-frame navigations initiated from the local ntp should be hidden.
  WebRequestInfoInitParams info_params_2 = create_request_params(
      ntp_origin, content::ResourceType::kSubFrame,
      extensions::WebRequestResourceType::SUB_FRAME, true);
  EXPECT_TRUE(WebRequestPermissions::HideRequest(
      extension_info_map_.get(), WebRequestInfo(std::move(info_params_2))));

  // Sub-frame navigations initiated from a non-sensitive domain should not be
  // hidden.
  WebRequestInfoInitParams info_params_3 = create_request_params(
      url::Origin::Create(example_com), content::ResourceType::kSubFrame,
      extensions::WebRequestResourceType::SUB_FRAME, true);
  EXPECT_FALSE(WebRequestPermissions::HideRequest(
      extension_info_map_.get(), WebRequestInfo(std::move(info_params_3))));
}

TEST_F(ExtensionWebRequestHelpersTestWithThreadsTest,
       TestCanExtensionAccessURL_HostPermissions) {
  // Request with empty initiator.
  std::unique_ptr<net::URLRequest> request(
      context.CreateRequest(GURL("http://example.com"), net::DEFAULT_PRIORITY,
                            NULL, TRAFFIC_ANNOTATION_FOR_TESTS));

  const content::ResourceType kResourceType =
      content::ResourceType::kSubResource;

  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            WebRequestPermissions::CanExtensionAccessURL(
                extension_info_map_.get(), permissionless_extension_->id(),
                request->url(),
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::DO_NOT_CHECK_HOST, request->initiator(),
                kResourceType));
  EXPECT_EQ(PermissionsData::PageAccess::kDenied,
            WebRequestPermissions::CanExtensionAccessURL(
                extension_info_map_.get(), permissionless_extension_->id(),
                request->url(),
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL,
                request->initiator(), kResourceType));
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            WebRequestPermissions::CanExtensionAccessURL(
                extension_info_map_.get(), com_extension_->id(), request->url(),
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL,
                request->initiator(), kResourceType));
  EXPECT_EQ(
      PermissionsData::PageAccess::kAllowed,
      WebRequestPermissions::CanExtensionAccessURL(
          extension_info_map_.get(), com_extension_->id(), request->url(),
          -1,     // No tab id.
          false,  // crosses_incognito
          WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR,
          request->initiator(), kResourceType));
  EXPECT_EQ(PermissionsData::PageAccess::kDenied,
            WebRequestPermissions::CanExtensionAccessURL(
                extension_info_map_.get(), com_extension_->id(), request->url(),
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::REQUIRE_ALL_URLS, request->initiator(),
                kResourceType));

  std::unique_ptr<net::URLRequest> request_with_initiator(
      context.CreateRequest(GURL("http://example.com"), net::DEFAULT_PRIORITY,
                            nullptr, TRAFFIC_ANNOTATION_FOR_TESTS));
  request_with_initiator->set_initiator(
      url::Origin::Create(GURL("http://www.example.org")));

  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            WebRequestPermissions::CanExtensionAccessURL(
                extension_info_map_.get(), permissionless_extension_->id(),
                request_with_initiator->url(),
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::DO_NOT_CHECK_HOST,
                request_with_initiator->initiator(), kResourceType));
  EXPECT_EQ(PermissionsData::PageAccess::kDenied,
            WebRequestPermissions::CanExtensionAccessURL(
                extension_info_map_.get(), permissionless_extension_->id(),
                request_with_initiator->url(),
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL,
                request_with_initiator->initiator(), kResourceType));
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            WebRequestPermissions::CanExtensionAccessURL(
                extension_info_map_.get(), com_extension_->id(),
                request_with_initiator->url(),
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL,
                request_with_initiator->initiator(), kResourceType));
  // Doesn't have access to the initiator.
  EXPECT_EQ(
      PermissionsData::PageAccess::kDenied,
      WebRequestPermissions::CanExtensionAccessURL(
          extension_info_map_.get(), com_extension_->id(),
          request_with_initiator->url(),
          -1,     // No tab id.
          false,  // crosses_incognito
          WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR,
          request_with_initiator->initiator(), kResourceType));
  // Navigation requests don't need access to the initiator.
  EXPECT_EQ(
      PermissionsData::PageAccess::kAllowed,
      WebRequestPermissions::CanExtensionAccessURL(
          extension_info_map_.get(), com_extension_->id(),
          request_with_initiator->url(),
          -1,     // No tab id.
          false,  // crosses_incognito
          WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR,
          request_with_initiator->initiator(),
          content::ResourceType::kSubFrame));

  EXPECT_EQ(PermissionsData::PageAccess::kDenied,
            WebRequestPermissions::CanExtensionAccessURL(
                extension_info_map_.get(), com_extension_->id(),
                request_with_initiator->url(),
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::REQUIRE_ALL_URLS,
                request_with_initiator->initiator(), kResourceType));

  // Public Sessions tests.
#if defined(OS_CHROMEOS)
  std::unique_ptr<net::URLRequest> org_request(context.CreateRequest(
      GURL("http://example.org"), net::DEFAULT_PRIORITY, nullptr));

  // com_extension_ doesn't have host permission for .org URLs.
  EXPECT_EQ(PermissionsData::PageAccess::kDenied,
            WebRequestPermissions::CanExtensionAccessURL(
                extension_info_map_.get(), com_policy_extension_->id(),
                org_request->url(),
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL,
                org_request->initiator(), kResourceType));

  chromeos::ScopedTestPublicSessionLoginState login_state;

  // Host permission checks are disabled in Public Sessions, instead all URLs
  // are whitelisted.
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            WebRequestPermissions::CanExtensionAccessURL(
                extension_info_map_.get(), com_policy_extension_->id(),
                org_request->url(),
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL,
                org_request->initiator(), kResourceType));

  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            WebRequestPermissions::CanExtensionAccessURL(
                extension_info_map_.get(), com_policy_extension_->id(),
                org_request->url(),
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::REQUIRE_ALL_URLS,
                org_request->initiator(), kResourceType));

  // Make sure that chrome:// URLs cannot be accessed.
  std::unique_ptr<net::URLRequest> chrome_request(
      context.CreateRequest(GURL("chrome://version/"), net::DEFAULT_PRIORITY,
                            nullptr, TRAFFIC_ANNOTATION_FOR_TESTS));

  EXPECT_EQ(PermissionsData::PageAccess::kDenied,
            WebRequestPermissions::CanExtensionAccessURL(
                extension_info_map_.get(), com_policy_extension_->id(),
                chrome_request->url(),
                -1,     // No tab id.
                false,  // crosses_incognito
                WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL,
                chrome_request->initiator(), kResourceType));
#endif
}

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_permissions.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

TEST(ExtensionWebRequestPermissions, TestHideRequestForURL) {
  enum HideRequestMask {
    HIDE_NONE = 0,
    HIDE_RENDERER_REQUEST = 1,
    HIDE_SUB_FRAME_NAVIGATION = 2,
    HIDE_MAIN_FRAME_NAVIGATION = 4,
    HIDE_BROWSER_SUB_RESOURCE_REQUEST = 8,
    HIDE_ALL = HIDE_RENDERER_REQUEST | HIDE_SUB_FRAME_NAVIGATION |
               HIDE_MAIN_FRAME_NAVIGATION | HIDE_BROWSER_SUB_RESOURCE_REQUEST,
  };

  ExtensionsAPIClient api_client;
  auto info_map = base::MakeRefCounted<extensions::InfoMap>();

  struct TestCase {
    const char* url;
    int expected_hide_request_mask;
  } cases[] = {
      {"https://www.google.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"http://www.example.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"https://www.example.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"https://clients.google.com",
       HIDE_BROWSER_SUB_RESOURCE_REQUEST | HIDE_SUB_FRAME_NAVIGATION},
      {"http://clients4.google.com",
       HIDE_BROWSER_SUB_RESOURCE_REQUEST | HIDE_SUB_FRAME_NAVIGATION},
      {"https://clients4.google.com",
       HIDE_BROWSER_SUB_RESOURCE_REQUEST | HIDE_SUB_FRAME_NAVIGATION},
      {"https://clients9999.google.com",
       HIDE_BROWSER_SUB_RESOURCE_REQUEST | HIDE_SUB_FRAME_NAVIGATION},
      {"https://clients9999..google.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"https://clients9999.example.google.com",
       HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"https://clients.google.com.",
       HIDE_BROWSER_SUB_RESOURCE_REQUEST | HIDE_SUB_FRAME_NAVIGATION},
      {"https://.clients.google.com.",
       HIDE_BROWSER_SUB_RESOURCE_REQUEST | HIDE_SUB_FRAME_NAVIGATION},
      {"https://test.clients.google.com",
       HIDE_SUB_FRAME_NAVIGATION | HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"http://google.example.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"http://www.example.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"https://www.example.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"https://sb-ssl.google.com", HIDE_ALL},
      {"https://sb-ssl.random.google.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"https://safebrowsing.googleapis.com", HIDE_ALL},
      // Unsupported scheme.
      {"blob:https://safebrowsing.googleapis.com/"
       "fc3f440b-78ed-469f-8af8-7a1717ff39ae",
       HIDE_ALL},
      {"filesystem:https://safebrowsing.googleapis.com/path", HIDE_ALL},
      {"https://safebrowsing.googleapis.com.", HIDE_ALL},
      {"https://safebrowsing.googleapis.com/v4", HIDE_ALL},
      {"https://safebrowsing.googleapis.com:80/v4", HIDE_ALL},
      {"https://safebrowsing.googleapis.com./v4", HIDE_ALL},
      {"https://safebrowsing.googleapis.com/v5", HIDE_ALL},
      {"https://safebrowsing.google.com/safebrowsing", HIDE_ALL},
      {"https://safebrowsing.google.com/safebrowsing/anything", HIDE_ALL},
      {"https://safebrowsing.google.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"https://chrome.google.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"http://www.google.com/", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"https://chrome.google.com/webstore", HIDE_ALL},
      {"https://chrome.google.com./webstore", HIDE_ALL},
      {"https://chrome.google.com./webstore/", HIDE_ALL},
      // Unsupported scheme.
      {"blob:https://chrome.google.com/fc3f440b-78ed-469f-8af8-7a1717ff39ae",
       HIDE_ALL},
      {"notregisteredscheme://www.foobar.com", HIDE_ALL},
      {"https://chrome.google.com:80/webstore", HIDE_ALL},
      {"https://chrome.google.com/webstore?query", HIDE_ALL},
      {"http://clients2.google.com/service/update2/crx", HIDE_ALL},
      {"https://clients2.google.com/service/update2/crx", HIDE_ALL},
      {"https://chrome.google.com/webstore/inlineinstall/detail/"
       "kcnhkahnjcbndmmehfkdnkjomaanaooo",
       HIDE_ALL},
  };
  const int kRendererProcessId = 1;
  const int kBrowserProcessId = -1;

  // Returns a WebRequestInfoInitParams instance constructed as per the given
  // parameters.
  auto create_request_params = [](const GURL& url, content::ResourceType type,
                                  int render_process_id) {
    WebRequestInfoInitParams request;
    request.url = url;
    request.type = type;
    request.render_process_id = render_process_id;

    request.web_request_type = ToWebRequestResourceType(type);
    request.is_browser_side_navigation =
        type == content::ResourceType::kMainFrame ||
        type == content::ResourceType::kSubFrame;
    return request;
  };

  for (const TestCase& test_case : cases) {
    SCOPED_TRACE(test_case.url);

    GURL request_url(test_case.url);
    ASSERT_TRUE(request_url.is_valid());

    {
      SCOPED_TRACE("Renderer initiated sub-resource request");
      WebRequestInfo request(create_request_params(
          request_url, content::ResourceType::kSubResource,
          kRendererProcessId));
      bool expect_hidden =
          test_case.expected_hide_request_mask & HIDE_RENDERER_REQUEST;
      EXPECT_EQ(expect_hidden,
                WebRequestPermissions::HideRequest(info_map.get(), request));
    }

    {
      SCOPED_TRACE("Browser initiated sub-resource request");
      WebRequestInfo request(create_request_params(
          request_url, content::ResourceType::kSubResource, kBrowserProcessId));
      bool expect_hidden = test_case.expected_hide_request_mask &
                           HIDE_BROWSER_SUB_RESOURCE_REQUEST;
      EXPECT_EQ(expect_hidden,
                WebRequestPermissions::HideRequest(info_map.get(), request));
    }

    {
      SCOPED_TRACE("Main-frame navigation");
      WebRequestInfo request(create_request_params(
          request_url, content::ResourceType::kMainFrame, kBrowserProcessId));
      bool expect_hidden =
          test_case.expected_hide_request_mask & HIDE_MAIN_FRAME_NAVIGATION;
      EXPECT_EQ(expect_hidden,
                WebRequestPermissions::HideRequest(info_map.get(), request));
    }

    {
      SCOPED_TRACE("Sub-frame navigation");
      WebRequestInfo request(create_request_params(
          request_url, content::ResourceType::kSubFrame, kBrowserProcessId));
      bool expect_hidden =
          test_case.expected_hide_request_mask & HIDE_SUB_FRAME_NAVIGATION;
      EXPECT_EQ(expect_hidden,
                WebRequestPermissions::HideRequest(info_map.get(), request));
    }
  }

  // Check protection of requests originating from the frame showing the Chrome
  // WebStore. Normally this request is not protected:
  GURL non_sensitive_url("http://www.google.com/test.js");

  {
    WebRequestInfo non_sensitive_request(create_request_params(
        non_sensitive_url, content::ResourceType::kScript, kRendererProcessId));
    EXPECT_FALSE(WebRequestPermissions::HideRequest(info_map.get(),
                                                    non_sensitive_request));
  }

  // If the origin is labeled by the WebStoreAppId, it becomes protected.
  {
    const int kWebstoreProcessId = 42;
    const int kSiteInstanceId = 23;
    info_map->RegisterExtensionProcess(extensions::kWebStoreAppId,
                                       kWebstoreProcessId, kSiteInstanceId);
    WebRequestInfo sensitive_request_info(create_request_params(
        non_sensitive_url, content::ResourceType::kScript, kWebstoreProcessId));
    EXPECT_TRUE(WebRequestPermissions::HideRequest(info_map.get(),
                                                   sensitive_request_info));
  }

  {
    // Check that a request for a non-sensitive URL is rejected if it's a PAC
    // script fetch.
    WebRequestInfoInitParams non_sensitive_request_params =
        create_request_params(non_sensitive_url, content::ResourceType::kScript,
                              kRendererProcessId);
    non_sensitive_request_params.is_pac_request = true;
    EXPECT_TRUE(WebRequestPermissions::HideRequest(
        info_map.get(),
        WebRequestInfo(std::move(non_sensitive_request_params))));
  }
}

TEST(ExtensionWebRequestPermissions,
     CanExtensionAccessURLWithWithheldPermissions) {
  // The InfoMap requires methods to be called on the IO thread. Fake it.
  content::TestBrowserThreadBundle thread_bundle(
      content::TestBrowserThreadBundle::IO_MAINLOOP);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("ext").AddPermission("<all_urls>").Build();
  URLPatternSet all_urls(
      {URLPattern(Extension::kValidHostPermissionSchemes, "<all_urls>")});
  // Simulate withholding the <all_urls> permission.
  extension->permissions_data()->SetPermissions(
      std::make_unique<PermissionSet>(),  // active permissions.
      std::make_unique<PermissionSet>(
          APIPermissionSet(), ManifestPermissionSet(), all_urls.Clone(),
          URLPatternSet()) /* withheld permissions */);

  scoped_refptr<InfoMap> info_map = base::MakeRefCounted<InfoMap>();
  info_map->AddExtension(extension.get(), base::Time(), false, false);

  auto get_access = [extension, info_map](
                        const GURL& url,
                        const base::Optional<url::Origin>& initiator,
                        const base::Optional<content::ResourceType>&
                            resource_type) {
    constexpr int kTabId = 42;
    constexpr WebRequestPermissions::HostPermissionsCheck kPermissionsCheck =
        WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL;
    return WebRequestPermissions::CanExtensionAccessURL(
        info_map.get(), extension->id(), url, kTabId,
        false /* crosses incognito */, kPermissionsCheck, initiator,
        resource_type);
  };

  const GURL example_com("https://example.com");
  const GURL chromium_org("https://chromium.org");
  const url::Origin example_com_origin(url::Origin::Create(example_com));
  const url::Origin chromium_org_origin(url::Origin::Create(chromium_org));

  GURL urls[] = {example_com, chromium_org};
  base::Optional<url::Origin> initiators[] = {base::nullopt, example_com_origin,
                                              chromium_org_origin};
  base::Optional<content::ResourceType> resource_types[] = {
      base::nullopt, content::ResourceType::kSubResource,
      content::ResourceType::kMainFrame};

  // With all permissions withheld, the result of any request should be
  // kWithheld.
  for (const auto& url : urls) {
    for (const auto& initiator : initiators) {
      for (const auto& resource_type : resource_types) {
        EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
                  get_access(url, initiator, resource_type));
      }
    }
  }

  // Grant access to chromium.org.
  URLPatternSet chromium_org_patterns({URLPattern(
      Extension::kValidHostPermissionSchemes, "https://chromium.org/*")});
  extension->permissions_data()->SetPermissions(
      std::make_unique<PermissionSet>(
          APIPermissionSet(), ManifestPermissionSet(),
          std::move(chromium_org_patterns), URLPatternSet()),
      std::make_unique<PermissionSet>(APIPermissionSet(),
                                      ManifestPermissionSet(), all_urls.Clone(),
                                      URLPatternSet()));

  // example.com isn't granted, so without an initiator or with an initiator
  // that the extension doesn't have access to, access is withheld.
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(example_com, base::nullopt,
                       content::ResourceType::kSubResource));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(example_com, example_com_origin,
                       content::ResourceType::kMainFrame));

  // However, if a sub-resource request is made to example.com from an initiator
  // that the extension has access to, access is allowed. This is functionally
  // necessary for any extension with webRequest to work with the runtime host
  // permissions feature. See https://crbug.com/851722.
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            get_access(example_com, chromium_org_origin,
                       content::ResourceType::kSubResource));
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            get_access(example_com, chromium_org_origin, base::nullopt));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(example_com, chromium_org_origin,
                       content::ResourceType::kSubFrame));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(example_com, chromium_org_origin,
                       content::ResourceType::kMainFrame));

  // With access to the requested origin, access is always allowed for
  // REQUIRE_HOST_PERMISSION_FOR_URL, independent of initiator.
  for (const auto& initiator : initiators) {
    for (const auto& resource_type : resource_types) {
      EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
                get_access(chromium_org, initiator, resource_type));
    }
  }
}

TEST(ExtensionWebRequestPermissions,
     RequireAccessToURLAndInitiatorWithWithheldPermissions) {
  // The InfoMap requires methods to be called on the IO thread. Fake it.
  content::TestBrowserThreadBundle thread_bundle(
      content::TestBrowserThreadBundle::IO_MAINLOOP);
  const char* kGoogleCom = "https://google.com/";
  const char* kExampleCom = "https://example.com/";
  const char* kYahooCom = "https://yahoo.com";

  // Set up the extension to have access to kGoogleCom and withheld access to
  // kExampleCom.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("ext").AddPermissions({kGoogleCom, kExampleCom}).Build();

  URLPatternSet kActivePatternSet(
      {URLPattern(Extension::kValidHostPermissionSchemes, kGoogleCom)});
  URLPatternSet kWithheldPatternSet(
      {URLPattern(Extension::kValidHostPermissionSchemes, kExampleCom)});

  extension->permissions_data()->SetPermissions(
      std::make_unique<PermissionSet>(
          APIPermissionSet(), ManifestPermissionSet(),
          kActivePatternSet.Clone(),
          kActivePatternSet.Clone()),  // active permissions.
      std::make_unique<PermissionSet>(
          APIPermissionSet(), ManifestPermissionSet(),
          kWithheldPatternSet.Clone(),
          kWithheldPatternSet.Clone()) /* withheld permissions */);

  scoped_refptr<InfoMap> info_map = base::MakeRefCounted<InfoMap>();
  info_map->AddExtension(extension.get(), base::Time(), false, false);

  auto get_access = [extension, info_map](
                        const GURL& url,
                        const base::Optional<url::Origin>& initiator,
                        const base::Optional<content::ResourceType>&
                            resource_type) {
    constexpr int kTabId = 42;
    constexpr WebRequestPermissions::HostPermissionsCheck kPermissionsCheck =
        WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR;
    return WebRequestPermissions::CanExtensionAccessURL(
        info_map.get(), extension->id(), url, kTabId,
        false /* crosses incognito */, kPermissionsCheck, initiator,
        resource_type);
  };

  using PageAccess = PermissionsData::PageAccess;
  const GURL kAllowedUrl(kGoogleCom);
  const GURL kWithheldUrl(kExampleCom);
  const GURL kDeniedUrl(kYahooCom);
  const url::Origin kAllowedOrigin(url::Origin::Create(kAllowedUrl));
  const url::Origin kWithheldOrigin(url::Origin::Create(kWithheldUrl));
  const url::Origin kDeniedOrigin(url::Origin::Create(kDeniedUrl));
  const url::Origin kOpaqueOrigin;
  struct {
    base::Optional<url::Origin> initiator;
    GURL url;
    PermissionsData::PageAccess expected_access_subresource;
    PermissionsData::PageAccess expected_access_navigation;
  } cases[] = {
      {base::nullopt, kAllowedUrl, PageAccess::kAllowed, PageAccess::kAllowed},
      {base::nullopt, kWithheldUrl, PageAccess::kWithheld,
       PageAccess::kWithheld},
      {base::nullopt, kDeniedUrl, PageAccess::kDenied, PageAccess::kDenied},

      {kOpaqueOrigin, kAllowedUrl, PageAccess::kAllowed, PageAccess::kAllowed},
      {kOpaqueOrigin, kWithheldUrl, PageAccess::kWithheld,
       PageAccess::kWithheld},
      {kOpaqueOrigin, kDeniedUrl, PageAccess::kDenied, PageAccess::kDenied},

      {kDeniedOrigin, kAllowedUrl, PageAccess::kDenied, PageAccess::kAllowed},
      {kDeniedOrigin, kWithheldUrl, PageAccess::kDenied, PageAccess::kWithheld},
      {kDeniedOrigin, kDeniedUrl, PageAccess::kDenied, PageAccess::kDenied},
      {kAllowedOrigin, kDeniedUrl, PageAccess::kDenied, PageAccess::kDenied},
      {kWithheldOrigin, kDeniedUrl, PageAccess::kDenied, PageAccess::kDenied},

      {kWithheldOrigin, kWithheldUrl, PageAccess::kWithheld,
       PageAccess::kWithheld},
      {kWithheldOrigin, kAllowedUrl, PageAccess::kWithheld,
       PageAccess::kAllowed},
      {kAllowedOrigin, kWithheldUrl, PageAccess::kAllowed,
       PageAccess::kWithheld},
      {kAllowedOrigin, kAllowedUrl, PageAccess::kAllowed, PageAccess::kAllowed},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(base::StringPrintf(
        "url-%s initiator-%s", test_case.url.spec().c_str(),
        test_case.initiator ? test_case.initiator->Serialize().c_str()
                            : "empty"));
    EXPECT_EQ(get_access(test_case.url, test_case.initiator,
                         content::ResourceType::kSubResource),
              test_case.expected_access_subresource);
    EXPECT_EQ(get_access(test_case.url, test_case.initiator, base::nullopt),
              test_case.expected_access_subresource);
    EXPECT_EQ(get_access(test_case.url, test_case.initiator,
                         content::ResourceType::kSubFrame),
              test_case.expected_access_navigation);
    EXPECT_EQ(get_access(test_case.url, test_case.initiator,
                         content::ResourceType::kMainFrame),
              test_case.expected_access_navigation);
  }
}

}  // namespace extensions

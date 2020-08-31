// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/power_monitor_test_base.h"
#include "base/test/test_file_util.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_content_verifier_delegate.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/previews_state.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/content_verifier/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/file_util.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/test_extension_dir.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

using blink::mojom::ResourceType;
using extensions::ExtensionRegistry;
using network::mojom::URLLoader;

namespace extensions {
namespace {

base::FilePath GetTestPath(const std::string& name) {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
  return path.AppendASCII("extensions").AppendASCII(name);
}

base::FilePath GetContentVerifierTestPath() {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(extensions::DIR_TEST_DATA, &path));
  return path.AppendASCII("content_hash_fetcher")
      .AppendASCII("different_sized_files");
}

scoped_refptr<Extension> CreateTestExtension(const std::string& name,
                                             bool incognito_split_mode) {
  base::DictionaryValue manifest;
  manifest.SetString("name", name);
  manifest.SetString("version", "1");
  manifest.SetInteger("manifest_version", 2);
  manifest.SetString("incognito", incognito_split_mode ? "split" : "spanning");

  base::FilePath path = GetTestPath("response_headers");

  std::string error;
  scoped_refptr<Extension> extension(
      Extension::Create(path, Manifest::INTERNAL, manifest,
                        Extension::NO_FLAGS, &error));
  EXPECT_TRUE(extension.get()) << error;
  return extension;
}

scoped_refptr<Extension> CreateWebStoreExtension() {
  std::unique_ptr<base::DictionaryValue> manifest =
      DictionaryBuilder()
          .Set("name", "WebStore")
          .Set("version", "1")
          .Set("manifest_version", 2)
          .Set("icons",
               DictionaryBuilder().Set("16", "webstore_icon_16.png").Build())
          .Set("web_accessible_resources",
               ListBuilder().Append("webstore_icon_16.png").Build())
          .Build();

  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_RESOURCES, &path));
  path = path.AppendASCII("web_store");

  std::string error;
  scoped_refptr<Extension> extension(Extension::Create(
      path, Manifest::COMPONENT, *manifest, Extension::NO_FLAGS, &error));
  EXPECT_TRUE(extension.get()) << error;
  return extension;
}

scoped_refptr<const Extension> CreateTestResponseHeaderExtension() {
  return ExtensionBuilder("An extension with web-accessible resources")
      .SetManifestKey("web_accessible_resources",
                      ListBuilder().Append("test.dat").Build())
      .SetPath(GetTestPath("response_headers"))
      .Build();
}

// Helper function to create a |ResourceRequest| for testing purposes.
network::ResourceRequest CreateResourceRequest(const std::string& method,
                                               ResourceType resource_type,
                                               const GURL& url) {
  network::ResourceRequest request;
  request.method = method;
  request.url = url;
  request.site_for_cookies =
      net::SiteForCookies::FromUrl(url);  // bypass third-party cookie blocking.
  request.request_initiator =
      url::Origin::Create(url);  // ensure initiator set.
  request.referrer_policy = content::Referrer::GetDefaultReferrerPolicy();
  request.resource_type = static_cast<int>(resource_type);
  request.is_main_frame =
      resource_type == blink::mojom::ResourceType::kMainFrame;
  return request;
}

// The result of either a URLRequest of a URLLoader response (but not both)
// depending on the on test type.
class GetResult {
 public:
  GetResult(network::mojom::URLResponseHeadPtr response, int result)
      : response_(std::move(response)), result_(result) {}
  GetResult(GetResult&& other) : result_(other.result_) {}
  ~GetResult() = default;

  std::string GetResponseHeaderByName(const std::string& name) const {
    std::string value;
    if (response_ && response_->headers)
      response_->headers->GetNormalizedHeader(name, &value);
    return value;
  }

  int result() const { return result_; }

 private:
  network::mojom::URLResponseHeadPtr response_;
  int result_;

  DISALLOW_COPY_AND_ASSIGN(GetResult);
};

}  // namespace

// This test lives in src/chrome instead of src/extensions because it tests
// functionality delegated back to Chrome via ChromeExtensionsBrowserClient.
// See chrome/browser/extensions/chrome_url_request_util.cc.
class ExtensionProtocolsTestBase : public testing::Test {
 public:
  explicit ExtensionProtocolsTestBase(bool force_incognito)
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        rvh_test_enabler_(new content::RenderViewHostTestEnabler()),
        force_incognito_(force_incognito) {}

  void SetUp() override {
    testing::Test::SetUp();
    testing_profile_ = TestingProfile::Builder().Build();
    contents_ = CreateTestWebContents();

    // Set up content verification.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(
        switches::kExtensionContentVerification,
        switches::kExtensionContentVerificationEnforce);
    content_verifier_ = new ContentVerifier(
        browser_context(),
        std::make_unique<ChromeContentVerifierDelegate>(browser_context()));
    info_map()->SetContentVerifier(content_verifier_.get());
  }

  void TearDown() override {
    loader_factory_.reset();
    content_verifier_->Shutdown();
    // Shut down the PowerMonitor if initialized.
    base::PowerMonitor::ShutdownForTesting();
  }

  void SetProtocolHandler(bool is_incognito) {
    loader_factory_ = extensions::CreateExtensionNavigationURLLoaderFactory(
        browser_context(), false);
  }

  GetResult RequestOrLoad(const GURL& url, ResourceType resource_type) {
    return LoadURL(url, resource_type);
  }

  void AddExtension(const scoped_refptr<const Extension>& extension,
                    bool incognito_enabled,
                    bool notifications_disabled) {
    info_map()->AddExtension(extension.get(), base::Time::Now(),
                             incognito_enabled, notifications_disabled);
    EXPECT_TRUE(extension_registry()->AddEnabled(extension));
    ExtensionPrefs::Get(browser_context())
        ->SetIsIncognitoEnabled(extension->id(), incognito_enabled);
  }

  void RemoveExtension(const scoped_refptr<const Extension>& extension,
                       const UnloadedExtensionReason reason) {
    info_map()->RemoveExtension(extension->id(), reason);
    EXPECT_TRUE(extension_registry()->RemoveEnabled(extension->id()));
    if (reason == UnloadedExtensionReason::DISABLE)
      EXPECT_TRUE(extension_registry()->AddDisabled(extension));
  }

  // Helper method to create a URL request/loader, call RequestOrLoad on it, and
  // return the result. If |extension| hasn't already been added to
  // info_map(), this will add it.
  GetResult DoRequestOrLoad(const scoped_refptr<Extension> extension,
                            const std::string& relative_path) {
    if (!info_map()->extensions().Contains(extension->id())) {
      AddExtension(extension.get(),
                   /*incognito_enabled=*/false,
                   /*notifications_disabled=*/false);
    }
    return RequestOrLoad(extension->GetResourceURL(relative_path),
                         blink::mojom::ResourceType::kMainFrame);
  }

  ExtensionRegistry* extension_registry() {
    return ExtensionRegistry::Get(browser_context());
  }

  InfoMap* info_map() {
    return ExtensionSystem::Get(browser_context())->info_map();
  }

  content::BrowserContext* browser_context() {
    return force_incognito_ ? testing_profile_->GetOffTheRecordProfile()
                            : testing_profile_.get();
  }

  void SimulateSystemSuspendForRequests() {
    power_monitor_source_ = new base::PowerMonitorTestSource();
    base::PowerMonitor::Initialize(
        std::unique_ptr<base::PowerMonitorSource>(power_monitor_source_));
  }

 protected:
  scoped_refptr<ContentVerifier> content_verifier_;

 private:
  GetResult LoadURL(const GURL& url, ResourceType resource_type) {
    constexpr int32_t kRoutingId = 81;
    constexpr int32_t kRequestId = 28;

    mojo::PendingRemote<network::mojom::URLLoader> loader;
    network::TestURLLoaderClient client;
    loader_factory_->CreateLoaderAndStart(
        loader.InitWithNewPipeAndPassReceiver(), kRoutingId, kRequestId,
        network::mojom::kURLLoadOptionNone,
        CreateResourceRequest("GET", resource_type, url), client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    if (power_monitor_source_) {
      power_monitor_source_->GenerateSuspendEvent();
      power_monitor_source_->GenerateResumeEvent();
    }

    client.RunUntilComplete();
    return GetResult(client.response_head().Clone(),
                     client.completion_status().error_code);
  }

  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    auto site_instance = content::SiteInstance::Create(browser_context());
    return content::WebContentsTester::CreateTestWebContents(
        browser_context(), std::move(site_instance));
  }

  content::WebContents* web_contents() { return contents_.get(); }

  content::RenderFrameHost* main_rfh() {
    return web_contents()->GetMainFrame();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::RenderViewHostTestEnabler> rvh_test_enabler_;
  std::unique_ptr<network::mojom::URLLoaderFactory> loader_factory_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<content::WebContents> contents_;
  const bool force_incognito_;

  // |power_monitor_source_| is owned by the global PowerMonitor.
  base::PowerMonitorTestSource* power_monitor_source_ = nullptr;
};

class ExtensionProtocolsTest : public ExtensionProtocolsTestBase {
 public:
  ExtensionProtocolsTest()
      : ExtensionProtocolsTestBase(false /*force_incognito*/) {}
};

class ExtensionProtocolsIncognitoTest : public ExtensionProtocolsTestBase {
 public:
  ExtensionProtocolsIncognitoTest()
      : ExtensionProtocolsTestBase(true /*force_incognito*/) {}
};

// Tests that making a chrome-extension request in an incognito context is
// only allowed under the right circumstances (if the extension is allowed
// in incognito, and it's either a non-main-frame request or a split-mode
// extension).
TEST_F(ExtensionProtocolsIncognitoTest, IncognitoRequest) {
  // Register an incognito extension protocol handler.
  SetProtocolHandler(true);

  struct TestCase {
    // Inputs.
    std::string name;
    bool incognito_split_mode;
    bool incognito_enabled;

    // Expected results.
    bool should_allow_main_frame_load;
    bool should_allow_sub_frame_load;
  } cases[] = {
    {"spanning disabled", false, false, false, false},
    {"split disabled", true, false, false, false},
    {"spanning enabled", false, true, false, false},
    {"split enabled", true, true, true, false},
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    scoped_refptr<Extension> extension =
        CreateTestExtension(cases[i].name, cases[i].incognito_split_mode);
    AddExtension(extension, cases[i].incognito_enabled, false);

    // First test a main frame request.
    {
      // It doesn't matter that the resource doesn't exist. If the resource
      // is blocked, we should see BLOCKED_BY_CLIENT. Otherwise, the request
      // should just fail because the file doesn't exist.
      auto get_result = RequestOrLoad(extension->GetResourceURL("404.html"),
                                      blink::mojom::ResourceType::kMainFrame);

      if (cases[i].should_allow_main_frame_load) {
        EXPECT_EQ(net::ERR_FILE_NOT_FOUND, get_result.result())
            << cases[i].name;
      } else {
        EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, get_result.result())
            << cases[i].name;
      }
    }

    // Subframe navigation requests are blocked in ExtensionNavigationThrottle
    // which isn't added in this unit test. This is tested in an integration
    // test in ExtensionResourceRequestPolicyTest.IframeNavigateToInaccessible.
    RemoveExtension(extension, UnloadedExtensionReason::UNINSTALL);
  }
}

void CheckForContentLengthHeader(const GetResult& get_result) {
  std::string content_length = get_result.GetResponseHeaderByName(
      net::HttpRequestHeaders::kContentLength);

  EXPECT_FALSE(content_length.empty());
  int length_value = 0;
  EXPECT_TRUE(base::StringToInt(content_length, &length_value));
  EXPECT_GT(length_value, 0);
}

#if defined(OS_CHROMEOS)
// Tests getting a resource for a component extension works correctly where
// there is no mime type. Such a resource currently only exists for Chrome OS
// build.
TEST_F(ExtensionProtocolsTest, ComponentResourceRequestNoMimeType) {
  SetProtocolHandler(false);
  std::unique_ptr<base::DictionaryValue> manifest =
      DictionaryBuilder()
          .Set("name", "pdf")
          .Set("version", "1")
          .Set("manifest_version", 2)
          .Set("web_accessible_resources",
               // Registered by chrome_component_extension_resource_manager.cc
               ListBuilder().Append("ink/glcore_base.js.mem").Build())
          .Build();

  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_RESOURCES, &path));
  path = path.AppendASCII("pdf");

  std::string error;
  scoped_refptr<Extension> extension(Extension::Create(
      path, Manifest::COMPONENT, *manifest, Extension::NO_FLAGS, &error));
  EXPECT_TRUE(extension.get()) << error;
  AddExtension(extension, false, false);

  auto get_result =
      RequestOrLoad(extension->GetResourceURL("ink/glcore_base.js.mem"),
                    blink::mojom::ResourceType::kXhr);
  EXPECT_EQ(net::OK, get_result.result());
  CheckForContentLengthHeader(get_result);
  EXPECT_EQ("", get_result.GetResponseHeaderByName(
                    net::HttpRequestHeaders::kContentType));
}
#endif

// Tests getting a resource for a component extension works correctly, both when
// the extension is enabled and when it is disabled.
TEST_F(ExtensionProtocolsTest, ComponentResourceRequest) {
  // Register a non-incognito extension protocol handler.
  SetProtocolHandler(false);

  scoped_refptr<Extension> extension = CreateWebStoreExtension();
  AddExtension(extension, false, false);

  // First test it with the extension enabled.
  {
    auto get_result =
        RequestOrLoad(extension->GetResourceURL("webstore_icon_16.png"),
                      blink::mojom::ResourceType::kMedia);
    EXPECT_EQ(net::OK, get_result.result());
    CheckForContentLengthHeader(get_result);
    EXPECT_EQ("image/png", get_result.GetResponseHeaderByName(
                               net::HttpRequestHeaders::kContentType));
  }

  // And then test it with the extension disabled.
  RemoveExtension(extension, UnloadedExtensionReason::DISABLE);
  {
    auto get_result =
        RequestOrLoad(extension->GetResourceURL("webstore_icon_16.png"),
                      blink::mojom::ResourceType::kMedia);
    EXPECT_EQ(net::OK, get_result.result());
    CheckForContentLengthHeader(get_result);
    EXPECT_EQ("image/png", get_result.GetResponseHeaderByName(
                               net::HttpRequestHeaders::kContentType));
  }
}

// Tests that a URL request for resource from an extension returns a few
// expected response headers.
TEST_F(ExtensionProtocolsTest, ResourceRequestResponseHeaders) {
  // Register a non-incognito extension protocol handler.
  SetProtocolHandler(false);

  scoped_refptr<const Extension> extension =
      CreateTestResponseHeaderExtension();
  AddExtension(extension, false, false);

  {
    auto get_result = RequestOrLoad(extension->GetResourceURL("test.dat"),
                                    blink::mojom::ResourceType::kMedia);
    EXPECT_EQ(net::OK, get_result.result());

    // Check that cache-related headers are set.
    std::string etag = get_result.GetResponseHeaderByName("ETag");
    EXPECT_TRUE(base::StartsWith(etag, "\"", base::CompareCase::SENSITIVE));
    EXPECT_TRUE(base::EndsWith(etag, "\"", base::CompareCase::SENSITIVE));

    std::string revalidation_header =
        get_result.GetResponseHeaderByName("cache-control");
    EXPECT_EQ("no-cache", revalidation_header);

    // We set test.dat as web-accessible, so it should have a CORS header.
    std::string access_control =
        get_result.GetResponseHeaderByName("Access-Control-Allow-Origin");
    EXPECT_EQ("*", access_control);
  }
}

// Tests that a URL request for main frame or subframe from an extension
// succeeds, but subresources fail. See http://crbug.com/312269.
TEST_F(ExtensionProtocolsTest, AllowFrameRequests) {
  // Register a non-incognito extension protocol handler.
  SetProtocolHandler(false);

  scoped_refptr<Extension> extension = CreateTestExtension("foo", false);
  AddExtension(extension, false, false);

  // All MAIN_FRAME requests should succeed. SUB_FRAME requests that are not
  // explicitly listed in web_accessible_resources or same-origin to the parent
  // should not succeed.
  {
    auto get_result = RequestOrLoad(extension->GetResourceURL("test.dat"),
                                    blink::mojom::ResourceType::kMainFrame);
    EXPECT_EQ(net::OK, get_result.result());
  }

  // Subframe navigation requests are blocked in ExtensionNavigationThrottle
  // which isn't added in this unit test. This is tested in an integration test
  // in ExtensionResourceRequestPolicyTest.IframeNavigateToInaccessible.

  // And subresource types, such as media, should fail.
  {
    auto get_result = RequestOrLoad(extension->GetResourceURL("test.dat"),
                                    blink::mojom::ResourceType::kMedia);
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, get_result.result());
  }
}

TEST_F(ExtensionProtocolsTest, MetadataFolder) {
  SetProtocolHandler(false);

  base::FilePath extension_dir = GetTestPath("metadata_folder");
  std::string error;
  scoped_refptr<Extension> extension =
      file_util::LoadExtension(extension_dir, Manifest::INTERNAL,
                               Extension::NO_FLAGS, &error);
  ASSERT_NE(extension.get(), nullptr) << "error: " << error;

  // Loading "/test.html" should succeed.
  EXPECT_EQ(net::OK, DoRequestOrLoad(extension, "test.html").result());

  // Loading "/_metadata/verified_contents.json" should fail.
  base::FilePath relative_path =
      base::FilePath(kMetadataFolder).Append(kVerifiedContentsFilename);
  EXPECT_TRUE(base::PathExists(extension_dir.Append(relative_path)));
  EXPECT_NE(net::OK,
            DoRequestOrLoad(extension, relative_path.AsUTF8Unsafe()).result());

  // Loading "/_metadata/a.txt" should also fail.
  relative_path = base::FilePath(kMetadataFolder).AppendASCII("a.txt");
  EXPECT_TRUE(base::PathExists(extension_dir.Append(relative_path)));
  EXPECT_NE(net::OK,
            DoRequestOrLoad(extension, relative_path.AsUTF8Unsafe()).result());
}

// Tests that unreadable files and deleted files correctly go through
// ContentVerifyJob.
TEST_F(ExtensionProtocolsTest, VerificationSeenForFileAccessErrors) {
  SetProtocolHandler(false);

  // Unzip extension containing verification hashes to a temporary directory.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath unzipped_path = temp_dir.GetPath();
  scoped_refptr<Extension> extension =
      content_verifier_test_utils::UnzipToDirAndLoadExtension(
          GetContentVerifierTestPath().AppendASCII("source.zip"),
          unzipped_path);
  ASSERT_TRUE(extension.get());
  ExtensionId extension_id = extension->id();

  const std::string kJs("1024.js");
  base::FilePath kRelativePath(FILE_PATH_LITERAL("1024.js"));

  // Valid and readable 1024.js.
  {
    TestContentVerifySingleJobObserver observer(extension_id, kRelativePath);

    content_verifier_->OnExtensionLoaded(browser_context(), extension.get());
    // Wait for PostTask to ContentVerifierIOData::AddData() to finish.
    content::RunAllPendingInMessageLoop();

    EXPECT_EQ(net::OK, DoRequestOrLoad(extension, kJs).result());
    EXPECT_EQ(ContentVerifyJob::NONE, observer.WaitForJobFinished());
  }

  // chmod -r 1024.js.
  {
    TestContentVerifySingleJobObserver observer(extension_id, kRelativePath);
    base::FilePath file_path = unzipped_path.AppendASCII(kJs);
    ASSERT_TRUE(base::MakeFileUnreadable(file_path));
    EXPECT_EQ(net::ERR_ACCESS_DENIED, DoRequestOrLoad(extension, kJs).result());
    EXPECT_EQ(ContentVerifyJob::HASH_MISMATCH, observer.WaitForJobFinished());
    // NOTE: In production, hash mismatch would have disabled |extension|, but
    // since UnzipToDirAndLoadExtension() doesn't add the extension to
    // ExtensionRegistry, ChromeContentVerifierDelegate won't disable it.
    // TODO(lazyboy): We may want to update this to more closely reflect the
    // real flow.
  }

  // Delete 1024.js.
  {
    TestContentVerifySingleJobObserver observer(extension_id, kRelativePath);
    base::FilePath file_path = unzipped_path.AppendASCII(kJs);
    ASSERT_TRUE(base::DieFileDie(file_path, false));
    EXPECT_EQ(net::ERR_FILE_NOT_FOUND,
              DoRequestOrLoad(extension, kJs).result());
    EXPECT_EQ(ContentVerifyJob::HASH_MISMATCH, observer.WaitForJobFinished());
  }
}

// Tests that zero byte files correctly go through ContentVerifyJob.
TEST_F(ExtensionProtocolsTest, VerificationSeenForZeroByteFile) {
  SetProtocolHandler(false);

  const std::string kEmptyJs("empty.js");
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath unzipped_path = temp_dir.GetPath();

  scoped_refptr<Extension> extension =
      content_verifier_test_utils::UnzipToDirAndLoadExtension(
          GetContentVerifierTestPath().AppendASCII("source.zip"),
          unzipped_path);
  ASSERT_TRUE(extension.get());

  base::FilePath kRelativePath(FILE_PATH_LITERAL("empty.js"));
  ExtensionId extension_id = extension->id();

  // Sanity check empty.js.
  base::FilePath file_path = unzipped_path.AppendASCII(kEmptyJs);
  int64_t foo_file_size = -1;
  ASSERT_TRUE(base::GetFileSize(file_path, &foo_file_size));
  ASSERT_EQ(0, foo_file_size);

  // Request empty.js.
  {
    TestContentVerifySingleJobObserver observer(extension_id, kRelativePath);

    content_verifier_->OnExtensionLoaded(browser_context(), extension.get());
    // Wait for PostTask to ContentVerifierIOData::AddData() to finish.
    content::RunAllPendingInMessageLoop();

    EXPECT_EQ(net::OK, DoRequestOrLoad(extension, kEmptyJs).result());
    EXPECT_EQ(ContentVerifyJob::NONE, observer.WaitForJobFinished());
  }

  // chmod -r empty.js.
  // Unreadable empty file doesn't generate hash mismatch. Note that this is the
  // current behavior of ContentVerifyJob.
  // TODO(lazyboy): The behavior is probably incorrect.
  {
    TestContentVerifySingleJobObserver observer(extension_id, kRelativePath);
    base::FilePath file_path = unzipped_path.AppendASCII(kEmptyJs);
    ASSERT_TRUE(base::MakeFileUnreadable(file_path));
    EXPECT_EQ(net::ERR_ACCESS_DENIED,
              DoRequestOrLoad(extension, kEmptyJs).result());
    EXPECT_EQ(ContentVerifyJob::NONE, observer.WaitForJobFinished());
  }

  // rm empty.js.
  // Deleted empty file doesn't generate hash mismatch. Note that this is the
  // current behavior of ContentVerifyJob.
  // TODO(lazyboy): The behavior is probably incorrect.
  {
    TestContentVerifySingleJobObserver observer(extension_id, kRelativePath);
    base::FilePath file_path = unzipped_path.AppendASCII(kEmptyJs);
    ASSERT_TRUE(base::DieFileDie(file_path, false));
    EXPECT_EQ(net::ERR_FILE_NOT_FOUND,
              DoRequestOrLoad(extension, kEmptyJs).result());
    EXPECT_EQ(ContentVerifyJob::NONE, observer.WaitForJobFinished());
  }
}

TEST_F(ExtensionProtocolsTest, VerifyScriptListedAsIcon) {
  SetProtocolHandler(false);

  const std::string kBackgroundJs("background.js");
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath unzipped_path = temp_dir.GetPath();

  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(extensions::DIR_TEST_DATA, &path));

  scoped_refptr<Extension> extension =
      content_verifier_test_utils::UnzipToDirAndLoadExtension(
          path.AppendASCII("content_hash_fetcher")
              .AppendASCII("manifest_mislabeled_script")
              .AppendASCII("source.zip"),
          unzipped_path);
  ASSERT_TRUE(extension.get());

  base::FilePath kRelativePath(FILE_PATH_LITERAL("background.js"));
  ExtensionId extension_id = extension->id();

  // Request background.js.
  {
    TestContentVerifySingleJobObserver observer(extension_id, kRelativePath);

    content_verifier_->OnExtensionLoaded(browser_context(), extension.get());
    // Wait for PostTask to ContentVerifierIOData::AddData() to finish.
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(net::OK, DoRequestOrLoad(extension, kBackgroundJs).result());
    EXPECT_EQ(ContentVerifyJob::NONE, observer.WaitForJobFinished());
  }

  // Modify background.js and request it.
  {
    base::FilePath file_path = unzipped_path.AppendASCII("background.js");
    const std::string content = "new content";
    EXPECT_TRUE(base::WriteFile(file_path, content));
    TestContentVerifySingleJobObserver observer(extension_id, kRelativePath);

    content_verifier_->OnExtensionLoaded(browser_context(), extension.get());
    // Wait for PostTask to ContentVerifierIOData::AddData() to finish.
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(net::OK, DoRequestOrLoad(extension, kBackgroundJs).result());
    EXPECT_EQ(ContentVerifyJob::HASH_MISMATCH, observer.WaitForJobFinished());
  }
}

// Tests that mime types are properly set for returned extension resources.
TEST_F(ExtensionProtocolsTest, MimeTypesForKnownFiles) {
  // Register a non-incognito extension protocol handler.
  SetProtocolHandler(false);

  TestExtensionDir test_dir;
  constexpr char kManifest[] = R"(
      {
        "name": "Test Ext",
        "description": "A test extension",
        "manifest_version": 2,
        "version": "0.1",
        "web_accessible_resources": ["*"]
      })";
  test_dir.WriteManifest(kManifest);
  std::unique_ptr<base::DictionaryValue> manifest =
      base::DictionaryValue::From(base::test::ParseJsonDeprecated(kManifest));
  ASSERT_TRUE(manifest);

  test_dir.WriteFile(FILE_PATH_LITERAL("json_file.json"), "{}");
  test_dir.WriteFile(FILE_PATH_LITERAL("js_file.js"), "function() {}");

  base::FilePath unpacked_path = test_dir.UnpackedPath();
  ASSERT_TRUE(base::PathExists(unpacked_path.AppendASCII("json_file.json")));
  std::string error;
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetPath(unpacked_path)
          .SetLocation(Manifest::INTERNAL)
          .Build();
  ASSERT_TRUE(extension);

  AddExtension(extension.get(), false, false);

  struct {
    const char* file_name;
    const char* expected_mime_type;
  } test_cases[] = {
      {"json_file.json", "application/json"},
      {"js_file.js", "text/javascript"},
      {"mem_file.mem", ""},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.file_name);
    auto result = RequestOrLoad(extension->GetResourceURL(test_case.file_name),
                                blink::mojom::ResourceType::kSubResource);
    EXPECT_EQ(
        test_case.expected_mime_type,
        result.GetResponseHeaderByName(net::HttpRequestHeaders::kContentType));
  }
}

#if defined(OS_WIN)
#define MAYBE_ExtensionRequestsNotAborted DISABLED_ExtensionRequestsNotAborted
#else
#define MAYBE_ExtensionRequestsNotAborted ExtensionRequestsNotAborted
#endif
// Tests that requests for extension resources (including the generated
// background page) are not aborted on system suspend.
//
// Flaky on Windows.
// TODO(https://crbug.com/921687): Investigate and fix.
TEST_F(ExtensionProtocolsTest, MAYBE_ExtensionRequestsNotAborted) {
  // Register a non-incognito extension protocol handler.
  SetProtocolHandler(false);

  base::FilePath extension_dir =
      GetTestPath("common").AppendASCII("background_script");
  std::string error;
  scoped_refptr<Extension> extension = file_util::LoadExtension(
      extension_dir, Manifest::INTERNAL, Extension::NO_FLAGS, &error);
  ASSERT_TRUE(extension.get()) << error;

  SimulateSystemSuspendForRequests();

  // Request the generated background page. Ensure the request completes
  // successfully.
  EXPECT_EQ(net::OK,
            DoRequestOrLoad(extension.get(), kGeneratedBackgroundPageFilename)
                .result());

  // Request the background.js file. Ensure the request completes successfully.
  EXPECT_EQ(net::OK, DoRequestOrLoad(extension.get(), "background.js").result());
}

}  // namespace extensions

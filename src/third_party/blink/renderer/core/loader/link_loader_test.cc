// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/link_loader.h"

#include <base/macros.h>
#include <memory>
#include "base/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/link_rel_attribute.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/link_loader_client.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/loader/network_hints_interface.h"
#include "third_party/blink/renderer/core/testing/dummy_modulator.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {

class MockLinkLoaderClient final
    : public GarbageCollectedFinalized<MockLinkLoaderClient>,
      public LinkLoaderClient {
  USING_GARBAGE_COLLECTED_MIXIN(MockLinkLoaderClient);

 public:
  static MockLinkLoaderClient* Create(bool should_load) {
    return MakeGarbageCollected<MockLinkLoaderClient>(should_load);
  }

  explicit MockLinkLoaderClient(bool should_load) : should_load_(should_load) {}

  void Trace(blink::Visitor* visitor) override {
    LinkLoaderClient::Trace(visitor);
  }

  bool ShouldLoadLink() override { return should_load_; }
  bool IsLinkCreatedByParser() override { return true; }

  void LinkLoaded() override {}
  void LinkLoadingErrored() override {}
  void DidStartLinkPrerender() override {}
  void DidStopLinkPrerender() override {}
  void DidSendLoadForLinkPrerender() override {}
  void DidSendDOMContentLoadedForLinkPrerender() override {}

  scoped_refptr<base::SingleThreadTaskRunner> GetLoadingTaskRunner() override {
    return Thread::Current()->GetTaskRunner();
  }

 private:
  const bool should_load_;
};

class NetworkHintsMock : public NetworkHintsInterface {
 public:
  NetworkHintsMock() = default;

  void DnsPrefetchHost(const String& host) const override {
    did_dns_prefetch_ = true;
  }

  void PreconnectHost(
      const KURL& host,
      const CrossOriginAttributeValue cross_origin) const override {
    did_preconnect_ = true;
    is_https_ = host.ProtocolIs("https");
    is_cross_origin_ = (cross_origin == kCrossOriginAttributeAnonymous);
  }

  bool DidDnsPrefetch() { return did_dns_prefetch_; }
  bool DidPreconnect() { return did_preconnect_; }
  bool IsHTTPS() { return is_https_; }
  bool IsCrossOrigin() { return is_cross_origin_; }

 private:
  mutable bool did_dns_prefetch_ = false;
  mutable bool did_preconnect_ = false;
  mutable bool is_https_ = false;
  mutable bool is_cross_origin_ = false;
};

class LinkLoaderPreloadTestBase : public testing::Test {
 public:
  struct Expectations {
    ResourceLoadPriority priority;
    mojom::RequestContextType context;
    bool link_loader_should_load_value;
    KURL load_url;
    network::mojom::ReferrerPolicy referrer_policy;
  };

  LinkLoaderPreloadTestBase() {
    dummy_page_holder_ = DummyPageHolder::Create(IntSize(500, 500));
  }

  ~LinkLoaderPreloadTestBase() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

 protected:
  void TestPreload(const LinkLoadParameters& params,
                   const Expectations& expected) {
    ResourceFetcher* fetcher = dummy_page_holder_->GetDocument().Fetcher();
    ASSERT_TRUE(fetcher);
    dummy_page_holder_->GetFrame().GetSettings()->SetScriptEnabled(true);
    Persistent<MockLinkLoaderClient> loader_client =
        MockLinkLoaderClient::Create(expected.link_loader_should_load_value);
    LinkLoader* loader = LinkLoader::Create(loader_client.Get());
    url_test_helpers::RegisterMockedErrorURLLoad(params.href);
    loader->LoadLink(params, dummy_page_holder_->GetDocument(),
                     NetworkHintsMock());
    if (!expected.load_url.IsNull() &&
        expected.priority != ResourceLoadPriority::kUnresolved) {
      ASSERT_EQ(1, fetcher->CountPreloads());
      Resource* resource = loader->GetResourceForTesting();
      ASSERT_NE(resource, nullptr);
      EXPECT_EQ(expected.load_url.GetString(), resource->Url().GetString());
      EXPECT_TRUE(fetcher->ContainsAsPreload(resource));
      EXPECT_EQ(expected.priority, resource->GetResourceRequest().Priority());
      EXPECT_EQ(expected.context,
                resource->GetResourceRequest().GetRequestContext());
      if (expected.referrer_policy !=
          network::mojom::ReferrerPolicy::kDefault) {
        EXPECT_EQ(expected.referrer_policy,
                  resource->GetResourceRequest().GetReferrerPolicy());
      }
    } else {
      ASSERT_EQ(0, fetcher->CountPreloads());
    }
  }
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

struct PreloadTestParams {
  const char* href;
  const char* as;
  const ResourceLoadPriority priority;
  const mojom::RequestContextType context;
  const bool expecting_load;
};

constexpr PreloadTestParams kPreloadTestParams[] = {
    {"http://example.test/cat.jpg", "image", ResourceLoadPriority::kLow,
     mojom::RequestContextType::IMAGE, true},
    {"http://example.test/cat.js", "script", ResourceLoadPriority::kHigh,
     mojom::RequestContextType::SCRIPT, true},
    {"http://example.test/cat.css", "style", ResourceLoadPriority::kVeryHigh,
     mojom::RequestContextType::STYLE, true},
    // TODO(yoav): It doesn't seem like the audio context is ever used. That
    // should probably be fixed (or we can consolidate audio and video).
    //
    // Until the preload cache is defined in terms of range requests and media
    // fetches we can't reliably preload audio/video content and expect it to be
    // served from the cache correctly. Until
    // https://github.com/w3c/preload/issues/97 is resolved and implemented we
    // need to disable these preloads.
    {"http://example.test/cat.wav", "audio", ResourceLoadPriority::kLow,
     mojom::RequestContextType::AUDIO, false},
    {"http://example.test/cat.mp4", "video", ResourceLoadPriority::kLow,
     mojom::RequestContextType::VIDEO, false},
    {"http://example.test/cat.vtt", "track", ResourceLoadPriority::kLow,
     mojom::RequestContextType::TRACK, true},
    {"http://example.test/cat.woff", "font", ResourceLoadPriority::kHigh,
     mojom::RequestContextType::FONT, true},
    // TODO(yoav): subresource should be *very* low priority (rather than
    // low).
    {"http://example.test/cat.empty", "fetch", ResourceLoadPriority::kHigh,
     mojom::RequestContextType::SUBRESOURCE, true},
    {"http://example.test/cat.blob", "blabla", ResourceLoadPriority::kLow,
     mojom::RequestContextType::SUBRESOURCE, false},
    {"http://example.test/cat.blob", "", ResourceLoadPriority::kLow,
     mojom::RequestContextType::SUBRESOURCE, false},
    {"bla://example.test/cat.gif", "image", ResourceLoadPriority::kUnresolved,
     mojom::RequestContextType::IMAGE, false}};

class LinkLoaderPreloadTest
    : public LinkLoaderPreloadTestBase,
      public testing::WithParamInterface<PreloadTestParams> {};

TEST_P(LinkLoaderPreloadTest, Preload) {
  const auto& test_case = GetParam();
  LinkLoadParameters params(
      LinkRelAttribute("preload"), kCrossOriginAttributeNotSet, String(),
      test_case.as, String(), String(), String(), String(),
      network::mojom::ReferrerPolicy::kDefault, KURL(NullURL(), test_case.href),
      String(), String());
  Expectations expectations = {
      test_case.priority, test_case.context, test_case.expecting_load,
      test_case.expecting_load ? params.href : NullURL(),
      network::mojom::ReferrerPolicy::kDefault};
  TestPreload(params, expectations);
}

INSTANTIATE_TEST_SUITE_P(LinkLoaderPreloadTest,
                         LinkLoaderPreloadTest,
                         testing::ValuesIn(kPreloadTestParams));

struct PreloadMimeTypeTestParams {
  const char* href;
  const char* as;
  const char* type;
  const ResourceLoadPriority priority;
  const mojom::RequestContextType context;
  const bool expecting_load;
};

constexpr PreloadMimeTypeTestParams kPreloadMimeTypeTestParams[] = {
    {"http://example.test/cat.webp", "image", "image/webp",
     ResourceLoadPriority::kLow, mojom::RequestContextType::IMAGE, true},
    {"http://example.test/cat.svg", "image", "image/svg+xml",
     ResourceLoadPriority::kLow, mojom::RequestContextType::IMAGE, true},
    {"http://example.test/cat.jxr", "image", "image/jxr",
     ResourceLoadPriority::kUnresolved, mojom::RequestContextType::IMAGE,
     false},
    {"http://example.test/cat.js", "script", "text/javascript",
     ResourceLoadPriority::kHigh, mojom::RequestContextType::SCRIPT, true},
    {"http://example.test/cat.js", "script", "text/coffeescript",
     ResourceLoadPriority::kUnresolved, mojom::RequestContextType::SCRIPT,
     false},
    {"http://example.test/cat.css", "style", "text/css",
     ResourceLoadPriority::kVeryHigh, mojom::RequestContextType::STYLE, true},
    {"http://example.test/cat.css", "style", "text/sass",
     ResourceLoadPriority::kUnresolved, mojom::RequestContextType::STYLE,
     false},
    // Until the preload cache is defined in terms of range requests and media
    // fetches we can't reliably preload audio/video content and expect it to be
    // served from the cache correctly. Until
    // https://github.com/w3c/preload/issues/97 is resolved and implemented we
    // need to disable these preloads.
    {"http://example.test/cat.wav", "audio", "audio/wav",
     ResourceLoadPriority::kLow, mojom::RequestContextType::AUDIO, false},
    {"http://example.test/cat.wav", "audio", "audio/mp57",
     ResourceLoadPriority::kUnresolved, mojom::RequestContextType::AUDIO,
     false},
    {"http://example.test/cat.webm", "video", "video/webm",
     ResourceLoadPriority::kLow, mojom::RequestContextType::VIDEO, false},
    {"http://example.test/cat.mp199", "video", "video/mp199",
     ResourceLoadPriority::kUnresolved, mojom::RequestContextType::VIDEO,
     false},
    {"http://example.test/cat.vtt", "track", "text/vtt",
     ResourceLoadPriority::kLow, mojom::RequestContextType::TRACK, true},
    {"http://example.test/cat.vtt", "track", "text/subtitlething",
     ResourceLoadPriority::kUnresolved, mojom::RequestContextType::TRACK,
     false},
    {"http://example.test/cat.woff", "font", "font/woff2",
     ResourceLoadPriority::kHigh, mojom::RequestContextType::FONT, true},
    {"http://example.test/cat.woff", "font", "font/woff84",
     ResourceLoadPriority::kUnresolved, mojom::RequestContextType::FONT, false},
    {"http://example.test/cat.empty", "fetch", "foo/bar",
     ResourceLoadPriority::kHigh, mojom::RequestContextType::SUBRESOURCE, true},
    {"http://example.test/cat.blob", "blabla", "foo/bar",
     ResourceLoadPriority::kLow, mojom::RequestContextType::SUBRESOURCE, false},
    {"http://example.test/cat.blob", "", "foo/bar", ResourceLoadPriority::kLow,
     mojom::RequestContextType::SUBRESOURCE, false}};

class LinkLoaderPreloadMimeTypeTest
    : public LinkLoaderPreloadTestBase,
      public testing::WithParamInterface<PreloadMimeTypeTestParams> {};

TEST_P(LinkLoaderPreloadMimeTypeTest, Preload) {
  const auto& test_case = GetParam();
  LinkLoadParameters params(
      LinkRelAttribute("preload"), kCrossOriginAttributeNotSet, test_case.type,
      test_case.as, String(), String(), String(), String(),
      network::mojom::ReferrerPolicy::kDefault, KURL(NullURL(), test_case.href),
      String(), String());
  Expectations expectations = {
      test_case.priority, test_case.context, test_case.expecting_load,
      test_case.expecting_load ? params.href : NullURL(),
      network::mojom::ReferrerPolicy::kDefault};
  TestPreload(params, expectations);
}

INSTANTIATE_TEST_SUITE_P(LinkLoaderPreloadMimeTypeTest,
                         LinkLoaderPreloadMimeTypeTest,
                         testing::ValuesIn(kPreloadMimeTypeTestParams));

struct PreloadMediaTestParams {
  const char* media;
  const ResourceLoadPriority priority;
  const bool link_loader_should_load_value;
  const bool expecting_load;
};

constexpr PreloadMediaTestParams kPreloadMediaTestParams[] = {
    {"(max-width: 600px)", ResourceLoadPriority::kLow, true, true},
    {"(max-width: 400px)", ResourceLoadPriority::kUnresolved, true, false},
    {"(max-width: 600px)", ResourceLoadPriority::kLow, false, false}};

class LinkLoaderPreloadMediaTest
    : public LinkLoaderPreloadTestBase,
      public testing::WithParamInterface<PreloadMediaTestParams> {};

TEST_P(LinkLoaderPreloadMediaTest, Preload) {
  const auto& test_case = GetParam();
  LinkLoadParameters params(
      LinkRelAttribute("preload"), kCrossOriginAttributeNotSet, "image/gif",
      "image", test_case.media, String(), String(), String(),
      network::mojom::ReferrerPolicy::kDefault,
      KURL(NullURL(), "http://example.test/cat.gif"), String(), String());
  Expectations expectations = {
      test_case.priority, mojom::RequestContextType::IMAGE,
      test_case.link_loader_should_load_value,
      test_case.expecting_load ? params.href : NullURL(),
      network::mojom::ReferrerPolicy::kDefault};
  TestPreload(params, expectations);
}

INSTANTIATE_TEST_SUITE_P(LinkLoaderPreloadMediaTest,
                         LinkLoaderPreloadMediaTest,
                         testing::ValuesIn(kPreloadMediaTestParams));

constexpr network::mojom::ReferrerPolicy kPreloadReferrerPolicyTestParams[] = {
    network::mojom::ReferrerPolicy::kOrigin,
    network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin,
    network::mojom::ReferrerPolicy::kSameOrigin,
    network::mojom::ReferrerPolicy::kStrictOrigin,
    network::mojom::ReferrerPolicy::
        kNoReferrerWhenDowngradeOriginWhenCrossOrigin,
    network::mojom::ReferrerPolicy::kNever};

class LinkLoaderPreloadReferrerPolicyTest
    : public LinkLoaderPreloadTestBase,
      public testing::WithParamInterface<network::mojom::ReferrerPolicy> {};

TEST_P(LinkLoaderPreloadReferrerPolicyTest, Preload) {
  const network::mojom::ReferrerPolicy referrer_policy = GetParam();
  LinkLoadParameters params(
      LinkRelAttribute("preload"), kCrossOriginAttributeNotSet, "image/gif",
      "image", String(), String(), String(), String(), referrer_policy,
      KURL(NullURL(), "http://example.test/cat.gif"), String(), String());
  Expectations expectations = {ResourceLoadPriority::kLow,
                               mojom::RequestContextType::IMAGE, true,
                               params.href, referrer_policy};
  TestPreload(params, expectations);
}

INSTANTIATE_TEST_SUITE_P(LinkLoaderPreloadReferrerPolicyTest,
                         LinkLoaderPreloadReferrerPolicyTest,
                         testing::ValuesIn(kPreloadReferrerPolicyTestParams));

struct PreloadNonceTestParams {
  const char* nonce;
  const char* content_security_policy;
  const bool expecting_load;
};

constexpr PreloadNonceTestParams kPreloadNonceTestParams[] = {
    {"abc", "script-src 'nonce-abc'", true},
    {"", "script-src 'nonce-abc'", false},
    {"def", "script-src 'nonce-abc'", false},
};

class LinkLoaderPreloadNonceTest
    : public LinkLoaderPreloadTestBase,
      public testing::WithParamInterface<PreloadNonceTestParams> {};

TEST_P(LinkLoaderPreloadNonceTest, Preload) {
  const auto& test_case = GetParam();
  dummy_page_holder_->GetDocument()
      .GetContentSecurityPolicy()
      ->DidReceiveHeader(test_case.content_security_policy,
                         kContentSecurityPolicyHeaderTypeEnforce,
                         kContentSecurityPolicyHeaderSourceHTTP);
  LinkLoadParameters params(
      LinkRelAttribute("preload"), kCrossOriginAttributeNotSet, String(),
      "script", String(), test_case.nonce, String(), String(),
      network::mojom::ReferrerPolicy::kDefault,
      KURL(NullURL(), "http://example.test/cat.js"), String(), String());
  Expectations expectations = {
      ResourceLoadPriority::kHigh, mojom::RequestContextType::SCRIPT,
      test_case.expecting_load,
      test_case.expecting_load ? params.href : NullURL(),
      network::mojom::ReferrerPolicy::kDefault};
  TestPreload(params, expectations);
}

INSTANTIATE_TEST_SUITE_P(LinkLoaderPreloadNonceTest,
                         LinkLoaderPreloadNonceTest,
                         testing::ValuesIn(kPreloadNonceTestParams));

struct PreloadImageSrcsetTestParams {
  const char* href;
  const char* image_srcset;
  const char* image_sizes;
  float scale_factor;
  const char* expected_url;
};

constexpr PreloadImageSrcsetTestParams kPreloadImageSrcsetTestParams[] = {
    {"http://example.test/cat.gif",
     "http://example.test/cat1x.gif 1x, http://example.test/cat2x.gif 2x",
     nullptr, 1.0, "http://example.test/cat1x.gif"},
    {"http://example.test/cat.gif",
     "http://example.test/cat1x.gif 1x, http://example.test/cat2x.gif 2x",
     nullptr, 2.0, "http://example.test/cat2x.gif"},
    {"http://example.test/cat.gif",
     "http://example.test/cat400.gif 400w, http://example.test/cat800.gif 800w",
     "400px", 1.0, "http://example.test/cat400.gif"},
    {"http://example.test/cat.gif",
     "http://example.test/cat400.gif 400w, http://example.test/cat800.gif 800w",
     "400px", 2.0, "http://example.test/cat800.gif"},
    {"http://example.test/cat.gif",
     "cat200.gif 200w, cat400.gif 400w, cat800.gif 800w", "200px", 1.0,
     "http://example.test/cat200.gif"},
};

class LinkLoaderPreloadImageSrcsetTest
    : public LinkLoaderPreloadTestBase,
      public testing::WithParamInterface<PreloadImageSrcsetTestParams> {};

TEST_P(LinkLoaderPreloadImageSrcsetTest, Preload) {
  const auto& test_case = GetParam();
  dummy_page_holder_->GetDocument().SetBaseURLOverride(
      KURL("http://example.test/"));
  dummy_page_holder_->GetPage().SetDeviceScaleFactorDeprecated(
      test_case.scale_factor);
  LinkLoadParameters params(
      LinkRelAttribute("preload"), kCrossOriginAttributeNotSet, "image/gif",
      "image", String(), String(), String(), String(),
      network::mojom::ReferrerPolicy::kDefault, KURL(NullURL(), test_case.href),
      test_case.image_srcset, test_case.image_sizes);
  Expectations expectations = {ResourceLoadPriority::kLow,
                               mojom::RequestContextType::IMAGE, true,
                               KURL(NullURL(), test_case.expected_url),
                               network::mojom::ReferrerPolicy::kDefault};
  TestPreload(params, expectations);
}

INSTANTIATE_TEST_SUITE_P(LinkLoaderPreloadImageSrcsetTest,
                         LinkLoaderPreloadImageSrcsetTest,
                         testing::ValuesIn(kPreloadImageSrcsetTestParams));

struct ModulePreloadTestParams {
  const char* href;
  const char* nonce;
  const char* integrity;
  CrossOriginAttributeValue cross_origin;
  network::mojom::ReferrerPolicy referrer_policy;
  bool expecting_load;
  network::mojom::FetchCredentialsMode expected_credentials_mode;
};

constexpr ModulePreloadTestParams kModulePreloadTestParams[] = {
    {"", nullptr, nullptr, kCrossOriginAttributeNotSet,
     network::mojom::ReferrerPolicy::kDefault, false,
     network::mojom::FetchCredentialsMode::kSameOrigin},
    {"http://example.test/cat.js", nullptr, nullptr,
     kCrossOriginAttributeNotSet, network::mojom::ReferrerPolicy::kDefault,
     true, network::mojom::FetchCredentialsMode::kSameOrigin},
    {"http://example.test/cat.js", nullptr, nullptr,
     kCrossOriginAttributeAnonymous, network::mojom::ReferrerPolicy::kDefault,
     true, network::mojom::FetchCredentialsMode::kSameOrigin},
    {"http://example.test/cat.js", "nonce", nullptr,
     kCrossOriginAttributeNotSet, network::mojom::ReferrerPolicy::kNever, true,
     network::mojom::FetchCredentialsMode::kSameOrigin},
    {"http://example.test/cat.js", nullptr, "sha384-abc",
     kCrossOriginAttributeNotSet, network::mojom::ReferrerPolicy::kDefault,
     true, network::mojom::FetchCredentialsMode::kSameOrigin}};

class LinkLoaderModulePreloadTest
    : public testing::TestWithParam<ModulePreloadTestParams> {};

class ModulePreloadTestModulator final : public DummyModulator {
 public:
  ModulePreloadTestModulator(const ModulePreloadTestParams* params)
      : params_(params), fetched_(false) {}

  void FetchSingle(const ModuleScriptFetchRequest& request,
                   ResourceFetcher*,
                   ModuleGraphLevel,
                   ModuleScriptCustomFetchType custom_fetch_type,
                   SingleModuleClient*) override {
    fetched_ = true;

    EXPECT_EQ(KURL(NullURL(), params_->href), request.Url());
    EXPECT_EQ(params_->nonce, request.Options().Nonce());
    EXPECT_EQ(kNotParserInserted, request.Options().ParserState());
    EXPECT_EQ(params_->expected_credentials_mode,
              request.Options().CredentialsMode());
    EXPECT_EQ(Referrer::NoReferrer(), request.ReferrerString());
    EXPECT_EQ(params_->referrer_policy, request.Options().GetReferrerPolicy());
    EXPECT_EQ(params_->integrity,
              request.Options().GetIntegrityAttributeValue());
    EXPECT_EQ(ModuleScriptCustomFetchType::kNone, custom_fetch_type);
  }

  bool fetched() const { return fetched_; }

 private:
  const ModulePreloadTestParams* params_;
  bool fetched_;
};

TEST_P(LinkLoaderModulePreloadTest, ModulePreload) {
  const auto& test_case = GetParam();
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create();
  ModulePreloadTestModulator* modulator =
      MakeGarbageCollected<ModulePreloadTestModulator>(&test_case);
  Modulator::SetModulator(
      ToScriptStateForMainWorld(dummy_page_holder->GetDocument().GetFrame()),
      modulator);
  Persistent<MockLinkLoaderClient> loader_client =
      MockLinkLoaderClient::Create(true);
  LinkLoader* loader = LinkLoader::Create(loader_client.Get());
  KURL href_url = KURL(NullURL(), test_case.href);
  LinkLoadParameters params(
      LinkRelAttribute("modulepreload"), test_case.cross_origin,
      String() /* type */, String() /* as */, String() /* media */,
      test_case.nonce, test_case.integrity, String(), test_case.referrer_policy,
      href_url, String() /* image_srcset */, String() /* image_sizes */);
  loader->LoadLink(params, dummy_page_holder->GetDocument(),
                   NetworkHintsMock());
  ASSERT_EQ(test_case.expecting_load, modulator->fetched());
}

INSTANTIATE_TEST_SUITE_P(LinkLoaderModulePreloadTest,
                         LinkLoaderModulePreloadTest,
                         testing::ValuesIn(kModulePreloadTestParams));

TEST(LinkLoaderTest, Prefetch) {
  struct TestCase {
    const char* href;
    // TODO(yoav): Add support for type and media crbug.com/662687
    const char* type;
    const char* media;
    const network::mojom::ReferrerPolicy referrer_policy;
    const bool link_loader_should_load_value;
    const bool expecting_load;
    const network::mojom::ReferrerPolicy expected_referrer_policy;
  } cases[] = {
      // Referrer Policy
      {"http://example.test/cat.jpg", "image/jpg", "",
       network::mojom::ReferrerPolicy::kOrigin, true, true,
       network::mojom::ReferrerPolicy::kOrigin},
      {"http://example.test/cat.jpg", "image/jpg", "",
       network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin, true, true,
       network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin},
      {"http://example.test/cat.jpg", "image/jpg", "",
       network::mojom::ReferrerPolicy::kNever, true, true,
       network::mojom::ReferrerPolicy::kNever},
  };

  // Test the cases with a single header
  for (const auto& test_case : cases) {
    std::unique_ptr<DummyPageHolder> dummy_page_holder =
        DummyPageHolder::Create(IntSize(500, 500));
    dummy_page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);
    Persistent<MockLinkLoaderClient> loader_client =
        MockLinkLoaderClient::Create(test_case.link_loader_should_load_value);
    LinkLoader* loader = LinkLoader::Create(loader_client.Get());
    KURL href_url = KURL(NullURL(), test_case.href);
    url_test_helpers::RegisterMockedErrorURLLoad(href_url);
    LinkLoadParameters params(
        LinkRelAttribute("prefetch"), kCrossOriginAttributeNotSet,
        test_case.type, "", test_case.media, "", "", String(),
        test_case.referrer_policy, href_url, String() /* image_srcset */,
        String() /* image_sizes */);
    loader->LoadLink(params, dummy_page_holder->GetDocument(),
                     NetworkHintsMock());
    ASSERT_TRUE(dummy_page_holder->GetDocument().Fetcher());
    Resource* resource = loader->GetResourceForTesting();
    if (test_case.expecting_load) {
      EXPECT_TRUE(resource);
    } else {
      EXPECT_FALSE(resource);
    }
    if (resource) {
      if (test_case.expected_referrer_policy !=
          network::mojom::ReferrerPolicy::kDefault) {
        EXPECT_EQ(test_case.expected_referrer_policy,
                  resource->GetResourceRequest().GetReferrerPolicy());
      }
    }
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }
}

TEST(LinkLoaderTest, DNSPrefetch) {
  struct {
    const char* href;
    const bool should_load;
  } cases[] = {
      {"http://example.com/", true},
      {"https://example.com/", true},
      {"//example.com/", true},
      {"//example.com/", false},
  };

  // Test the cases with a single header
  for (const auto& test_case : cases) {
    std::unique_ptr<DummyPageHolder> dummy_page_holder =
        DummyPageHolder::Create(IntSize(500, 500));
    dummy_page_holder->GetDocument().GetSettings()->SetDNSPrefetchingEnabled(
        true);
    Persistent<MockLinkLoaderClient> loader_client =
        MockLinkLoaderClient::Create(test_case.should_load);
    LinkLoader* loader = LinkLoader::Create(loader_client.Get());
    KURL href_url = KURL(KURL(String("http://example.com")), test_case.href);
    NetworkHintsMock network_hints;
    LinkLoadParameters params(
        LinkRelAttribute("dns-prefetch"), kCrossOriginAttributeNotSet, String(),
        String(), String(), String(), String(), String(),
        network::mojom::ReferrerPolicy::kDefault, href_url,
        String() /* image_srcset */, String() /* image_sizes */);
    loader->LoadLink(params, dummy_page_holder->GetDocument(), network_hints);
    EXPECT_FALSE(network_hints.DidPreconnect());
    EXPECT_EQ(test_case.should_load, network_hints.DidDnsPrefetch());
  }
}

TEST(LinkLoaderTest, Preconnect) {
  struct {
    const char* href;
    CrossOriginAttributeValue cross_origin;
    const bool should_load;
    const bool is_https;
    const bool is_cross_origin;
  } cases[] = {
      {"http://example.com/", kCrossOriginAttributeNotSet, true, false, false},
      {"https://example.com/", kCrossOriginAttributeNotSet, true, true, false},
      {"http://example.com/", kCrossOriginAttributeAnonymous, true, false,
       true},
      {"//example.com/", kCrossOriginAttributeNotSet, true, false, false},
      {"http://example.com/", kCrossOriginAttributeNotSet, false, false, false},
  };

  // Test the cases with a single header
  for (const auto& test_case : cases) {
    std::unique_ptr<DummyPageHolder> dummy_page_holder =
        DummyPageHolder::Create(IntSize(500, 500));
    Persistent<MockLinkLoaderClient> loader_client =
        MockLinkLoaderClient::Create(test_case.should_load);
    LinkLoader* loader = LinkLoader::Create(loader_client.Get());
    KURL href_url = KURL(KURL(String("http://example.com")), test_case.href);
    NetworkHintsMock network_hints;
    LinkLoadParameters params(
        LinkRelAttribute("preconnect"), test_case.cross_origin, String(),
        String(), String(), String(), String(), String(),
        network::mojom::ReferrerPolicy::kDefault, href_url,
        String() /* image_srcset */, String() /* image_sizes */);
    loader->LoadLink(params, dummy_page_holder->GetDocument(), network_hints);
    EXPECT_EQ(test_case.should_load, network_hints.DidPreconnect());
    EXPECT_EQ(test_case.is_https, network_hints.IsHTTPS());
    EXPECT_EQ(test_case.is_cross_origin, network_hints.IsCrossOrigin());
  }
}

TEST(LinkLoaderTest, PreloadAndPrefetch) {
  std::unique_ptr<DummyPageHolder> dummy_page_holder =
      DummyPageHolder::Create(IntSize(500, 500));
  ResourceFetcher* fetcher = dummy_page_holder->GetDocument().Fetcher();
  ASSERT_TRUE(fetcher);
  dummy_page_holder->GetFrame().GetSettings()->SetScriptEnabled(true);
  Persistent<MockLinkLoaderClient> loader_client =
      MockLinkLoaderClient::Create(true);
  LinkLoader* loader = LinkLoader::Create(loader_client.Get());
  KURL href_url = KURL(KURL(), "https://www.example.com/");
  url_test_helpers::RegisterMockedErrorURLLoad(href_url);
  LinkLoadParameters params(
      LinkRelAttribute("preload prefetch"), kCrossOriginAttributeNotSet,
      "application/javascript", "script", "", "", "", String(),
      network::mojom::ReferrerPolicy::kDefault, href_url,
      String() /* image_srcset */, String() /* image_sizes */);
  loader->LoadLink(params, dummy_page_holder->GetDocument(),
                   NetworkHintsMock());
  ASSERT_EQ(1, fetcher->CountPreloads());
  Resource* resource = loader->GetResourceForTesting();
  ASSERT_NE(resource, nullptr);
  EXPECT_TRUE(resource->IsLinkPreload());
}

}  // namespace

}  // namespace blink

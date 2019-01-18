// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/allowed_by_nosniff.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/loader/testing/test_resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

using MimeTypeCheck = AllowedByNosniff::MimeTypeCheck;
using WebFeature = mojom::WebFeature;
using ::testing::_;

class CountUsageMockFetchContext : public MockFetchContext {
 public:
  CountUsageMockFetchContext() : MockFetchContext(nullptr, nullptr) {}
  static CountUsageMockFetchContext* Create() {
    return MakeGarbageCollected<
        ::testing::StrictMock<CountUsageMockFetchContext>>();
  }
  MOCK_CONST_METHOD1(CountUsage, void(mojom::WebFeature));
};

class MockConsoleLogger : public GarbageCollectedFinalized<MockConsoleLogger>,
                          public ConsoleLogger {
  USING_GARBAGE_COLLECTED_MIXIN(MockConsoleLogger);

 public:
  MOCK_METHOD2(AddInfoMessage, void(Source, const String&));
  MOCK_METHOD2(AddWarningMessage, void(Source, const String&));
  MOCK_METHOD2(AddErrorMessage, void(Source, const String&));
};

}  // namespace

class AllowedByNosniffTest : public testing::Test {
 public:
  static scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner() {
    return base::MakeRefCounted<scheduler::FakeTaskRunner>();
  }
};

TEST_F(AllowedByNosniffTest, AllowedOrNot) {
  struct {
    const char* mimetype;
    bool allowed;
    bool strict_allowed;
  } data[] = {
      // Supported mimetypes:
      {"text/javascript", true, true},
      {"application/javascript", true, true},
      {"text/ecmascript", true, true},

      // Blocked mimetpyes:
      {"image/png", false, false},
      {"text/csv", false, false},
      {"video/mpeg", false, false},

      // Legacy mimetypes:
      {"text/html", true, false},
      {"text/plain", true, false},
      {"application/xml", true, false},
      {"application/octet-stream", true, false},

      // Potato mimetypes:
      {"text/potato", true, false},
      {"potato/text", true, false},
      {"aaa/aaa", true, false},
      {"zzz/zzz", true, false},

      // Parameterized mime types:
      {"text/javascript; charset=utf-8", true, true},
      {"text/javascript;charset=utf-8", true, true},
      {"text/javascript;bla;bla", true, true},
      {"text/csv; charset=utf-8", false, false},
      {"text/csv;charset=utf-8", false, false},
      {"text/csv;bla;bla", false, false},

      // Funky capitalization:
      {"text/html", true, false},
      {"Text/html", true, false},
      {"text/Html", true, false},
      {"TeXt/HtMl", true, false},
      {"TEXT/HTML", true, false},
  };

  for (auto& testcase : data) {
    SCOPED_TRACE(testing::Message()
                 << "\n  mime type: " << testcase.mimetype
                 << "\n  allowed: " << (testcase.allowed ? "true" : "false")
                 << "\n  strict_allowed: "
                 << (testcase.strict_allowed ? "true" : "false"));

    const KURL url("https://bla.com/");
    auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>(
        SecurityOrigin::Create(url));
    auto* context = CountUsageMockFetchContext::Create();
    // Bind |properties| to |context| through a ResourceFetcher.
    MakeGarbageCollected<ResourceFetcher>(
        ResourceFetcherInit(*properties, context, CreateTaskRunner()));
    Persistent<MockConsoleLogger> logger =
        MakeGarbageCollected<MockConsoleLogger>();
    ResourceResponse response(url);
    response.SetHTTPHeaderField("Content-Type", testcase.mimetype);

    // Nosniff 'legacy' setting: Both worker + non-worker obey the 'allowed'
    // setting. Warnings for any blocked script.
    RuntimeEnabledFeatures::SetWorkerNosniffBlockEnabled(false);
    RuntimeEnabledFeatures::SetWorkerNosniffWarnEnabled(false);
    EXPECT_CALL(*context, CountUsage(_)).Times(::testing::AnyNumber());
    if (!testcase.allowed)
      EXPECT_CALL(*logger, AddErrorMessage(_, _));
    EXPECT_EQ(testcase.allowed,
              AllowedByNosniff::MimeTypeAsScript(*context, logger, response,
                                                 MimeTypeCheck::kLax, false));
    ::testing::Mock::VerifyAndClear(context);

    EXPECT_CALL(*context, CountUsage(_)).Times(::testing::AnyNumber());
    if (!testcase.allowed)
      EXPECT_CALL(*logger, AddErrorMessage(_, _));
    EXPECT_EQ(testcase.allowed,
              AllowedByNosniff::MimeTypeAsScript(
                  *context, logger, response, MimeTypeCheck::kStrict, false));
    ::testing::Mock::VerifyAndClear(context);

    // Nosniff worker blocked: Workers follow the 'strict_allow' setting.
    // Warnings for any blocked scripts.
    RuntimeEnabledFeatures::SetWorkerNosniffBlockEnabled(true);
    RuntimeEnabledFeatures::SetWorkerNosniffWarnEnabled(false);

    EXPECT_CALL(*context, CountUsage(_)).Times(::testing::AnyNumber());
    if (!testcase.allowed)
      EXPECT_CALL(*logger, AddErrorMessage(_, _));
    EXPECT_EQ(testcase.allowed,
              AllowedByNosniff::MimeTypeAsScript(*context, logger, response,
                                                 MimeTypeCheck::kLax, false));
    ::testing::Mock::VerifyAndClear(context);

    EXPECT_CALL(*context, CountUsage(_)).Times(::testing::AnyNumber());
    if (!testcase.strict_allowed)
      EXPECT_CALL(*logger, AddErrorMessage(_, _));
    EXPECT_EQ(testcase.strict_allowed,
              AllowedByNosniff::MimeTypeAsScript(
                  *context, logger, response, MimeTypeCheck::kStrict, false));
    ::testing::Mock::VerifyAndClear(context);

    // Nosniff 'legacy', but with warnings. The allowed setting follows the
    // 'allowed' setting, but the warnings follow the 'strict' setting.
    RuntimeEnabledFeatures::SetWorkerNosniffBlockEnabled(false);
    RuntimeEnabledFeatures::SetWorkerNosniffWarnEnabled(true);

    EXPECT_CALL(*context, CountUsage(_)).Times(::testing::AnyNumber());
    if (!testcase.allowed)
      EXPECT_CALL(*logger, AddErrorMessage(_, _));
    EXPECT_EQ(testcase.allowed,
              AllowedByNosniff::MimeTypeAsScript(*context, logger, response,
                                                 MimeTypeCheck::kLax, false));
    ::testing::Mock::VerifyAndClear(context);

    EXPECT_CALL(*context, CountUsage(_)).Times(::testing::AnyNumber());
    if (!testcase.strict_allowed)
      EXPECT_CALL(*logger, AddErrorMessage(_, _));
    EXPECT_EQ(testcase.allowed,
              AllowedByNosniff::MimeTypeAsScript(
                  *context, logger, response, MimeTypeCheck::kStrict, false));
    ::testing::Mock::VerifyAndClear(context);
  }
}

TEST_F(AllowedByNosniffTest, Counters) {
  const char* bla = "https://bla.com";
  const char* blubb = "https://blubb.com";
  struct {
    const char* url;
    const char* origin;
    const char* mimetype;
    WebFeature expected;
  } data[] = {
      // Test same- vs cross-origin cases.
      {bla, "", "text/plain", WebFeature::kCrossOriginTextScript},
      {bla, "", "text/plain", WebFeature::kCrossOriginTextPlain},
      {bla, blubb, "text/plain", WebFeature::kCrossOriginTextScript},
      {bla, blubb, "text/plain", WebFeature::kCrossOriginTextPlain},
      {bla, bla, "text/plain", WebFeature::kSameOriginTextScript},
      {bla, bla, "text/plain", WebFeature::kSameOriginTextPlain},

      // Test mime type and subtype handling.
      {bla, bla, "text/xml", WebFeature::kSameOriginTextScript},
      {bla, bla, "text/xml", WebFeature::kSameOriginTextXml},

      // Test mime types from crbug.com/765544, with random cross/same site
      // origins.
      {bla, bla, "text/plain", WebFeature::kSameOriginTextPlain},
      {bla, blubb, "text/xml", WebFeature::kCrossOriginTextXml},
      {blubb, blubb, "application/octet-stream",
       WebFeature::kSameOriginApplicationOctetStream},
      {blubb, bla, "application/xml", WebFeature::kCrossOriginApplicationXml},
      {bla, bla, "text/html", WebFeature::kSameOriginTextHtml},
  };

  for (auto& testcase : data) {
    SCOPED_TRACE(testing::Message() << "\n  url: " << testcase.url
                                    << "\n  origin: " << testcase.origin
                                    << "\n  mime type: " << testcase.mimetype
                                    << "\n  webfeature: " << testcase.expected);
    auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>(
        SecurityOrigin::Create(KURL(testcase.origin)));
    auto* context = CountUsageMockFetchContext::Create();
    // Bind |properties| to |context| through a ResourceFetcher.
    MakeGarbageCollected<ResourceFetcher>(
        ResourceFetcherInit(*properties, context, CreateTaskRunner()));
    Persistent<MockConsoleLogger> logger =
        MakeGarbageCollected<MockConsoleLogger>();
    ResourceResponse response(KURL(testcase.url));
    response.SetHTTPHeaderField("Content-Type", testcase.mimetype);

    EXPECT_CALL(*context, CountUsage(testcase.expected));
    EXPECT_CALL(*context, CountUsage(::testing::Ne(testcase.expected)))
        .Times(::testing::AnyNumber());
    AllowedByNosniff::MimeTypeAsScript(*context, logger, response,
                                       MimeTypeCheck::kLax, false);
    ::testing::Mock::VerifyAndClear(context);
  }
}

TEST_F(AllowedByNosniffTest, AllTheSchemes) {
  // We test various URL schemes.
  // To force a decision based on the scheme, we give all responses an
  // invalid Content-Type plus a "nosniff" header. That way, all Content-Type
  // based checks are always denied and we can test for whether this is decided
  // based on the URL or not.
  struct {
    const char* url;
    bool allowed;
  } data[] = {
      {"http://example.com/bla.js", false},
      {"https://example.com/bla.js", false},
      {"file://etc/passwd.js", true},
      {"file://etc/passwd", false},
      {"chrome://dino/dino.js", true},
      {"chrome://dino/dino.css", false},
      {"ftp://example.com/bla.js", true},
      {"ftp://example.com/bla.txt", false},

      {"file://home/potato.txt", false},
      {"file://home/potato.js", true},
      {"file://home/potato.mjs", true},
      {"chrome://dino/dino.mjs", true},
  };

  for (auto& testcase : data) {
    auto* properties = MakeGarbageCollected<TestResourceFetcherProperties>();
    auto* context = CountUsageMockFetchContext::Create();
    // Bind |properties| to |context| through a ResourceFetcher.
    MakeGarbageCollected<ResourceFetcher>(
        ResourceFetcherInit(*properties, context, CreateTaskRunner()));
    Persistent<MockConsoleLogger> logger =
        MakeGarbageCollected<MockConsoleLogger>();
    EXPECT_CALL(*logger, AddErrorMessage(_, _)).Times(::testing::AnyNumber());
    SCOPED_TRACE(testing::Message() << "\n  url: " << testcase.url
                                    << "\n  allowed: " << testcase.allowed);
    ResourceResponse response(KURL(testcase.url));
    response.SetHTTPHeaderField("Content-Type", "invalid");
    response.SetHTTPHeaderField("X-Content-Type-Options", "nosniff");
    EXPECT_EQ(testcase.allowed,
              AllowedByNosniff::MimeTypeAsScript(*context, logger, response,
                                                 MimeTypeCheck::kLax, false));
  }
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/AllowedByNosniff.h"

#include "core/dom/Document.h"
#include "core/frame/UseCounter.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class AllowedByNosniffTest : public ::testing::Test {
 public:
  void SetUp() {
    // Create a new dummy page holder for each test, so that we get a fresh
    // set of counters for each.
    dummy_page_holder_ = DummyPageHolder::Create();
  }

  Document* doc() { return &dummy_page_holder_->GetDocument(); }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(AllowedByNosniffTest, SanityCheckSetUp) {
  // UseCounter counts will be silently swallowed under various conditions,
  // e.g. if the document doesn't actually hold a frame. This test is a sanity
  // test that UseCounter::Count + UseCounter::IsCounted work at all with the
  // current test setup. If this test fails, we know that the setup is wrong,
  // rather than the code under test.
  WebFeature f = WebFeature::kSameOriginTextScript;
  EXPECT_FALSE(UseCounter::IsCounted(*doc(), f));
  UseCounter::Count(doc(), f);
  EXPECT_TRUE(UseCounter::IsCounted(*doc(), f));
}

TEST_F(AllowedByNosniffTest, AllowedOrNot) {
  struct {
    const char* mimetype;
    bool allowed;
  } data[] = {
      // Supported mimetypes:
      {"text/javascript", true},
      {"application/javascript", true},
      {"text/ecmascript", true},

      // Blocked mimetpyes:
      {"image/png", false},
      {"text/csv", false},
      {"video/mpeg", false},

      // Legacy mimetypes:
      {"text/html", true},
      {"text/plain", true},
      {"application/xml", true},
      {"application/octet-stream", true},

      // Made-up mimetypes:
      {"text/potato", true},
      {"potato/text", true},
      {"aaa/aaa", true},
      {"zzz/zzz", true},

      // Parameterized mime types
      {"text/javascript; charset=utf-8", true},
      {"text/javascript;charset=utf-8", true},
      {"text/javascript;bla;bla", true},
      {"text/csv; charset=utf-8", false},
      {"text/csv;charset=utf-8", false},
      {"text/csv;bla;bla", false},

      // Funky capitalization:
      {"text/html", true},
      {"Text/html", true},
      {"text/Html", true},
      {"TeXt/HtMl", true},
      {"TEXT/HTML", true},
  };

  for (auto& testcase : data) {
    SCOPED_TRACE(::testing::Message()
                 << "\n  mime type: " << testcase.mimetype
                 << "\n  allowed: " << (testcase.allowed ? "true" : "false"));

    KURL url(NullURL(), "https://bla.com/");
    doc()->SetSecurityOrigin(SecurityOrigin::Create(url));
    ResourceResponse response;
    response.SetURL(url);
    response.SetHTTPHeaderField("Content-Type", testcase.mimetype);

    EXPECT_EQ(testcase.allowed,
              AllowedByNosniff::MimeTypeAsScript(doc(), response));
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
    SetUp();
    SCOPED_TRACE(::testing::Message()
                 << "\n  url: " << testcase.url << "\n  origin: "
                 << testcase.origin << "\n  mime type: " << testcase.mimetype
                 << "\n  webfeature: " << testcase.expected);
    doc()->SetSecurityOrigin(
        SecurityOrigin::Create(KURL(NullURL(), testcase.origin)));
    ResourceResponse response;
    response.SetURL(KURL(NullURL(), testcase.url));
    response.SetHTTPHeaderField("Content-Type", testcase.mimetype);

    AllowedByNosniff::MimeTypeAsScript(doc(), response);
    EXPECT_TRUE(UseCounter::IsCounted(*doc(), testcase.expected));
  }
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/CSPDirectiveList.h"

#include "core/frame/SubresourceIntegrity.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/frame/csp/SourceListDirective.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/wtf/text/StringOperators.h"
#include "platform/wtf/text/WTFString.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class CSPDirectiveListTest : public ::testing::Test {
 public:
  CSPDirectiveListTest() : csp(ContentSecurityPolicy::Create()) {}

  virtual void SetUp() {
    csp->SetupSelf(
        *SecurityOrigin::CreateFromString("https://example.test/image.png"));
  }

  CSPDirectiveList* CreateList(const String& list,
                               ContentSecurityPolicyHeaderType type) {
    Vector<UChar> characters;
    list.AppendTo(characters);
    const UChar* begin = characters.data();
    const UChar* end = begin + characters.size();

    return CSPDirectiveList::Create(csp, begin, end, type,
                                    kContentSecurityPolicyHeaderSourceHTTP);
  }

 protected:
  Persistent<ContentSecurityPolicy> csp;
};

TEST_F(CSPDirectiveListTest, Header) {
  struct TestCase {
    const char* list;
    const char* expected;
  } cases[] = {{"script-src 'self'", "script-src 'self'"},
               {"  script-src 'self'  ", "script-src 'self'"},
               {"\t\tscript-src 'self'", "script-src 'self'"},
               {"script-src 'self' \t", "script-src 'self'"}};

  for (const auto& test : cases) {
    Member<CSPDirectiveList> directive_list =
        CreateList(test.list, kContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(test.expected, directive_list->Header());
    directive_list =
        CreateList(test.list, kContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.expected, directive_list->Header());
  }
}

TEST_F(CSPDirectiveListTest, IsMatchingNoncePresent) {
  struct TestCase {
    const char* list;
    const char* nonce;
    bool expected;
  } cases[] = {
      {"script-src 'self'", "yay", false},
      {"script-src 'self'", "boo", false},
      {"script-src 'nonce-yay'", "yay", true},
      {"script-src 'nonce-yay'", "boo", false},
      {"script-src 'nonce-yay' 'nonce-boo'", "yay", true},
      {"script-src 'nonce-yay' 'nonce-boo'", "boo", true},

      // Falls back to 'default-src'
      {"default-src 'nonce-yay'", "yay", true},
      {"default-src 'nonce-yay'", "boo", false},
      {"default-src 'nonce-boo'; script-src 'nonce-yay'", "yay", true},
      {"default-src 'nonce-boo'; script-src 'nonce-yay'", "boo", false},

      // Unrelated directives do not affect result
      {"style-src 'nonce-yay'", "yay", false},
      {"style-src 'nonce-yay'", "boo", false},
  };

  for (const auto& test : cases) {
    // Report-only
    Member<CSPDirectiveList> directive_list =
        CreateList(test.list, kContentSecurityPolicyHeaderTypeReport);
    Member<SourceListDirective> script_src =
        directive_list->OperativeDirective(directive_list->script_src_.Get());
    EXPECT_EQ(test.expected,
              directive_list->IsMatchingNoncePresent(script_src, test.nonce));
    // Empty/null strings are always not present, regardless of the policy.
    EXPECT_FALSE(directive_list->IsMatchingNoncePresent(script_src, ""));
    EXPECT_FALSE(directive_list->IsMatchingNoncePresent(script_src, String()));

    // Enforce
    directive_list =
        CreateList(test.list, kContentSecurityPolicyHeaderTypeEnforce);
    script_src =
        directive_list->OperativeDirective(directive_list->script_src_.Get());
    EXPECT_EQ(test.expected,
              directive_list->IsMatchingNoncePresent(script_src, test.nonce));
    // Empty/null strings are always not present, regardless of the policy.
    EXPECT_FALSE(directive_list->IsMatchingNoncePresent(script_src, ""));
    EXPECT_FALSE(directive_list->IsMatchingNoncePresent(script_src, String()));
  }
}

TEST_F(CSPDirectiveListTest, AllowScriptFromSourceNoNonce) {
  struct TestCase {
    const char* list;
    const char* url;
    bool expected;
  } cases[] = {
      {"script-src https://example.com", "https://example.com/script.js", true},
      {"script-src https://example.com/", "https://example.com/script.js",
       true},
      {"script-src https://example.com/",
       "https://example.com/script/script.js", true},
      {"script-src https://example.com/script", "https://example.com/script.js",
       false},
      {"script-src https://example.com/script",
       "https://example.com/script/script.js", false},
      {"script-src https://example.com/script/",
       "https://example.com/script.js", false},
      {"script-src https://example.com/script/",
       "https://example.com/script/script.js", true},
      {"script-src https://example.com", "https://not.example.com/script.js",
       false},
      {"script-src https://*.example.com", "https://not.example.com/script.js",
       true},
      {"script-src https://*.example.com", "https://example.com/script.js",
       false},

      // Falls back to default-src:
      {"default-src https://example.com", "https://example.com/script.js",
       true},
      {"default-src https://example.com/", "https://example.com/script.js",
       true},
      {"default-src https://example.com/",
       "https://example.com/script/script.js", true},
      {"default-src https://example.com/script",
       "https://example.com/script.js", false},
      {"default-src https://example.com/script",
       "https://example.com/script/script.js", false},
      {"default-src https://example.com/script/",
       "https://example.com/script.js", false},
      {"default-src https://example.com/script/",
       "https://example.com/script/script.js", true},
      {"default-src https://example.com", "https://not.example.com/script.js",
       false},
      {"default-src https://*.example.com", "https://not.example.com/script.js",
       true},
      {"default-src https://*.example.com", "https://example.com/script.js",
       false},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(::testing::Message()
                 << "List: `" << test.list << "`, URL: `" << test.url << "`");
    KURL script_src = KURL(NullURL(), test.url);

    // Report-only
    Member<CSPDirectiveList> directive_list =
        CreateList(test.list, kContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(test.expected,
              directive_list->AllowScriptFromSource(
                  script_src, String(), IntegrityMetadataSet(), kParserInserted,
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting));

    // Enforce
    directive_list =
        CreateList(test.list, kContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.expected,
              directive_list->AllowScriptFromSource(
                  script_src, String(), IntegrityMetadataSet(), kParserInserted,
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting));
  }
}

TEST_F(CSPDirectiveListTest, AllowFromSourceWithNonce) {
  struct TestCase {
    const char* list;
    const char* url;
    const char* nonce;
    bool expected;
  } cases[] = {
      // Doesn't affect lists without nonces:
      {"https://example.com", "https://example.com/file", "yay", true},
      {"https://example.com", "https://example.com/file", "boo", true},
      {"https://example.com", "https://example.com/file", "", true},
      {"https://example.com", "https://not.example.com/file", "yay", false},
      {"https://example.com", "https://not.example.com/file", "boo", false},
      {"https://example.com", "https://not.example.com/file", "", false},

      // Doesn't affect URLs that match the whitelist.
      {"https://example.com 'nonce-yay'", "https://example.com/file", "yay",
       true},
      {"https://example.com 'nonce-yay'", "https://example.com/file", "boo",
       true},
      {"https://example.com 'nonce-yay'", "https://example.com/file", "", true},

      // Does affect URLs that don't.
      {"https://example.com 'nonce-yay'", "https://not.example.com/file", "yay",
       true},
      {"https://example.com 'nonce-yay'", "https://not.example.com/file", "boo",
       false},
      {"https://example.com 'nonce-yay'", "https://not.example.com/file", "",
       false},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(::testing::Message()
                 << "List: `" << test.list << "`, URL: `" << test.url << "`");
    KURL resource = KURL(NullURL(), test.url);

    // Report-only 'script-src'
    Member<CSPDirectiveList> directive_list =
        CreateList(String("script-src ") + test.list,
                   kContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(test.expected,
              directive_list->AllowScriptFromSource(
                  resource, String(test.nonce), IntegrityMetadataSet(),
                  kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting));

    // Enforce 'script-src'
    directive_list = CreateList(String("script-src ") + test.list,
                                kContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.expected,
              directive_list->AllowScriptFromSource(
                  resource, String(test.nonce), IntegrityMetadataSet(),
                  kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting));

    // Report-only 'style-src'
    directive_list = CreateList(String("style-src ") + test.list,
                                kContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(test.expected,
              directive_list->AllowStyleFromSource(
                  resource, String(test.nonce),
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting));

    // Enforce 'style-src'
    directive_list = CreateList(String("style-src ") + test.list,
                                kContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.expected,
              directive_list->AllowStyleFromSource(
                  resource, String(test.nonce),
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting));

    // Report-only 'style-src'
    directive_list = CreateList(String("default-src ") + test.list,
                                kContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(test.expected,
              directive_list->AllowScriptFromSource(
                  resource, String(test.nonce), IntegrityMetadataSet(),
                  kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting));
    EXPECT_EQ(test.expected,
              directive_list->AllowStyleFromSource(
                  resource, String(test.nonce),
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting));

    // Enforce 'style-src'
    directive_list = CreateList(String("default-src ") + test.list,
                                kContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.expected,
              directive_list->AllowScriptFromSource(
                  resource, String(test.nonce), IntegrityMetadataSet(),
                  kParserInserted, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting));
    EXPECT_EQ(test.expected,
              directive_list->AllowStyleFromSource(
                  resource, String(test.nonce),
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting));
  }
}

TEST_F(CSPDirectiveListTest, AllowScriptFromSourceWithHash) {
  struct TestCase {
    const char* list;
    const char* url;
    const char* integrity;
    bool expected;
  } cases[] = {
      // Doesn't affect lists without hashes.
      {"https://example.com", "https://example.com/file", "sha256-yay", true},
      {"https://example.com", "https://example.com/file", "sha256-boo", true},
      {"https://example.com", "https://example.com/file", "", true},
      {"https://example.com", "https://not.example.com/file", "sha256-yay",
       false},
      {"https://example.com", "https://not.example.com/file", "sha256-boo",
       false},
      {"https://example.com", "https://not.example.com/file", "", false},

      // Doesn't affect URLs that match the whitelist.
      {"https://example.com 'sha256-yay'", "https://example.com/file",
       "sha256-yay", true},
      {"https://example.com 'sha256-yay'", "https://example.com/file",
       "sha256-boo", true},
      {"https://example.com 'sha256-yay'", "https://example.com/file", "",
       true},

      // Does affect URLs that don't match the whitelist.
      {"https://example.com 'sha256-yay'", "https://not.example.com/file",
       "sha256-yay", true},
      {"https://example.com 'sha256-yay'", "https://not.example.com/file",
       "sha256-boo", false},
      {"https://example.com 'sha256-yay'", "https://not.example.com/file", "",
       false},

      // Both algorithm and digest must match.
      {"'sha256-yay'", "https://a.com/file", "sha384-yay", false},

      // Sha-1 is not supported, but -384 and -512 are.
      {"'sha1-yay'", "https://a.com/file", "sha1-yay", false},
      {"'sha384-yay'", "https://a.com/file", "sha384-yay", true},
      {"'sha512-yay'", "https://a.com/file", "sha512-yay", true},

      // Unknown (or future) hash algorithms don't work.
      {"'asdf256-yay'", "https://a.com/file", "asdf256-yay", false},

      // But they also don't interfere.
      {"'sha256-yay'", "https://a.com/file", "sha256-yay asdf256-boo", true},

      // Additional whitelisted hashes in the CSP don't interfere.
      {"'sha256-yay' 'sha384-boo'", "https://a.com/file", "sha256-yay", true},
      {"'sha256-yay' 'sha384-boo'", "https://a.com/file", "sha384-boo", true},

      // All integrity hashes must appear in the CSP (and match).
      {"'sha256-yay'", "https://a.com/file", "sha256-yay sha384-boo", false},
      {"'sha384-boo'", "https://a.com/file", "sha256-yay sha384-boo", false},
      {"'sha256-yay' 'sha384-boo'", "https://a.com/file",
       "sha256-yay sha384-yay", false},
      {"'sha256-yay' 'sha384-boo'", "https://a.com/file",
       "sha256-boo sha384-boo", false},
      {"'sha256-yay' 'sha384-boo'", "https://a.com/file",
       "sha256-yay sha384-boo", true},

      // At least one integrity hash must be present.
      {"'sha256-yay'", "https://a.com/file", "", false},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(::testing::Message()
                 << "List: `" << test.list << "`, URL: `" << test.url
                 << "`, Integrity: `" << test.integrity << "`");
    KURL resource = KURL(NullURL(), test.url);

    IntegrityMetadataSet integrity_metadata;
    ASSERT_EQ(SubresourceIntegrity::kIntegrityParseValidResult,
              SubresourceIntegrity::ParseIntegrityAttribute(
                  test.integrity, integrity_metadata));

    // Report-only 'script-src'
    Member<CSPDirectiveList> directive_list =
        CreateList(String("script-src ") + test.list,
                   kContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(test.expected,
              directive_list->AllowScriptFromSource(
                  resource, String(), integrity_metadata, kParserInserted,
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting));

    // Enforce 'script-src'
    directive_list = CreateList(String("script-src ") + test.list,
                                kContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.expected,
              directive_list->AllowScriptFromSource(
                  resource, String(), integrity_metadata, kParserInserted,
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting));
  }
}

TEST_F(CSPDirectiveListTest, allowRequestWithoutIntegrity) {
  struct TestCase {
    const char* list;
    const char* url;
    const WebURLRequest::RequestContext context;
    bool expected;
  } cases[] = {

      {"require-sri-for script", "https://example.com/file",
       WebURLRequest::kRequestContextScript, false},

      // Extra WSP
      {"require-sri-for  script     script  ", "https://example.com/file",
       WebURLRequest::kRequestContextScript, false},
      {"require-sri-for      style    script", "https://example.com/file",
       WebURLRequest::kRequestContextStyle, false},

      {"require-sri-for style script", "https://example.com/file",
       WebURLRequest::kRequestContextScript, false},
      {"require-sri-for style script", "https://example.com/file",
       WebURLRequest::kRequestContextImport, false},
      {"require-sri-for style script", "https://example.com/file",
       WebURLRequest::kRequestContextImage, true},

      {"require-sri-for script", "https://example.com/file",
       WebURLRequest::kRequestContextAudio, true},
      {"require-sri-for script", "https://example.com/file",
       WebURLRequest::kRequestContextScript, false},
      {"require-sri-for script", "https://example.com/file",
       WebURLRequest::kRequestContextImport, false},
      {"require-sri-for script", "https://example.com/file",
       WebURLRequest::kRequestContextServiceWorker, false},
      {"require-sri-for script", "https://example.com/file",
       WebURLRequest::kRequestContextSharedWorker, false},
      {"require-sri-for script", "https://example.com/file",
       WebURLRequest::kRequestContextWorker, false},
      {"require-sri-for script", "https://example.com/file",
       WebURLRequest::kRequestContextStyle, true},

      {"require-sri-for style", "https://example.com/file",
       WebURLRequest::kRequestContextAudio, true},
      {"require-sri-for style", "https://example.com/file",
       WebURLRequest::kRequestContextScript, true},
      {"require-sri-for style", "https://example.com/file",
       WebURLRequest::kRequestContextImport, true},
      {"require-sri-for style", "https://example.com/file",
       WebURLRequest::kRequestContextServiceWorker, true},
      {"require-sri-for style", "https://example.com/file",
       WebURLRequest::kRequestContextSharedWorker, true},
      {"require-sri-for style", "https://example.com/file",
       WebURLRequest::kRequestContextWorker, true},
      {"require-sri-for style", "https://example.com/file",
       WebURLRequest::kRequestContextStyle, false},

      // Multiple tokens
      {"require-sri-for script style", "https://example.com/file",
       WebURLRequest::kRequestContextStyle, false},
      {"require-sri-for script style", "https://example.com/file",
       WebURLRequest::kRequestContextScript, false},
      {"require-sri-for script style", "https://example.com/file",
       WebURLRequest::kRequestContextImport, false},
      {"require-sri-for script style", "https://example.com/file",
       WebURLRequest::kRequestContextImage, true},

      // Matching is case-insensitive
      {"require-sri-for Script", "https://example.com/file",
       WebURLRequest::kRequestContextScript, false},

      // Unknown tokens do not affect result
      {"require-sri-for blabla12 as", "https://example.com/file",
       WebURLRequest::kRequestContextScript, true},
      {"require-sri-for blabla12 as  script", "https://example.com/file",
       WebURLRequest::kRequestContextScript, false},
      {"require-sri-for script style img", "https://example.com/file",
       WebURLRequest::kRequestContextScript, false},
      {"require-sri-for script style img", "https://example.com/file",
       WebURLRequest::kRequestContextImport, false},
      {"require-sri-for script style img", "https://example.com/file",
       WebURLRequest::kRequestContextStyle, false},
      {"require-sri-for script style img", "https://example.com/file",
       WebURLRequest::kRequestContextImage, true},

      // Empty token list has no effect
      {"require-sri-for      ", "https://example.com/file",
       WebURLRequest::kRequestContextScript, true},
      {"require-sri-for      ", "https://example.com/file",
       WebURLRequest::kRequestContextImport, true},
      {"require-sri-for      ", "https://example.com/file",
       WebURLRequest::kRequestContextStyle, true},
      {"require-sri-for      ", "https://example.com/file",
       WebURLRequest::kRequestContextServiceWorker, true},
      {"require-sri-for      ", "https://example.com/file",
       WebURLRequest::kRequestContextSharedWorker, true},
      {"require-sri-for      ", "https://example.com/file",
       WebURLRequest::kRequestContextWorker, true},

      // Order does not matter
      {"require-sri-for a b script", "https://example.com/file",
       WebURLRequest::kRequestContextScript, false},
      {"require-sri-for a script b", "https://example.com/file",
       WebURLRequest::kRequestContextScript, false},
  };

  for (const auto& test : cases) {
    KURL resource = KURL(NullURL(), test.url);
    // Report-only
    Member<CSPDirectiveList> directive_list =
        CreateList(test.list, kContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(true, directive_list->AllowRequestWithoutIntegrity(
                        test.context, resource,
                        ResourceRequest::RedirectStatus::kNoRedirect,
                        SecurityViolationReportingPolicy::kSuppressReporting));

    // Enforce
    directive_list =
        CreateList(test.list, kContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.expected,
              directive_list->AllowRequestWithoutIntegrity(
                  test.context, resource,
                  ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting));
  }
}

TEST_F(CSPDirectiveListTest, WorkerSrc) {
  struct TestCase {
    const char* list;
    bool allowed;
  } cases[] = {
      {"worker-src 'none'", false},
      {"worker-src http://not.example.test", false},
      {"worker-src https://example.test", true},
      {"default-src *; worker-src 'none'", false},
      {"default-src *; worker-src http://not.example.test", false},
      {"default-src *; worker-src https://example.test", true},
      {"script-src *; worker-src 'none'", false},
      {"script-src *; worker-src http://not.example.test", false},
      {"script-src *; worker-src https://example.test", true},
      {"default-src *; script-src *; worker-src 'none'", false},
      {"default-src *; script-src *; worker-src http://not.example.test",
       false},
      {"default-src *; script-src *; worker-src https://example.test", true},

      // Fallback to script-src.
      {"script-src 'none'", false},
      {"script-src http://not.example.test", false},
      {"script-src https://example.test", true},
      {"default-src *; script-src 'none'", false},
      {"default-src *; script-src http://not.example.test", false},
      {"default-src *; script-src https://example.test", true},

      // Fallback to default-src.
      {"default-src 'none'", false},
      {"default-src http://not.example.test", false},
      {"default-src https://example.test", true},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.list);
    KURL resource = KURL(NullURL(), "https://example.test/worker.js");
    Member<CSPDirectiveList> directive_list =
        CreateList(test.list, kContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.allowed,
              directive_list->AllowWorkerFromSource(
                  resource, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting));
  }
}

TEST_F(CSPDirectiveListTest, WorkerSrcChildSrcFallback) {
  // TODO(mkwst): Remove this test once we remove the temporary fallback
  // behavior. https://crbug.com/662930
  struct TestCase {
    const char* list;
    bool allowed;
  } cases[] = {
      // When 'worker-src' is not present, 'child-src' can allow a worker when
      // present.
      {"child-src https://example.test", true},
      {"child-src https://not-example.test", true},
      {"script-src https://example.test", true},
      {"script-src https://not-example.test", false},
      {"child-src https://example.test; script-src https://example.test", true},
      {"child-src https://example.test; script-src https://not-example.test",
       true},
      {"child-src https://not-example.test; script-src https://example.test",
       true},
      {"child-src https://not-example.test; script-src "
       "https://not-example.test",
       false},

      // If 'worker-src' is present, 'child-src' will not allow a worker.
      {"worker-src https://example.test; child-src https://example.test", true},
      {"worker-src https://example.test; child-src https://not-example.test",
       true},
      {"worker-src https://not-example.test; child-src https://example.test",
       false},
      {"worker-src https://not-example.test; child-src "
       "https://not-example.test",
       false},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.list);
    KURL resource = KURL(NullURL(), "https://example.test/worker.js");
    Member<CSPDirectiveList> directive_list =
        CreateList(test.list, kContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.allowed,
              directive_list->AllowWorkerFromSource(
                  resource, ResourceRequest::RedirectStatus::kNoRedirect,
                  SecurityViolationReportingPolicy::kSuppressReporting));
  }
}

TEST_F(CSPDirectiveListTest, SubsumesBasedOnCSPSourcesOnly) {
  CSPDirectiveList* a = CreateList(
      "script-src http://*.one.com; img-src https://one.com "
      "http://two.com/imgs/",
      kContentSecurityPolicyHeaderTypeEnforce);

  struct TestCase {
    const std::vector<const char*> policies;
    bool expected;
    bool expected_first_policy_opposite;
  } cases[] = {
      // `listB`, which is not as restrictive as `A`, is not subsumed.
      {{""}, false, true},
      {{"script-src http://example.com"}, false, false},
      {{"img-src http://example.com"}, false, false},
      {{"script-src http://*.one.com"}, false, true},
      {{"img-src https://one.com http://two.com/imgs/"}, false, true},
      {{"default-src http://example.com"}, false, false},
      {{"default-src https://one.com http://two.com/imgs/"}, false, false},
      {{"default-src http://one.com"}, false, false},
      {{"script-src http://*.one.com; img-src http://two.com/"}, false, false},
      {{"script-src http://*.one.com", "img-src http://one.com"}, false, true},
      {{"script-src http://*.one.com", "script-src https://two.com"},
       false,
       true},
      {{"script-src http://*.random.com", "script-src https://random.com"},
       false,
       false},
      {{"script-src http://one.com", "script-src https://random.com"},
       false,
       false},
      {{"script-src http://*.random.com; default-src http://one.com "
        "http://two.com/imgs/",
        "default-src https://random.com"},
       false,
       false},
      // `listB`, which is as restrictive as `A`, is subsumed.
      {{"default-src https://one.com"}, true, false},
      {{"default-src http://random.com",
        "default-src https://non-random.com:*"},
       true,
       false},
      {{"script-src http://*.one.com; img-src https://one.com"}, true, false},
      {{"script-src http://*.one.com; img-src https://one.com "
        "http://two.com/imgs/"},
       true,
       true},
      {{"script-src http://*.one.com",
        "img-src https://one.com http://two.com/imgs/"},
       true,
       true},
      {{"script-src http://*.random.com; default-src https://one.com "
        "http://two.com/imgs/",
        "default-src https://else.com"},
       true,
       false},
      {{"script-src http://*.random.com; default-src https://one.com "
        "http://two.com/imgs/",
        "default-src https://one.com"},
       true,
       false},
  };

  CSPDirectiveList* empty_a =
      CreateList("", kContentSecurityPolicyHeaderTypeEnforce);

  for (const auto& test : cases) {
    HeapVector<Member<CSPDirectiveList>> list_b;
    for (const auto& policy : test.policies) {
      list_b.push_back(
          CreateList(policy, kContentSecurityPolicyHeaderTypeEnforce));
    }

    EXPECT_EQ(test.expected, a->Subsumes(list_b));
    // Empty CSPDirective subsumes any list.
    EXPECT_TRUE(empty_a->Subsumes(list_b));
    // Check if first policy of `listB` subsumes `A`.
    EXPECT_EQ(test.expected_first_policy_opposite,
              list_b[0]->Subsumes(HeapVector<Member<CSPDirectiveList>>(1, a)));
  }
}

TEST_F(CSPDirectiveListTest, SubsumesIfNoneIsPresent) {
  struct TestCase {
    const char* policy_a;
    const std::vector<const char*> policies_b;
    bool expected;
  } cases[] = {
      // `policyA` subsumes any vector of policies.
      {"", {""}, true},
      {"", {"script-src http://example.com"}, true},
      {"", {"script-src 'none'"}, true},
      {"", {"script-src http://*.one.com", "script-src https://two.com"}, true},
      // `policyA` is 'none', but no policy in `policiesB` is.
      {"script-src ", {""}, false},
      {"script-src 'none'", {""}, false},
      {"script-src ", {"script-src http://example.com"}, false},
      {"script-src 'none'", {"script-src http://example.com"}, false},
      {"script-src ", {"img-src 'none'"}, false},
      {"script-src 'none'", {"img-src 'none'"}, false},
      {"script-src ",
       {"script-src http://*.one.com", "img-src https://two.com"},
       false},
      {"script-src 'none'",
       {"script-src http://*.one.com", "img-src https://two.com"},
       false},
      {"script-src 'none'",
       {"script-src http://*.one.com", "script-src https://two.com"},
       true},
      {"script-src 'none'",
       {"script-src http://*.one.com", "script-src 'self'"},
       true},
      // `policyA` is not 'none', but at least effective result of `policiesB`
      // is.
      {"script-src http://example.com 'none'", {"script-src 'none'"}, true},
      {"script-src http://example.com", {"script-src 'none'"}, true},
      {"script-src http://example.com 'none'",
       {"script-src http://*.one.com", "script-src http://one.com",
        "script-src 'none'"},
       true},
      {"script-src http://example.com",
       {"script-src http://*.one.com", "script-src http://one.com",
        "script-src 'none'"},
       true},
      {"script-src http://one.com 'none'",
       {"script-src http://*.one.com", "script-src http://one.com",
        "script-src https://one.com"},
       true},
      // `policyA` is `none` and at least effective result of `policiesB` is
      // too.
      {"script-src ", {"script-src ", "script-src "}, true},
      {"script-src 'none'", {"script-src", "script-src 'none'"}, true},
      {"script-src ", {"script-src 'none'", "script-src 'none'"}, true},
      {"script-src ",
       {"script-src 'none' http://example.com",
        "script-src 'none' http://example.com"},
       false},
      {"script-src 'none'", {"script-src 'none'", "script-src 'none'"}, true},
      {"script-src 'none'",
       {"script-src 'none'", "script-src 'none'", "script-src 'none'"},
       true},
      {"script-src 'none'",
       {"script-src http://*.one.com", "script-src http://one.com",
        "script-src 'none'"},
       true},
      {"script-src 'none'",
       {"script-src http://*.one.com", "script-src http://two.com",
        "script-src http://three.com"},
       true},
      // Policies contain special keywords.
      {"script-src ", {"script-src ", "script-src 'unsafe-eval'"}, true},
      {"script-src 'none'",
       {"script-src 'unsafe-inline'", "script-src 'none'"},
       true},
      {"script-src ",
       {"script-src 'none' 'unsafe-inline'",
        "script-src 'none' 'unsafe-inline'"},
       false},
      {"script-src ",
       {"script-src 'none' 'unsafe-inline'",
        "script-src 'unsafe-inline' 'strict-dynamic'"},
       false},
      {"script-src 'unsafe-eval'",
       {"script-src 'unsafe-eval'", "script 'unsafe-inline'"},
       true},
      {"script-src 'unsafe-inline'",
       {"script-src  ", "script http://example.com"},
       true},
  };

  for (const auto& test : cases) {
    CSPDirectiveList* a =
        CreateList(test.policy_a, kContentSecurityPolicyHeaderTypeEnforce);

    HeapVector<Member<CSPDirectiveList>> list_b;
    for (const auto& policy_b : test.policies_b)
      list_b.push_back(
          CreateList(policy_b, kContentSecurityPolicyHeaderTypeEnforce));

    EXPECT_EQ(test.expected, a->Subsumes(list_b));
  }
}

TEST_F(CSPDirectiveListTest, SubsumesPluginTypes) {
  struct TestCase {
    const char* policy_a;
    const std::vector<const char*> policies_b;
    bool expected;
  } cases[] = {
      // `policyA` subsumes `policiesB`.
      {"script-src 'unsafe-inline'",
       {"script-src  ", "script-src http://example.com",
        "plugin-types text/plain"},
       true},
      {"script-src http://example.com",
       {"script-src http://example.com; plugin-types "},
       true},
      {"script-src http://example.com",
       {"script-src http://example.com; plugin-types text/plain"},
       true},
      {"script-src http://example.com; plugin-types text/plain",
       {"script-src http://example.com; plugin-types text/plain"},
       true},
      {"script-src http://example.com; plugin-types text/plain",
       {"script-src http://example.com; plugin-types "},
       true},
      {"script-src http://example.com; plugin-types text/plain",
       {"script-src http://example.com; plugin-types ", "plugin-types "},
       true},
      {"plugin-types application/pdf text/plain",
       {"plugin-types application/pdf text/plain",
        "plugin-types application/x-blink-test-plugin"},
       true},
      {"plugin-types application/pdf text/plain",
       {"plugin-types application/pdf text/plain",
        "plugin-types application/pdf text/plain "
        "application/x-blink-test-plugin"},
       true},
      {"plugin-types application/x-shockwave-flash application/pdf text/plain",
       {"plugin-types application/x-shockwave-flash application/pdf text/plain",
        "plugin-types application/x-shockwave-flash"},
       true},
      {"plugin-types application/x-shockwave-flash",
       {"plugin-types application/x-shockwave-flash application/pdf text/plain",
        "plugin-types application/x-shockwave-flash"},
       true},
      // `policyA` does not subsume `policiesB`.
      {"script-src http://example.com; plugin-types text/plain",
       {"script-src http://example.com"},
       false},
      {"plugin-types random-value",
       {"script-src 'unsafe-inline'", "plugin-types text/plain"},
       false},
      {"plugin-types random-value",
       {"script-src http://example.com", "script-src http://example.com"},
       false},
      {"plugin-types random-value",
       {"plugin-types  text/plain", "plugin-types text/plain"},
       false},
      {"script-src http://example.com; plugin-types text/plain",
       {"plugin-types ", "plugin-types "},
       false},
      {"plugin-types application/pdf text/plain",
       {"plugin-types application/x-blink-test-plugin",
        "plugin-types application/x-blink-test-plugin"},
       false},
      {"plugin-types application/pdf text/plain",
       {"plugin-types application/pdf application/x-blink-test-plugin",
        "plugin-types application/x-blink-test-plugin"},
       false},
  };

  for (const auto& test : cases) {
    CSPDirectiveList* a =
        CreateList(test.policy_a, kContentSecurityPolicyHeaderTypeEnforce);

    HeapVector<Member<CSPDirectiveList>> list_b;
    for (const auto& policy_b : test.policies_b)
      list_b.push_back(
          CreateList(policy_b, kContentSecurityPolicyHeaderTypeEnforce));

    EXPECT_EQ(test.expected, a->Subsumes(list_b));
  }
}

TEST_F(CSPDirectiveListTest, OperativeDirectiveGivenType) {
  enum DefaultBehaviour {
    kDefault,
    kNoDefault,
    kChildAndDefault,
    kScriptAndDefault
  };

  struct TestCase {
    ContentSecurityPolicy::DirectiveType directive;
    const DefaultBehaviour type;
  } cases[] = {
      // Directives with default directive.
      {ContentSecurityPolicy::DirectiveType::kChildSrc, kDefault},
      {ContentSecurityPolicy::DirectiveType::kConnectSrc, kDefault},
      {ContentSecurityPolicy::DirectiveType::kFontSrc, kDefault},
      {ContentSecurityPolicy::DirectiveType::kImgSrc, kDefault},
      {ContentSecurityPolicy::DirectiveType::kManifestSrc, kDefault},
      {ContentSecurityPolicy::DirectiveType::kMediaSrc, kDefault},
      {ContentSecurityPolicy::DirectiveType::kObjectSrc, kDefault},
      {ContentSecurityPolicy::DirectiveType::kScriptSrc, kDefault},
      {ContentSecurityPolicy::DirectiveType::kStyleSrc, kDefault},
      // Directives with no default directive.
      {ContentSecurityPolicy::DirectiveType::kBaseURI, kNoDefault},
      {ContentSecurityPolicy::DirectiveType::kDefaultSrc, kNoDefault},
      {ContentSecurityPolicy::DirectiveType::kFrameAncestors, kNoDefault},
      {ContentSecurityPolicy::DirectiveType::kFormAction, kNoDefault},
      // Directive with multiple default directives.
      {ContentSecurityPolicy::DirectiveType::kFrameSrc, kChildAndDefault},
      {ContentSecurityPolicy::DirectiveType::kWorkerSrc, kScriptAndDefault},
  };

  // Initial set-up.
  std::stringstream all_directives;
  for (const auto& test : cases) {
    const char* name = ContentSecurityPolicy::GetDirectiveName(test.directive);
    all_directives << name << " http://" << name << ".com; ";
  }
  CSPDirectiveList* all_directives_list = CreateList(
      all_directives.str().c_str(), kContentSecurityPolicyHeaderTypeEnforce);
  CSPDirectiveList* empty =
      CreateList("", kContentSecurityPolicyHeaderTypeEnforce);

  for (const auto& test : cases) {
    const char* name = ContentSecurityPolicy::GetDirectiveName(test.directive);
    // When CSPDirectiveList is empty, then `null` should be returned for any
    // type.
    EXPECT_FALSE(empty->OperativeDirective(test.directive));

    // When all directives present, then given a type that directive value
    // should be returned.
    HeapVector<Member<CSPSource>> sources =
        all_directives_list->OperativeDirective(test.directive)->list_;
    EXPECT_EQ(sources.size(), 1u);
    EXPECT_TRUE(sources[0]->host_.StartsWith(name));

    std::stringstream all_except_this;
    std::stringstream all_except_child_src_and_this;
    std::stringstream all_except_script_src_and_this;
    for (const auto& subtest : cases) {
      if (subtest.directive == test.directive)
        continue;
      const char* directive_name =
          ContentSecurityPolicy::GetDirectiveName(subtest.directive);
      all_except_this << directive_name << " http://" << directive_name
                      << ".com; ";
      if (subtest.directive !=
          ContentSecurityPolicy::DirectiveType::kChildSrc) {
        all_except_child_src_and_this << directive_name << " http://"
                                      << directive_name << ".com; ";
      }
      if (subtest.directive !=
          ContentSecurityPolicy::DirectiveType::kScriptSrc) {
        all_except_script_src_and_this << directive_name << " http://"
                                       << directive_name << ".com; ";
      }
    }
    CSPDirectiveList* all_except_this_list = CreateList(
        all_except_this.str().c_str(), kContentSecurityPolicyHeaderTypeEnforce);
    CSPDirectiveList* all_except_child_src_and_this_list =
        CreateList(all_except_child_src_and_this.str().c_str(),
                   kContentSecurityPolicyHeaderTypeEnforce);
    CSPDirectiveList* all_except_script_src_and_this_list =
        CreateList(all_except_script_src_and_this.str().c_str(),
                   kContentSecurityPolicyHeaderTypeEnforce);

    switch (test.type) {
      case kDefault:
        sources =
            all_except_this_list->OperativeDirective(test.directive)->list_;
        EXPECT_EQ(sources.size(), 1u);
        EXPECT_EQ(sources[0]->host_, "default-src.com");
        break;
      case kNoDefault:
        EXPECT_FALSE(all_except_this_list->OperativeDirective(test.directive));
        break;
      case kChildAndDefault:
        sources =
            all_except_this_list->OperativeDirective(test.directive)->list_;
        EXPECT_EQ(sources.size(), 1u);
        EXPECT_EQ(sources[0]->host_, "child-src.com");
        sources = all_except_child_src_and_this_list
                      ->OperativeDirective(test.directive)
                      ->list_;
        EXPECT_EQ(sources.size(), 1u);
        EXPECT_EQ(sources[0]->host_, "default-src.com");
        break;
      case kScriptAndDefault:
        sources =
            all_except_this_list->OperativeDirective(test.directive)->list_;
        EXPECT_EQ(sources.size(), 1u);
        EXPECT_EQ(sources[0]->host_, "script-src.com");
        sources = all_except_script_src_and_this_list
                      ->OperativeDirective(test.directive)
                      ->list_;
        EXPECT_EQ(sources.size(), 1u);
        EXPECT_EQ(sources[0]->host_, "default-src.com");
        break;
    }
  }
}

TEST_F(CSPDirectiveListTest, GetSourceVector) {
  const std::vector<const char*> policies = {
      // Policy 1
      "default-src https://default-src.com",
      // Policy 2
      "child-src http://child-src.com",
      // Policy 3
      "child-src http://child-src.com; default-src https://default-src.com",
      // Policy 4
      "base-uri http://base-uri.com",
      // Policy 5
      "frame-src http://frame-src.com"};

  // Check expectations on the initial set-up.
  HeapVector<Member<CSPDirectiveList>> policy_vector;
  for (const auto& policy : policies) {
    policy_vector.push_back(
        CreateList(policy, kContentSecurityPolicyHeaderTypeEnforce));
  }
  HeapVector<Member<SourceListDirective>> result =
      CSPDirectiveList::GetSourceVector(
          ContentSecurityPolicy::DirectiveType::kDefaultSrc, policy_vector);
  EXPECT_EQ(result.size(), 2u);
  result = CSPDirectiveList::GetSourceVector(
      ContentSecurityPolicy::DirectiveType::kChildSrc, policy_vector);
  EXPECT_EQ(result.size(), 3u);
  result = CSPDirectiveList::GetSourceVector(
      ContentSecurityPolicy::DirectiveType::kBaseURI, policy_vector);
  EXPECT_EQ(result.size(), 1u);
  result = CSPDirectiveList::GetSourceVector(
      ContentSecurityPolicy::DirectiveType::kFrameSrc, policy_vector);
  EXPECT_EQ(result.size(), 4u);

  enum DefaultBehaviour { kDefault, kNoDefault, kChildAndDefault };

  struct TestCase {
    ContentSecurityPolicy::DirectiveType directive;
    const DefaultBehaviour type;
    size_t expected_total;
    int expected_current;
    int expected_default_src;
    int expected_child_src;
  } cases[] = {
      // Directives with default directive.
      {ContentSecurityPolicy::DirectiveType::kChildSrc, kDefault, 4u, 3, 1, 3},
      {ContentSecurityPolicy::DirectiveType::kConnectSrc, kDefault, 3u, 1, 2,
       0},
      {ContentSecurityPolicy::DirectiveType::kFontSrc, kDefault, 3u, 1, 2, 0},
      {ContentSecurityPolicy::DirectiveType::kImgSrc, kDefault, 3u, 1, 2, 0},
      {ContentSecurityPolicy::DirectiveType::kManifestSrc, kDefault, 3u, 1, 2,
       0},
      {ContentSecurityPolicy::DirectiveType::kMediaSrc, kDefault, 3u, 1, 2, 0},
      {ContentSecurityPolicy::DirectiveType::kObjectSrc, kDefault, 3u, 1, 2, 0},
      {ContentSecurityPolicy::DirectiveType::kScriptSrc, kDefault, 3u, 1, 2, 0},
      {ContentSecurityPolicy::DirectiveType::kStyleSrc, kDefault, 3u, 1, 2, 0},
      // Directives with no default directive.
      {ContentSecurityPolicy::DirectiveType::kBaseURI, kNoDefault, 2u, 2, 0, 0},
      {ContentSecurityPolicy::DirectiveType::kFrameAncestors, kNoDefault, 1u, 1,
       0, 0},
      {ContentSecurityPolicy::DirectiveType::kFormAction, kNoDefault, 1u, 1, 0,
       0},
      // Directive with multiple default directives.
      {ContentSecurityPolicy::DirectiveType::kFrameSrc, kChildAndDefault, 5u, 2,
       1, 2},
  };

  for (const auto& test : cases) {
    // Initial set-up.
    HeapVector<Member<CSPDirectiveList>> policy_vector;
    for (const auto& policy : policies) {
      policy_vector.push_back(
          CreateList(policy, kContentSecurityPolicyHeaderTypeEnforce));
    }
    // Append current test's policy.
    std::stringstream current_directive;
    const char* name = ContentSecurityPolicy::GetDirectiveName(test.directive);
    current_directive << name << " http://" << name << ".com;";
    policy_vector.push_back(
        CreateList(current_directive.str().c_str(),
                   kContentSecurityPolicyHeaderTypeEnforce));

    HeapVector<Member<SourceListDirective>> result =
        CSPDirectiveList::GetSourceVector(test.directive, policy_vector);

    EXPECT_EQ(result.size(), test.expected_total);

    int actual_current = 0, actual_default = 0, actual_child = 0;
    for (const auto& src_list : result) {
      HeapVector<Member<CSPSource>> sources = src_list->list_;
      for (const auto& source : sources) {
        if (source->host_.StartsWith(name))
          actual_current += 1;
        else if (source->host_ == "default-src.com")
          actual_default += 1;

        if (source->host_ == "child-src.com")
          actual_child += 1;
      }
    }

    EXPECT_EQ(actual_default, test.expected_default_src);
    EXPECT_EQ(actual_current, test.expected_current);
    EXPECT_EQ(actual_child, test.expected_child_src);

    // If another default-src is added that should only impact Fetch Directives
    policy_vector.push_back(
        CreateList("default-src https://default-src.com;",
                   kContentSecurityPolicyHeaderTypeEnforce));
    size_t udpated_total =
        test.type != kNoDefault ? test.expected_total + 1 : test.expected_total;
    EXPECT_EQ(
        CSPDirectiveList::GetSourceVector(test.directive, policy_vector).size(),
        udpated_total);
    size_t expected_child_src =
        test.directive == ContentSecurityPolicy::DirectiveType::kChildSrc ? 5u
                                                                          : 4u;
    EXPECT_EQ(
        CSPDirectiveList::GetSourceVector(
            ContentSecurityPolicy::DirectiveType::kChildSrc, policy_vector)
            .size(),
        expected_child_src);

    // If another child-src is added that should only impact frame-src and
    // child-src
    policy_vector.push_back(
        CreateList("child-src http://child-src.com;",
                   kContentSecurityPolicyHeaderTypeEnforce));
    udpated_total = test.type == kChildAndDefault ||
                            test.directive ==
                                ContentSecurityPolicy::DirectiveType::kChildSrc
                        ? udpated_total + 1
                        : udpated_total;
    EXPECT_EQ(
        CSPDirectiveList::GetSourceVector(test.directive, policy_vector).size(),
        udpated_total);
    expected_child_src = expected_child_src + 1u;
    EXPECT_EQ(
        CSPDirectiveList::GetSourceVector(
            ContentSecurityPolicy::DirectiveType::kChildSrc, policy_vector)
            .size(),
        expected_child_src);

    // If we add sandbox, nothing should change since it is currenly not
    // considered.
    policy_vector.push_back(
        CreateList("sandbox http://sandbox.com;",
                   kContentSecurityPolicyHeaderTypeEnforce));
    EXPECT_EQ(
        CSPDirectiveList::GetSourceVector(test.directive, policy_vector).size(),
        udpated_total);
    EXPECT_EQ(
        CSPDirectiveList::GetSourceVector(
            ContentSecurityPolicy::DirectiveType::kChildSrc, policy_vector)
            .size(),
        expected_child_src);
  }
}

}  // namespace blink

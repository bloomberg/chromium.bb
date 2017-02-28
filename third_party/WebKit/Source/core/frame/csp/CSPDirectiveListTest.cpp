// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/CSPDirectiveList.h"

#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/frame/csp/SourceListDirective.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/ResourceRequest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/StringOperators.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSPDirectiveListTest : public ::testing::Test {
 public:
  CSPDirectiveListTest() : csp(ContentSecurityPolicy::create()) {}

  virtual void SetUp() {
    csp->setupSelf(
        *SecurityOrigin::createFromString("https://example.test/image.png"));
  }

  CSPDirectiveList* createList(const String& list,
                               ContentSecurityPolicyHeaderType type) {
    Vector<UChar> characters;
    list.appendTo(characters);
    const UChar* begin = characters.data();
    const UChar* end = begin + characters.size();

    return CSPDirectiveList::create(csp, begin, end, type,
                                    ContentSecurityPolicyHeaderSourceHTTP);
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
    Member<CSPDirectiveList> directiveList =
        createList(test.list, ContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(test.expected, directiveList->header());
    directiveList =
        createList(test.list, ContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.expected, directiveList->header());
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
    Member<CSPDirectiveList> directiveList =
        createList(test.list, ContentSecurityPolicyHeaderTypeReport);
    Member<SourceListDirective> scriptSrc =
        directiveList->operativeDirective(directiveList->m_scriptSrc.get());
    EXPECT_EQ(test.expected,
              directiveList->isMatchingNoncePresent(scriptSrc, test.nonce));
    // Empty/null strings are always not present, regardless of the policy.
    EXPECT_FALSE(directiveList->isMatchingNoncePresent(scriptSrc, ""));
    EXPECT_FALSE(directiveList->isMatchingNoncePresent(scriptSrc, String()));

    // Enforce
    directiveList =
        createList(test.list, ContentSecurityPolicyHeaderTypeEnforce);
    scriptSrc =
        directiveList->operativeDirective(directiveList->m_scriptSrc.get());
    EXPECT_EQ(test.expected,
              directiveList->isMatchingNoncePresent(scriptSrc, test.nonce));
    // Empty/null strings are always not present, regardless of the policy.
    EXPECT_FALSE(directiveList->isMatchingNoncePresent(scriptSrc, ""));
    EXPECT_FALSE(directiveList->isMatchingNoncePresent(scriptSrc, String()));
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
    SCOPED_TRACE(testing::Message() << "List: `" << test.list << "`, URL: `"
                                    << test.url << "`");
    KURL scriptSrc = KURL(KURL(), test.url);

    // Report-only
    Member<CSPDirectiveList> directiveList =
        createList(test.list, ContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(test.expected,
              directiveList->allowScriptFromSource(
                  scriptSrc, String(), ParserInserted,
                  ResourceRequest::RedirectStatus::NoRedirect,
                  SecurityViolationReportingPolicy::SuppressReporting));

    // Enforce
    directiveList =
        createList(test.list, ContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.expected,
              directiveList->allowScriptFromSource(
                  scriptSrc, String(), ParserInserted,
                  ResourceRequest::RedirectStatus::NoRedirect,
                  SecurityViolationReportingPolicy::SuppressReporting));
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
    SCOPED_TRACE(testing::Message() << "List: `" << test.list << "`, URL: `"
                                    << test.url << "`");
    KURL resource = KURL(KURL(), test.url);

    // Report-only 'script-src'
    Member<CSPDirectiveList> directiveList =
        createList(String("script-src ") + test.list,
                   ContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(test.expected,
              directiveList->allowScriptFromSource(
                  resource, String(test.nonce), ParserInserted,
                  ResourceRequest::RedirectStatus::NoRedirect,
                  SecurityViolationReportingPolicy::SuppressReporting));

    // Enforce 'script-src'
    directiveList = createList(String("script-src ") + test.list,
                               ContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.expected,
              directiveList->allowScriptFromSource(
                  resource, String(test.nonce), ParserInserted,
                  ResourceRequest::RedirectStatus::NoRedirect,
                  SecurityViolationReportingPolicy::SuppressReporting));

    // Report-only 'style-src'
    directiveList = createList(String("style-src ") + test.list,
                               ContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(test.expected,
              directiveList->allowStyleFromSource(
                  resource, String(test.nonce),
                  ResourceRequest::RedirectStatus::NoRedirect,
                  SecurityViolationReportingPolicy::SuppressReporting));

    // Enforce 'style-src'
    directiveList = createList(String("style-src ") + test.list,
                               ContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.expected,
              directiveList->allowStyleFromSource(
                  resource, String(test.nonce),
                  ResourceRequest::RedirectStatus::NoRedirect,
                  SecurityViolationReportingPolicy::SuppressReporting));

    // Report-only 'style-src'
    directiveList = createList(String("default-src ") + test.list,
                               ContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(test.expected,
              directiveList->allowScriptFromSource(
                  resource, String(test.nonce), ParserInserted,
                  ResourceRequest::RedirectStatus::NoRedirect,
                  SecurityViolationReportingPolicy::SuppressReporting));
    EXPECT_EQ(test.expected,
              directiveList->allowStyleFromSource(
                  resource, String(test.nonce),
                  ResourceRequest::RedirectStatus::NoRedirect,
                  SecurityViolationReportingPolicy::SuppressReporting));

    // Enforce 'style-src'
    directiveList = createList(String("default-src ") + test.list,
                               ContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.expected,
              directiveList->allowScriptFromSource(
                  resource, String(test.nonce), ParserInserted,
                  ResourceRequest::RedirectStatus::NoRedirect,
                  SecurityViolationReportingPolicy::SuppressReporting));
    EXPECT_EQ(test.expected,
              directiveList->allowStyleFromSource(
                  resource, String(test.nonce),
                  ResourceRequest::RedirectStatus::NoRedirect,
                  SecurityViolationReportingPolicy::SuppressReporting));
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
       WebURLRequest::RequestContextScript, false},

      // Extra WSP
      {"require-sri-for  script     script  ", "https://example.com/file",
       WebURLRequest::RequestContextScript, false},
      {"require-sri-for      style    script", "https://example.com/file",
       WebURLRequest::RequestContextStyle, false},

      {"require-sri-for style script", "https://example.com/file",
       WebURLRequest::RequestContextScript, false},
      {"require-sri-for style script", "https://example.com/file",
       WebURLRequest::RequestContextImport, false},
      {"require-sri-for style script", "https://example.com/file",
       WebURLRequest::RequestContextImage, true},

      {"require-sri-for script", "https://example.com/file",
       WebURLRequest::RequestContextAudio, true},
      {"require-sri-for script", "https://example.com/file",
       WebURLRequest::RequestContextScript, false},
      {"require-sri-for script", "https://example.com/file",
       WebURLRequest::RequestContextImport, false},
      {"require-sri-for script", "https://example.com/file",
       WebURLRequest::RequestContextServiceWorker, false},
      {"require-sri-for script", "https://example.com/file",
       WebURLRequest::RequestContextSharedWorker, false},
      {"require-sri-for script", "https://example.com/file",
       WebURLRequest::RequestContextWorker, false},
      {"require-sri-for script", "https://example.com/file",
       WebURLRequest::RequestContextStyle, true},

      {"require-sri-for style", "https://example.com/file",
       WebURLRequest::RequestContextAudio, true},
      {"require-sri-for style", "https://example.com/file",
       WebURLRequest::RequestContextScript, true},
      {"require-sri-for style", "https://example.com/file",
       WebURLRequest::RequestContextImport, true},
      {"require-sri-for style", "https://example.com/file",
       WebURLRequest::RequestContextServiceWorker, true},
      {"require-sri-for style", "https://example.com/file",
       WebURLRequest::RequestContextSharedWorker, true},
      {"require-sri-for style", "https://example.com/file",
       WebURLRequest::RequestContextWorker, true},
      {"require-sri-for style", "https://example.com/file",
       WebURLRequest::RequestContextStyle, false},

      // Multiple tokens
      {"require-sri-for script style", "https://example.com/file",
       WebURLRequest::RequestContextStyle, false},
      {"require-sri-for script style", "https://example.com/file",
       WebURLRequest::RequestContextScript, false},
      {"require-sri-for script style", "https://example.com/file",
       WebURLRequest::RequestContextImport, false},
      {"require-sri-for script style", "https://example.com/file",
       WebURLRequest::RequestContextImage, true},

      // Matching is case-insensitive
      {"require-sri-for Script", "https://example.com/file",
       WebURLRequest::RequestContextScript, false},

      // Unknown tokens do not affect result
      {"require-sri-for blabla12 as", "https://example.com/file",
       WebURLRequest::RequestContextScript, true},
      {"require-sri-for blabla12 as  script", "https://example.com/file",
       WebURLRequest::RequestContextScript, false},
      {"require-sri-for script style img", "https://example.com/file",
       WebURLRequest::RequestContextScript, false},
      {"require-sri-for script style img", "https://example.com/file",
       WebURLRequest::RequestContextImport, false},
      {"require-sri-for script style img", "https://example.com/file",
       WebURLRequest::RequestContextStyle, false},
      {"require-sri-for script style img", "https://example.com/file",
       WebURLRequest::RequestContextImage, true},

      // Empty token list has no effect
      {"require-sri-for      ", "https://example.com/file",
       WebURLRequest::RequestContextScript, true},
      {"require-sri-for      ", "https://example.com/file",
       WebURLRequest::RequestContextImport, true},
      {"require-sri-for      ", "https://example.com/file",
       WebURLRequest::RequestContextStyle, true},
      {"require-sri-for      ", "https://example.com/file",
       WebURLRequest::RequestContextServiceWorker, true},
      {"require-sri-for      ", "https://example.com/file",
       WebURLRequest::RequestContextSharedWorker, true},
      {"require-sri-for      ", "https://example.com/file",
       WebURLRequest::RequestContextWorker, true},

      // Order does not matter
      {"require-sri-for a b script", "https://example.com/file",
       WebURLRequest::RequestContextScript, false},
      {"require-sri-for a script b", "https://example.com/file",
       WebURLRequest::RequestContextScript, false},
  };

  for (const auto& test : cases) {
    KURL resource = KURL(KURL(), test.url);
    // Report-only
    Member<CSPDirectiveList> directiveList =
        createList(test.list, ContentSecurityPolicyHeaderTypeReport);
    EXPECT_EQ(true, directiveList->allowRequestWithoutIntegrity(
                        test.context, resource,
                        ResourceRequest::RedirectStatus::NoRedirect,
                        SecurityViolationReportingPolicy::SuppressReporting));

    // Enforce
    directiveList =
        createList(test.list, ContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(
        test.expected,
        directiveList->allowRequestWithoutIntegrity(
            test.context, resource, ResourceRequest::RedirectStatus::NoRedirect,
            SecurityViolationReportingPolicy::SuppressReporting));
  }
}

TEST_F(CSPDirectiveListTest, workerSrc) {
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
      {"child-src *; worker-src 'none'", false},
      {"child-src *; worker-src http://not.example.test", false},
      {"child-src *; worker-src https://example.test", true},
      {"default-src *; child-src *; worker-src 'none'", false},
      {"default-src *; child-src *; worker-src http://not.example.test", false},
      {"default-src *; child-src *; worker-src https://example.test", true},

      // Fallback to child-src.
      {"child-src 'none'", false},
      {"child-src http://not.example.test", false},
      {"child-src https://example.test", true},
      {"default-src *; child-src 'none'", false},
      {"default-src *; child-src http://not.example.test", false},
      {"default-src *; child-src https://example.test", true},

      // Fallback to default-src.
      {"default-src 'none'", false},
      {"default-src http://not.example.test", false},
      {"default-src https://example.test", true},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(test.list);
    KURL resource = KURL(KURL(), "https://example.test/worker.js");
    Member<CSPDirectiveList> directiveList =
        createList(test.list, ContentSecurityPolicyHeaderTypeEnforce);
    EXPECT_EQ(test.allowed,
              directiveList->allowWorkerFromSource(
                  resource, ResourceRequest::RedirectStatus::NoRedirect,
                  SecurityViolationReportingPolicy::SuppressReporting));
  }
}

TEST_F(CSPDirectiveListTest, SubsumesBasedOnCSPSourcesOnly) {
  CSPDirectiveList* A = createList(
      "script-src http://*.one.com; img-src https://one.com "
      "http://two.com/imgs/",
      ContentSecurityPolicyHeaderTypeEnforce);

  struct TestCase {
    const std::vector<const char*> policies;
    bool expected;
    bool expectedFirstPolicyOpposite;
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

  CSPDirectiveList* emptyA =
      createList("", ContentSecurityPolicyHeaderTypeEnforce);

  for (const auto& test : cases) {
    HeapVector<Member<CSPDirectiveList>> listB;
    for (const auto& policy : test.policies) {
      listB.push_back(
          createList(policy, ContentSecurityPolicyHeaderTypeEnforce));
    }

    EXPECT_EQ(test.expected, A->subsumes(listB));
    // Empty CSPDirective subsumes any list.
    EXPECT_TRUE(emptyA->subsumes(listB));
    // Check if first policy of `listB` subsumes `A`.
    EXPECT_EQ(test.expectedFirstPolicyOpposite,
              listB[0]->subsumes(HeapVector<Member<CSPDirectiveList>>(1, A)));
  }
}

TEST_F(CSPDirectiveListTest, SubsumesIfNoneIsPresent) {
  struct TestCase {
    const char* policyA;
    const std::vector<const char*> policiesB;
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
    CSPDirectiveList* A =
        createList(test.policyA, ContentSecurityPolicyHeaderTypeEnforce);

    HeapVector<Member<CSPDirectiveList>> listB;
    for (const auto& policyB : test.policiesB)
      listB.push_back(
          createList(policyB, ContentSecurityPolicyHeaderTypeEnforce));

    EXPECT_EQ(test.expected, A->subsumes(listB));
  }
}

TEST_F(CSPDirectiveListTest, SubsumesPluginTypes) {
  struct TestCase {
    const char* policyA;
    const std::vector<const char*> policiesB;
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
    CSPDirectiveList* A =
        createList(test.policyA, ContentSecurityPolicyHeaderTypeEnforce);

    HeapVector<Member<CSPDirectiveList>> listB;
    for (const auto& policyB : test.policiesB)
      listB.push_back(
          createList(policyB, ContentSecurityPolicyHeaderTypeEnforce));

    EXPECT_EQ(test.expected, A->subsumes(listB));
  }
}

TEST_F(CSPDirectiveListTest, OperativeDirectiveGivenType) {
  enum DefaultBehaviour { Default, NoDefault, ChildAndDefault };

  struct TestCase {
    ContentSecurityPolicy::DirectiveType directive;
    const DefaultBehaviour type;
  } cases[] = {
      // Directives with default directive.
      {ContentSecurityPolicy::DirectiveType::ChildSrc, Default},
      {ContentSecurityPolicy::DirectiveType::ConnectSrc, Default},
      {ContentSecurityPolicy::DirectiveType::FontSrc, Default},
      {ContentSecurityPolicy::DirectiveType::ImgSrc, Default},
      {ContentSecurityPolicy::DirectiveType::ManifestSrc, Default},
      {ContentSecurityPolicy::DirectiveType::MediaSrc, Default},
      {ContentSecurityPolicy::DirectiveType::ObjectSrc, Default},
      {ContentSecurityPolicy::DirectiveType::ScriptSrc, Default},
      {ContentSecurityPolicy::DirectiveType::StyleSrc, Default},
      // Directives with no default directive.
      {ContentSecurityPolicy::DirectiveType::BaseURI, NoDefault},
      {ContentSecurityPolicy::DirectiveType::DefaultSrc, NoDefault},
      {ContentSecurityPolicy::DirectiveType::FrameAncestors, NoDefault},
      {ContentSecurityPolicy::DirectiveType::FormAction, NoDefault},
      // Directive with multiple default directives.
      {ContentSecurityPolicy::DirectiveType::FrameSrc, ChildAndDefault},
      {ContentSecurityPolicy::DirectiveType::WorkerSrc, ChildAndDefault},
  };

  // Initial set-up.
  std::stringstream allDirectives;
  for (const auto& test : cases) {
    const char* name = ContentSecurityPolicy::getDirectiveName(test.directive);
    allDirectives << name << " http://" << name << ".com; ";
  }
  CSPDirectiveList* allDirectivesList = createList(
      allDirectives.str().c_str(), ContentSecurityPolicyHeaderTypeEnforce);
  CSPDirectiveList* empty =
      createList("", ContentSecurityPolicyHeaderTypeEnforce);

  for (const auto& test : cases) {
    const char* name = ContentSecurityPolicy::getDirectiveName(test.directive);
    // When CSPDirectiveList is empty, then `null` should be returned for any
    // type.
    EXPECT_FALSE(empty->operativeDirective(test.directive));

    // When all directives present, then given a type that directive value
    // should be returned.
    HeapVector<Member<CSPSource>> sources =
        allDirectivesList->operativeDirective(test.directive)->m_list;
    EXPECT_EQ(sources.size(), 1u);
    EXPECT_TRUE(sources[0]->m_host.startsWith(name));

    std::stringstream allExceptThis;
    std::stringstream allExceptChildSrcAndThis;
    for (const auto& subtest : cases) {
      if (subtest.directive == test.directive)
        continue;
      const char* directiveName =
          ContentSecurityPolicy::getDirectiveName(subtest.directive);
      allExceptThis << directiveName << " http://" << directiveName << ".com; ";
      if (subtest.directive != ContentSecurityPolicy::DirectiveType::ChildSrc) {
        allExceptChildSrcAndThis << directiveName << " http://" << directiveName
                                 << ".com; ";
      }
    }
    CSPDirectiveList* allExceptThisList = createList(
        allExceptThis.str().c_str(), ContentSecurityPolicyHeaderTypeEnforce);
    CSPDirectiveList* allExceptChildSrcAndThisList =
        createList(allExceptChildSrcAndThis.str().c_str(),
                   ContentSecurityPolicyHeaderTypeEnforce);

    switch (test.type) {
      case Default:
        sources = allExceptThisList->operativeDirective(test.directive)->m_list;
        EXPECT_EQ(sources.size(), 1u);
        EXPECT_EQ(sources[0]->m_host, "default-src.com");
        break;
      case NoDefault:
        EXPECT_FALSE(allExceptThisList->operativeDirective(test.directive));
        break;
      case ChildAndDefault:
        sources = allExceptThisList->operativeDirective(test.directive)->m_list;
        EXPECT_EQ(sources.size(), 1u);
        EXPECT_EQ(sources[0]->m_host, "child-src.com");
        sources =
            allExceptChildSrcAndThisList->operativeDirective(test.directive)
                ->m_list;
        EXPECT_EQ(sources.size(), 1u);
        EXPECT_EQ(sources[0]->m_host, "default-src.com");
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
  HeapVector<Member<CSPDirectiveList>> policyVector;
  for (const auto& policy : policies) {
    policyVector.push_back(
        createList(policy, ContentSecurityPolicyHeaderTypeEnforce));
  }
  HeapVector<Member<SourceListDirective>> result =
      CSPDirectiveList::getSourceVector(
          ContentSecurityPolicy::DirectiveType::DefaultSrc, policyVector);
  EXPECT_EQ(result.size(), 2u);
  result = CSPDirectiveList::getSourceVector(
      ContentSecurityPolicy::DirectiveType::ChildSrc, policyVector);
  EXPECT_EQ(result.size(), 3u);
  result = CSPDirectiveList::getSourceVector(
      ContentSecurityPolicy::DirectiveType::BaseURI, policyVector);
  EXPECT_EQ(result.size(), 1u);
  result = CSPDirectiveList::getSourceVector(
      ContentSecurityPolicy::DirectiveType::FrameSrc, policyVector);
  EXPECT_EQ(result.size(), 4u);

  enum DefaultBehaviour { Default, NoDefault, ChildAndDefault };

  struct TestCase {
    ContentSecurityPolicy::DirectiveType directive;
    const DefaultBehaviour type;
    size_t expectedTotal;
    int expectedCurrent;
    int expectedDefaultSrc;
    int expectedChildSrc;
  } cases[] = {
      // Directives with default directive.
      {ContentSecurityPolicy::DirectiveType::ChildSrc, Default, 4u, 3, 1, 3},
      {ContentSecurityPolicy::DirectiveType::ConnectSrc, Default, 3u, 1, 2, 0},
      {ContentSecurityPolicy::DirectiveType::FontSrc, Default, 3u, 1, 2, 0},
      {ContentSecurityPolicy::DirectiveType::ImgSrc, Default, 3u, 1, 2, 0},
      {ContentSecurityPolicy::DirectiveType::ManifestSrc, Default, 3u, 1, 2, 0},
      {ContentSecurityPolicy::DirectiveType::MediaSrc, Default, 3u, 1, 2, 0},
      {ContentSecurityPolicy::DirectiveType::ObjectSrc, Default, 3u, 1, 2, 0},
      {ContentSecurityPolicy::DirectiveType::ScriptSrc, Default, 3u, 1, 2, 0},
      {ContentSecurityPolicy::DirectiveType::StyleSrc, Default, 3u, 1, 2, 0},
      // Directives with no default directive.
      {ContentSecurityPolicy::DirectiveType::BaseURI, NoDefault, 2u, 2, 0, 0},
      {ContentSecurityPolicy::DirectiveType::FrameAncestors, NoDefault, 1u, 1,
       0, 0},
      {ContentSecurityPolicy::DirectiveType::FormAction, NoDefault, 1u, 1, 0,
       0},
      // Directive with multiple default directives.
      {ContentSecurityPolicy::DirectiveType::FrameSrc, ChildAndDefault, 5u, 2,
       1, 2},
  };

  for (const auto& test : cases) {
    // Initial set-up.
    HeapVector<Member<CSPDirectiveList>> policyVector;
    for (const auto& policy : policies) {
      policyVector.push_back(
          createList(policy, ContentSecurityPolicyHeaderTypeEnforce));
    }
    // Append current test's policy.
    std::stringstream currentDirective;
    const char* name = ContentSecurityPolicy::getDirectiveName(test.directive);
    currentDirective << name << " http://" << name << ".com;";
    policyVector.push_back(createList(currentDirective.str().c_str(),
                                      ContentSecurityPolicyHeaderTypeEnforce));

    HeapVector<Member<SourceListDirective>> result =
        CSPDirectiveList::getSourceVector(test.directive, policyVector);

    EXPECT_EQ(result.size(), test.expectedTotal);

    int actualCurrent = 0, actualDefault = 0, actualChild = 0;
    for (const auto& srcList : result) {
      HeapVector<Member<CSPSource>> sources = srcList->m_list;
      for (const auto& source : sources) {
        if (source->m_host.startsWith(name))
          actualCurrent += 1;
        else if (source->m_host == "default-src.com")
          actualDefault += 1;

        if (source->m_host == "child-src.com")
          actualChild += 1;
      }
    }

    EXPECT_EQ(actualDefault, test.expectedDefaultSrc);
    EXPECT_EQ(actualCurrent, test.expectedCurrent);
    EXPECT_EQ(actualChild, test.expectedChildSrc);

    // If another default-src is added that should only impact Fetch Directives
    policyVector.push_back(createList("default-src https://default-src.com;",
                                      ContentSecurityPolicyHeaderTypeEnforce));
    size_t udpatedTotal =
        test.type != NoDefault ? test.expectedTotal + 1 : test.expectedTotal;
    EXPECT_EQ(
        CSPDirectiveList::getSourceVector(test.directive, policyVector).size(),
        udpatedTotal);
    size_t expectedChildSrc =
        test.directive == ContentSecurityPolicy::DirectiveType::ChildSrc ? 5u
                                                                         : 4u;
    EXPECT_EQ(CSPDirectiveList::getSourceVector(
                  ContentSecurityPolicy::DirectiveType::ChildSrc, policyVector)
                  .size(),
              expectedChildSrc);

    // If another child-src is added that should only impact frame-src and
    // child-src
    policyVector.push_back(createList("child-src http://child-src.com;",
                                      ContentSecurityPolicyHeaderTypeEnforce));
    udpatedTotal =
        test.type == ChildAndDefault ||
                test.directive == ContentSecurityPolicy::DirectiveType::ChildSrc
            ? udpatedTotal + 1
            : udpatedTotal;
    EXPECT_EQ(
        CSPDirectiveList::getSourceVector(test.directive, policyVector).size(),
        udpatedTotal);
    expectedChildSrc = expectedChildSrc + 1u;
    EXPECT_EQ(CSPDirectiveList::getSourceVector(
                  ContentSecurityPolicy::DirectiveType::ChildSrc, policyVector)
                  .size(),
              expectedChildSrc);

    // If we add sandbox, nothing should change since it is currenly not
    // considered.
    policyVector.push_back(createList("sandbox http://sandbox.com;",
                                      ContentSecurityPolicyHeaderTypeEnforce));
    EXPECT_EQ(
        CSPDirectiveList::getSourceVector(test.directive, policyVector).size(),
        udpatedTotal);
    EXPECT_EQ(CSPDirectiveList::getSourceVector(
                  ContentSecurityPolicy::DirectiveType::ChildSrc, policyVector)
                  .size(),
              expectedChildSrc);
  }
}

}  // namespace blink

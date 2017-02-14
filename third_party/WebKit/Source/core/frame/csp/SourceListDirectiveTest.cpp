// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/SourceListDirective.h"

#include "core/dom/Document.h"
#include "core/frame/csp/CSPSource.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class SourceListDirectiveTest : public ::testing::Test {
 public:
  SourceListDirectiveTest() : csp(ContentSecurityPolicy::create()) {}

 protected:
  struct Source {
    String scheme;
    String host;
    const int port;
    String path;
    CSPSource::WildcardDisposition hostWildcard;
    CSPSource::WildcardDisposition portWildcard;
  };

  virtual void SetUp() {
    KURL secureURL(ParsedURLString, "https://example.test/image.png");
    RefPtr<SecurityOrigin> secureOrigin(SecurityOrigin::create(secureURL));
    document = Document::create();
    document->setSecurityOrigin(secureOrigin);
    csp->bindToExecutionContext(document.get());
  }

  ContentSecurityPolicy* SetUpWithOrigin(const String& origin) {
    KURL secureURL(ParsedURLString, origin);
    RefPtr<SecurityOrigin> secureOrigin(SecurityOrigin::create(secureURL));
    Document* document = Document::create();
    document->setSecurityOrigin(secureOrigin);
    ContentSecurityPolicy* csp = ContentSecurityPolicy::create();
    csp->bindToExecutionContext(document);
    return csp;
  }

  bool equalSources(const Source& a, const Source& b) {
    return a.scheme == b.scheme && a.host == b.host && a.port == b.port &&
           a.path == b.path && a.hostWildcard == b.hostWildcard &&
           a.portWildcard == b.portWildcard;
  }

  Persistent<ContentSecurityPolicy> csp;
  Persistent<Document> document;
};

TEST_F(SourceListDirectiveTest, BasicMatchingNone) {
  KURL base;
  String sources = "'none'";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_FALSE(sourceList.allows(KURL(base, "http://example.com/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "https://example.test/")));
}

TEST_F(SourceListDirectiveTest, BasicMatchingStrictDynamic) {
  String sources = "'strict-dynamic'";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_TRUE(sourceList.allowDynamic());
}

TEST_F(SourceListDirectiveTest, BasicMatchingUnsafeHashedAttributes) {
  String sources = "'unsafe-hashed-attributes'";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_TRUE(sourceList.allowHashedAttributes());
}

TEST_F(SourceListDirectiveTest, BasicMatchingStar) {
  KURL base;
  String sources = "*";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_TRUE(sourceList.allows(KURL(base, "http://example.com/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example.com/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://example.com/bar")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://foo.example.com/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://foo.example.com/bar")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "ftp://example.com/")));

  EXPECT_FALSE(sourceList.allows(KURL(base, "data:https://example.test/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "blob:https://example.test/")));
  EXPECT_FALSE(
      sourceList.allows(KURL(base, "filesystem:https://example.test/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "file:///etc/hosts")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "applewebdata://example.test/")));
}

TEST_F(SourceListDirectiveTest, StarallowsSelf) {
  KURL base;
  String sources = "*";
  SourceListDirective sourceList("script-src", sources, csp.get());

  // With a protocol of 'file', '*' allows 'file:':
  RefPtr<SecurityOrigin> origin = SecurityOrigin::create("file", "", 0);
  csp->setupSelf(*origin);
  EXPECT_TRUE(sourceList.allows(KURL(base, "file:///etc/hosts")));

  // The other results are the same as above:
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://example.com/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example.com/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://example.com/bar")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://foo.example.com/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://foo.example.com/bar")));

  EXPECT_FALSE(sourceList.allows(KURL(base, "data:https://example.test/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "blob:https://example.test/")));
  EXPECT_FALSE(
      sourceList.allows(KURL(base, "filesystem:https://example.test/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "applewebdata://example.test/")));
}

TEST_F(SourceListDirectiveTest, BasicMatchingSelf) {
  KURL base;
  String sources = "'self'";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_FALSE(sourceList.allows(KURL(base, "http://example.com/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "https://not-example.com/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example.test/")));
}

TEST_F(SourceListDirectiveTest, BlobMatchingSelf) {
  KURL base;
  String sources = "'self'";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example.test/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "blob:https://example.test/")));

  // Register "https" as bypassing CSP, which should trigger the innerURL
  // behavior.
  SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy("https");

  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example.test/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "blob:https://example.test/")));

  // Unregister the scheme to clean up after ourselves.
  SchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      "https");
}

TEST_F(SourceListDirectiveTest, FilesystemMatchingSelf) {
  KURL base;
  String sources = "'self'";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example.test/")));
  EXPECT_FALSE(sourceList.allows(
      KURL(base, "filesystem:https://example.test/file.txt")));

  // Register "https" as bypassing CSP, which should trigger the innerURL
  // behavior.
  SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy("https");

  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example.test/")));
  EXPECT_TRUE(sourceList.allows(
      KURL(base, "filesystem:https://example.test/file.txt")));

  // Unregister the scheme to clean up after ourselves.
  SchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      "https");
}

TEST_F(SourceListDirectiveTest, BlobDisallowedWhenBypassingSelfScheme) {
  KURL base;
  String sources = "'self' blob:";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_TRUE(sourceList.allows(
      KURL(base, "blob:https://example.test/1be95204-93d6-4GUID")));
  EXPECT_TRUE(sourceList.allows(
      KURL(base, "blob:https://not-example.test/1be95204-93d6-4GUID")));

  // Register "https" as bypassing CSP, which should trigger the innerURL
  // behavior.
  SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy("https");

  EXPECT_TRUE(sourceList.allows(
      KURL(base, "blob:https://example.test/1be95204-93d6-4GUID")));
  // TODO(mkwst, arthursonzogni): This should be true.
  // See http://crbug.com/692046
  EXPECT_FALSE(sourceList.allows(
      KURL(base, "blob:https://not-example.test/1be95204-93d6-4GUID")));

  // Unregister the scheme to clean up after ourselves.
  SchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      "https");
}

TEST_F(SourceListDirectiveTest, FilesystemDisallowedWhenBypassingSelfScheme) {
  KURL base;
  String sources = "'self' filesystem:";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_TRUE(sourceList.allows(
      KURL(base, "filesystem:https://example.test/file.txt")));
  EXPECT_TRUE(sourceList.allows(
      KURL(base, "filesystem:https://not-example.test/file.txt")));

  // Register "https" as bypassing CSP, which should trigger the innerURL
  // behavior.
  SchemeRegistry::registerURLSchemeAsBypassingContentSecurityPolicy("https");

  EXPECT_TRUE(sourceList.allows(
      KURL(base, "filesystem:https://example.test/file.txt")));
  // TODO(mkwst, arthursonzogni): This should be true.
  // See http://crbug.com/692046
  EXPECT_FALSE(sourceList.allows(
      KURL(base, "filesystem:https://not-example.test/file.txt")));

  // Unregister the scheme to clean up after ourselves.
  SchemeRegistry::removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      "https");
}

TEST_F(SourceListDirectiveTest, BlobMatchingBlob) {
  KURL base;
  String sources = "blob:";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_FALSE(sourceList.allows(KURL(base, "https://example.test/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "blob:https://example.test/")));
}

TEST_F(SourceListDirectiveTest, BasicMatching) {
  KURL base;
  String sources = "http://example1.com:8000/foo/ https://example2.com/";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_TRUE(sourceList.allows(KURL(base, "http://example1.com:8000/foo/")));
  EXPECT_TRUE(
      sourceList.allows(KURL(base, "http://example1.com:8000/foo/bar")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example2.com/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example2.com/foo/")));

  EXPECT_FALSE(sourceList.allows(KURL(base, "https://not-example.com/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "http://example1.com/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "https://example1.com/foo")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "http://example1.com:9000/foo/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "http://example1.com:8000/FOO/")));
}

TEST_F(SourceListDirectiveTest, WildcardMatching) {
  KURL base;
  String sources =
      "http://example1.com:*/foo/ https://*.example2.com/bar/ http://*.test/";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_TRUE(sourceList.allows(KURL(base, "http://example1.com/foo/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://example1.com:8000/foo/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://example1.com:9000/foo/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://foo.example2.com/bar/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://foo.test/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "http://foo.bar.test/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example1.com/foo/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example1.com:8000/foo/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://example1.com:9000/foo/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://foo.test/")));
  EXPECT_TRUE(sourceList.allows(KURL(base, "https://foo.bar.test/")));

  EXPECT_FALSE(sourceList.allows(KURL(base, "https://example1.com:8000/foo")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "https://example2.com:8000/bar")));
  EXPECT_FALSE(
      sourceList.allows(KURL(base, "https://foo.example2.com:8000/bar")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "https://example2.foo.com/bar")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "http://foo.test.bar/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "https://example2.com/bar/")));
  EXPECT_FALSE(sourceList.allows(KURL(base, "http://test/")));
}

TEST_F(SourceListDirectiveTest, RedirectMatching) {
  KURL base;
  String sources = "http://example1.com/foo/ http://example2.com/bar/";
  SourceListDirective sourceList("script-src", sources, csp.get());

  EXPECT_TRUE(
      sourceList.allows(KURL(base, "http://example1.com/foo/"),
                        ResourceRequest::RedirectStatus::FollowedRedirect));
  EXPECT_TRUE(
      sourceList.allows(KURL(base, "http://example1.com/bar/"),
                        ResourceRequest::RedirectStatus::FollowedRedirect));
  EXPECT_TRUE(
      sourceList.allows(KURL(base, "http://example2.com/bar/"),
                        ResourceRequest::RedirectStatus::FollowedRedirect));
  EXPECT_TRUE(
      sourceList.allows(KURL(base, "http://example2.com/foo/"),
                        ResourceRequest::RedirectStatus::FollowedRedirect));
  EXPECT_TRUE(
      sourceList.allows(KURL(base, "https://example1.com/foo/"),
                        ResourceRequest::RedirectStatus::FollowedRedirect));
  EXPECT_TRUE(
      sourceList.allows(KURL(base, "https://example1.com/bar/"),
                        ResourceRequest::RedirectStatus::FollowedRedirect));

  EXPECT_FALSE(
      sourceList.allows(KURL(base, "http://example3.com/foo/"),
                        ResourceRequest::RedirectStatus::FollowedRedirect));
}

TEST_F(SourceListDirectiveTest, GetIntersectCSPSources) {
  String sources =
      "http://example1.com/foo/ http://*.example2.com/bar/ "
      "http://*.example3.com:*/bar/";
  SourceListDirective sourceList("script-src", sources, csp.get());
  struct TestCase {
    String sources;
    String expected;
  } cases[] = {
      {"http://example1.com/foo/ http://example2.com/bar/",
       "http://example1.com/foo/ http://example2.com/bar/"},
      // Normalizing schemes.
      {"https://example1.com/foo/ http://example2.com/bar/",
       "https://example1.com/foo/ http://example2.com/bar/"},
      {"https://example1.com/foo/ https://example2.com/bar/",
       "https://example1.com/foo/ https://example2.com/bar/"},
      {"https://example1.com/foo/ wss://example2.com/bar/",
       "https://example1.com/foo/"},
      // Normalizing hosts.
      {"http://*.example1.com/foo/ http://*.example2.com/bar/",
       "http://example1.com/foo/ http://*.example2.com/bar/"},
      {"http://*.example1.com/foo/ http://foo.example2.com/bar/",
       "http://example1.com/foo/ http://foo.example2.com/bar/"},
      // Normalizing ports.
      {"http://example1.com/foo/ http://example2.com/bar/",
       "http://example1.com/foo/ http://example2.com/bar/"},
      {"http://example1.com/foo/ http://example2.com:90/bar/",
       "http://example1.com/foo/"},
      {"http://example1.com:*/foo/ http://example2.com/bar/",
       "http://example1.com/foo/ http://example2.com/bar/"},
      {"http://*.example3.com:100/bar/ http://example1.com/foo/",
       "http://example1.com/foo/ http://*.example3.com:100/bar/"},
      // Normalizing paths.
      {"http://example1.com/ http://example2.com/",
       "http://example1.com/foo/ http://example2.com/bar/"},
      {"http://example1.com/foo/index.html http://example2.com/bar/",
       "http://example1.com/foo/index.html http://example2.com/bar/"},
      {"http://example1.com/bar http://example2.com/bar/",
       "http://example2.com/bar/"},
      // Not similar to be normalized
      {"http://non-example1.com/foo/ http://non-example2.com/bar/", ""},
      {"https://non-example1.com/foo/ wss://non-example2.com/bar/", ""},
  };

  for (const auto& test : cases) {
    SourceListDirective secondList("script-src", test.sources, csp.get());
    HeapVector<Member<CSPSource>> normalized =
        sourceList.getIntersectCSPSources(secondList.m_list);
    SourceListDirective helperSourceList("script-src", test.expected,
                                         csp.get());
    HeapVector<Member<CSPSource>> expected = helperSourceList.m_list;
    EXPECT_EQ(normalized.size(), expected.size());
    for (size_t i = 0; i < normalized.size(); i++) {
      Source a = {normalized[i]->m_scheme,       normalized[i]->m_host,
                  normalized[i]->m_port,         normalized[i]->m_path,
                  normalized[i]->m_hostWildcard, normalized[i]->m_portWildcard};
      Source b = {expected[i]->m_scheme,       expected[i]->m_host,
                  expected[i]->m_port,         expected[i]->m_path,
                  expected[i]->m_hostWildcard, expected[i]->m_portWildcard};
      EXPECT_TRUE(equalSources(a, b));
    }
  }
}

TEST_F(SourceListDirectiveTest, GetIntersectCSPSourcesSchemes) {
  SourceListDirective listA("script-src",
                            "http: http://example1.com/foo/ "
                            "https://example1.com/foo/ "
                            "http://example1.com/bar/page.html "
                            "wss: ws://another.test/bar/",
                            csp.get());
  struct TestCase {
    String sources;
    String expected;
    String expectedReversed;
  } cases[] = {{"http:", "http:"},
               {"https:", "https:"},
               {"ws:", "wss: ws://another.test/bar/"},
               {"wss:", "wss:"},
               {"https: ws:", "wss: https: ws://another.test/bar/"},
               {"https: http: wss:", "http: wss:"},
               {"https: http: wss:", "http: wss:"},
               {"https: http://another-example1.com/bar/",
                "https: http://another-example1.com/bar/"},
               {"http://*.example1.com/",
                "http://*.example1.com/ http://example1.com/foo/ "
                "https://example1.com/foo/ http://example1.com/bar/page.html"},
               {"http://example1.com/foo/ https://example1.com/foo/",
                "http://example1.com/foo/ https://example1.com/foo/ "
                "http://example1.com/foo/ https://example1.com/foo/"},
               {"https://example1.com/foo/ http://example1.com/foo/",
                "https://example1.com/foo/ http://example1.com/foo/ "
                "http://example1.com/foo/ https://example1.com/foo/"},
               // If exaclty the same policy is specified, it is optimized.
               {"http: http://example1.com/foo/ https://example1.com/foo/ "
                "http://example1.com/bar/page.html wss: ws://another.test/bar/",
                "http: wss: ws://another.test/bar/"}};

  for (const auto& test : cases) {
    SourceListDirective listB("script-src", test.sources, csp.get());
    HeapVector<Member<CSPSource>> normalized =
        listA.getIntersectCSPSources(listB.m_list);

    SourceListDirective helperSourceList("script-src", test.expected,
                                         csp.get());
    HeapVector<Member<CSPSource>> expected = helperSourceList.m_list;
    EXPECT_EQ(normalized.size(), expected.size());
    for (size_t i = 0; i < expected.size(); i++) {
      Source a = {expected[i]->m_scheme,       expected[i]->m_host,
                  expected[i]->m_port,         expected[i]->m_path,
                  expected[i]->m_hostWildcard, expected[i]->m_portWildcard};
      Source b = {normalized[i]->m_scheme,       normalized[i]->m_host,
                  normalized[i]->m_port,         normalized[i]->m_path,
                  normalized[i]->m_hostWildcard, normalized[i]->m_portWildcard};
      EXPECT_TRUE(equalSources(a, b));
    }
  }
}

TEST_F(SourceListDirectiveTest, Subsumes) {
  KURL base;
  String requiredSources =
      "http://example1.com/foo/ http://*.example2.com/bar/ "
      "http://*.example3.com:*/bar/";
  SourceListDirective required("script-src", requiredSources, csp.get());

  struct TestCase {
    std::vector<String> sourcesVector;
    bool expected;
  } cases[] = {
      // Non-intersecting source lists give an effective policy of 'none', which
      // is always subsumed.
      {{"http://example1.com/bar/", "http://*.example3.com:*/bar/"}, true},
      {{"http://example1.com/bar/",
        "http://*.example3.com:*/bar/ http://*.example2.com/bar/"},
       true},
      // Lists that intersect into one of the required sources are subsumed.
      {{"http://example1.com/foo/"}, true},
      {{"http://*.example2.com/bar/"}, true},
      {{"http://*.example3.com:*/bar/"}, true},
      {{"https://example1.com/foo/",
        "http://*.example1.com/foo/ http://*.example2.com/bar/"},
       true},
      {{"http://example2.com/bar/",
        "http://*.example3.com:*/bar/ http://*.example2.com/bar/"},
       true},
      {{"http://example3.com:100/bar/",
        "http://*.example3.com:*/bar/ http://*.example2.com/bar/"},
       true},
      // Lists that intersect into two of the required sources are subsumed.
      {{"http://example1.com/foo/ http://*.example2.com/bar/"}, true},
      {{"http://example1.com/foo/ http://example2.com/bar/",
        "http://example2.com/bar/ http://example1.com/foo/"},
       true},
      // Ordering should not matter.
      {{"https://example1.com/foo/ https://example2.com/bar/",
        "http://example2.com/bar/ http://example1.com/foo/"},
       true},
      // Lists that intersect into a policy identical to the required list are
      // subsumed.
      {{"http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ http://example1.com/foo/"},
       true},
      {{"http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/"},
       true},
      {{"http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/",
        "http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ http://example4.com/foo/"},
       true},
      {{"http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/",
        "http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ http://example1.com/foo/"},
       true},
      // Lists that include sources that aren't subsumed by the required list
      // are not subsumed.
      {{"http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ http://*.example4.com:*/bar/"},
       false},
      {{"http://example1.com/foo/ http://example2.com/foo/"}, false},
      {{"http://*.example1.com/bar/", "http://example1.com/bar/"}, false},
      {{"http://*.example1.com/foo/"}, false},
      {{"wss://example2.com/bar/"}, false},
      {{"http://*.non-example3.com:*/bar/"}, false},
      {{"http://example3.com/foo/"}, false},
      {{"http://not-example1.com", "http://not-example1.com"}, false},
  };

  for (const auto& test : cases) {
    HeapVector<Member<SourceListDirective>> returned;

    for (const auto& sources : test.sourcesVector) {
      SourceListDirective* member =
          new SourceListDirective("script-src", sources, csp.get());
      returned.push_back(member);
    }

    EXPECT_EQ(required.subsumes(returned), test.expected);
  }
}

TEST_F(SourceListDirectiveTest, SubsumesWithSelf) {
  SourceListDirective A("script-src",
                        "http://example1.com/foo/ http://*.example2.com/bar/ "
                        "http://*.example3.com:*/bar/ 'self'",
                        csp.get());

  struct TestCase {
    std::vector<const char*> sourcesB;
    const char* originB;
    bool expected;
  } cases[] = {
      // "https://example.test/" is a secure origin for both A and B.
      {{"'self'"}, "https://example.test/", true},
      {{"'self' 'self' 'self'"}, "https://example.test/", true},
      {{"'self'", "'self'", "'self'"}, "https://example.test/", true},
      {{"'self'", "'self'", "https://*.example.test/"},
       "https://example.test/",
       true},
      {{"'self'", "'self'", "https://*.example.test/bar/"},
       "https://example.test/",
       true},
      {{"'self' https://another.test/bar", "'self' http://*.example.test/bar",
        "https://*.example.test/bar/"},
       "https://example.test/",
       true},
      {{"http://example1.com/foo/ 'self'"}, "https://example.test/", true},
      {{"http://example1.com/foo/ https://example.test/"},
       "https://example.test/",
       true},
      {{"http://example1.com/foo/ http://*.example2.com/bar/"},
       "https://example.test/",
       true},
      {{"http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ https://example.test/"},
       "https://example.test/",
       true},
      {{"http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ 'self'"},
       "https://example.test/",
       true},
      {{"'self'", "'self'", "https://example.test/"},
       "https://example.test/",
       true},
      {{"'self'", "https://example.test/folder/"},
       "https://example.test/",
       true},
      {{"'self'", "http://example.test/folder/"},
       "https://example.test/",
       true},
      {{"'self' https://example.com/", "https://example.com/"},
       "https://example.test/",
       false},
      {{"http://example1.com/foo/ http://*.example2.com/bar/",
        "http://example1.com/foo/ http://*.example2.com/bar/ 'self'"},
       "https://example.test/",
       true},
      {{"http://*.example1.com/foo/", "http://*.example1.com/foo/ 'self'"},
       "https://example.test/",
       false},
      {{"https://*.example.test/", "https://*.example.test/ 'self'"},
       "https://example.test/",
       false},
      {{"http://example.test/"}, "https://example.test/", false},
      {{"https://example.test/"}, "https://example.test/", true},
      // Origins of A and B do not match.
      {{"https://example.test/"}, "https://other-origin.test/", false},
      {{"'self'"}, "https://other-origin.test/", true},
      {{"http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ 'self'"},
       "https://other-origin.test/",
       true},
      {{"http://example1.com/foo/ http://*.example2.com/bar/ "
        "http://*.example3.com:*/bar/ https://other-origin.test/"},
       "https://other-origin.test/",
       true},
      {{"http://example1.com/foo/ 'self'"}, "https://other-origin.test/", true},
      {{"'self'", "https://example.test/"}, "https://other-origin.test/", true},
      {{"'self' https://example.test/", "https://example.test/"},
       "https://other-origin.test/",
       false},
      {{"https://example.test/", "http://example.test/"},
       "https://other-origin.test/",
       false},
      {{"'self'", "http://other-origin.test/"},
       "https://other-origin.test/",
       true},
      {{"'self'", "https://non-example.test/"},
       "https://other-origin.test/",
       true},
      // B's origin matches one of sources in the source list of A.
      {{"'self'", "http://*.example1.com/foo/"}, "http://example1.com/", true},
      {{"http://*.example2.com/bar/", "'self'"},
       "http://example2.com/bar/",
       true},
      {{"'self' http://*.example1.com/foo/", "http://*.example1.com/foo/"},
       "http://example1.com/",
       false},
      {{"http://*.example2.com/bar/ http://example1.com/",
        "'self' http://example1.com/"},
       "http://example2.com/bar/",
       false},
  };

  for (const auto& test : cases) {
    ContentSecurityPolicy* cspB = SetUpWithOrigin(String(test.originB));

    HeapVector<Member<SourceListDirective>> vectorB;
    for (const auto& sources : test.sourcesB) {
      SourceListDirective* member =
          new SourceListDirective("script-src", sources, cspB);
      vectorB.push_back(member);
    }

    EXPECT_EQ(test.expected, A.subsumes(vectorB));
  }
}

TEST_F(SourceListDirectiveTest, AllowAllInline) {
  struct TestCase {
    String sources;
    bool expected;
  } cases[] = {
      // List does not contain 'unsafe-inline'.
      {"http://example1.com/foo/", false},
      {"'sha512-321cba'", false},
      {"'nonce-yay'", false},
      {"'strict-dynamic'", false},
      {"'sha512-321cba' http://example1.com/foo/", false},
      {"http://example1.com/foo/ 'sha512-321cba'", false},
      {"http://example1.com/foo/ 'nonce-yay'", false},
      {"'sha512-321cba' 'nonce-yay'", false},
      {"http://example1.com/foo/ 'sha512-321cba' 'nonce-yay'", false},
      {"http://example1.com/foo/ 'sha512-321cba' 'nonce-yay'", false},
      {" 'sha512-321cba' 'nonce-yay' 'strict-dynamic'", false},
      // List contains 'unsafe-inline'.
      {"'unsafe-inline'", true},
      {"'self' 'unsafe-inline'", true},
      {"'unsafe-inline' http://example1.com/foo/", true},
      {"'sha512-321cba' 'unsafe-inline'", false},
      {"'nonce-yay' 'unsafe-inline'", false},
      {"'strict-dynamic' 'unsafe-inline' 'nonce-yay'", false},
      {"'sha512-321cba' http://example1.com/foo/ 'unsafe-inline'", false},
      {"http://example1.com/foo/ 'sha512-321cba' 'unsafe-inline'", false},
      {"http://example1.com/foo/ 'nonce-yay' 'unsafe-inline'", false},
      {"'sha512-321cba' 'nonce-yay' 'unsafe-inline'", false},
      {"http://example1.com/foo/ 'sha512-321cba' 'unsafe-inline' 'nonce-yay'",
       false},
      {"http://example1.com/foo/ 'sha512-321cba' 'nonce-yay' 'unsafe-inline'",
       false},
      {" 'sha512-321cba' 'unsafe-inline' 'nonce-yay' 'strict-dynamic'", false},
  };

  // Script-src and style-src differently handle presence of 'strict-dynamic'.
  SourceListDirective scriptSrc("script-src",
                                "'strict-dynamic' 'unsafe-inline'", csp.get());
  EXPECT_FALSE(scriptSrc.allowAllInline());

  SourceListDirective styleSrc("style-src", "'strict-dynamic' 'unsafe-inline'",
                               csp.get());
  EXPECT_TRUE(styleSrc.allowAllInline());

  for (const auto& test : cases) {
    SourceListDirective scriptSrc("script-src", test.sources, csp.get());
    EXPECT_EQ(scriptSrc.allowAllInline(), test.expected);

    SourceListDirective styleSrc("style-src", test.sources, csp.get());
    EXPECT_EQ(styleSrc.allowAllInline(), test.expected);

    // If source list doesn't have a valid type, it must not allow all inline.
    SourceListDirective imgSrc("img-src", test.sources, csp.get());
    EXPECT_FALSE(imgSrc.allowAllInline());
  }
}

TEST_F(SourceListDirectiveTest, SubsumesAllowAllInline) {
  struct TestCase {
    bool isScriptSrc;
    String sourcesA;
    std::vector<String> sourcesB;
    bool expected;
  } cases[] = {
      // `sourcesA` allows all inline behavior.
      {false,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic'",
       {"'unsafe-inline' http://example1.com/foo/bar.html"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline'",
       {"http://example1.com/foo/ 'unsafe-inline'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline'",
       {"'unsafe-inline' 'nonce-yay'", "'unsafe-inline'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline'",
       {"'unsafe-inline' 'nonce-yay'", "'unsafe-inline'", "'strict-dynamic'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline'",
       {"'unsafe-inline' 'nonce-yay'", "'unsafe-inline'",
        "'strict-dynamic' 'nonce-yay'"},
       true},
      // `sourcesA` does not allow all inline behavior.
      {false,
       "http://example1.com/foo/ 'self' 'strict-dynamic'",
       {"'unsafe-inline' http://example1.com/foo/bar.html"},
       false},
      {true, "http://example1.com/foo/ 'self'", {"'unsafe-inline'"}, false},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline'",
       {"'unsafe-inline' 'nonce-yay'", "'nonce-abc'"},
       true},
      {true,
       "http://example1.com/foo/ 'self'",
       {"'unsafe-inline' https://example.test/"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic'",
       {"'unsafe-inline' https://example.test/"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic'",
       {"'unsafe-inline' 'strict-dynamic'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'nonce-yay'",
       {"'unsafe-inline' 'nonce-yay'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic' "
       "'nonce-yay'",
       {"'unsafe-inline' 'nonce-yay'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic' "
       "'nonce-yay'",
       {"http://example1.com/foo/ 'unsafe-inline' 'strict-dynamic'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321cba' "
       "'strict-dynamic'",
       {"'unsafe-inline' 'sha512-321cba'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic' "
       "'sha512-321cba'",
       {"http://example1.com/foo/ 'unsafe-inline' 'strict-dynamic'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321cba'",
       {"http://example1.com/foo/ 'unsafe-inline'",
        "http://example1.com/foo/ 'sha512-321cba'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321cba'",
       {"http://example1.com/foo/ 'unsafe-inline'",
        "http://example1.com/foo/ 'unsafe-inline' 'sha512-321cba'"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321cba'",
       {"http://example1.com/foo/ 'unsafe-inline' 'nonce-yay'",
        "http://example1.com/foo/ 'unsafe-inline' 'sha512-321cba'"},
       true},
  };

  for (const auto& test : cases) {
    SourceListDirective A(test.isScriptSrc ? "script-src" : "style-src",
                          test.sourcesA, csp.get());
    ContentSecurityPolicy* cspB =
        SetUpWithOrigin("https://another.test/image.png");

    HeapVector<Member<SourceListDirective>> vectorB;
    for (const auto& sources : test.sourcesB) {
      SourceListDirective* member = new SourceListDirective(
          test.isScriptSrc ? "script-src" : "style-src", sources, cspB);
      vectorB.push_back(member);
    }

    EXPECT_EQ(A.subsumes(vectorB), test.expected);
  }
}

TEST_F(SourceListDirectiveTest, SubsumesUnsafeAttributes) {
  struct TestCase {
    bool isScriptSrc;
    String sourcesA;
    std::vector<String> sourcesB;
    bool expected;
  } cases[] = {
      // A or policiesB contain `unsafe-eval`.
      {false,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic' "
       "'unsafe-eval'",
       {"http://example1.com/foo/bar.html 'unsafe-eval'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-eval'",
       {"http://example1.com/foo/ 'unsafe-inline'"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-eval'",
       {"http://example1.com/foo/ 'unsafe-inline' 'unsafe-eval'"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-eval'",
       {"http://example1.com/foo/ 'unsafe-eval'",
        "http://example1.com/foo/bar 'self' unsafe-eval'",
        "http://non-example.com/foo/ 'unsafe-eval' 'self'"},
       true},
      {true,
       "http://example1.com/foo/ 'self'",
       {"http://example1.com/foo/ 'unsafe-eval'"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline'",
       {"http://example1.com/foo/ 'unsafe-eval'",
        "http://example1.com/foo/bar 'self' 'unsafe-eval'",
        "http://non-example.com/foo/ 'unsafe-eval' 'self'"},
       false},
      // A or policiesB contain `unsafe-hashed-attributes`.
      {false,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'unsafe-eval' "
       "'strict-dynamic' "
       "'unsafe-hashed-attributes'",
       {"http://example1.com/foo/bar.html 'unsafe-hashed-attributes'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-hashed-attributes'",
       {"http://example1.com/foo/ 'unsafe-inline'"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-hashed-attributes'",
       {"http://example1.com/foo/ 'unsafe-inline' 'unsafe-hashed-attributes'"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-eval' "
       "'unsafe-hashed-attributes'",
       {"http://example1.com/foo/ 'unsafe-eval' 'unsafe-hashed-attributes'",
        "http://example1.com/foo/bar 'self' 'unsafe-hashed-attributes'",
        "http://non-example.com/foo/ 'unsafe-hashed-attributes' 'self'"},
       true},
      {true,
       "http://example1.com/foo/ 'self'",
       {"http://example1.com/foo/ 'unsafe-hashed-attributes'"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline'",
       {"http://example1.com/foo/ 'unsafe-hashed-attributes'",
        "http://example1.com/foo/bar 'self' 'unsafe-hashed-attributes'",
        "https://example1.com/foo/bar 'unsafe-hashed-attributes' 'self'"},
       false},
  };

  ContentSecurityPolicy* cspB =
      SetUpWithOrigin("https://another.test/image.png");

  for (const auto& test : cases) {
    SourceListDirective A(test.isScriptSrc ? "script-src" : "style-src",
                          test.sourcesA, csp.get());

    HeapVector<Member<SourceListDirective>> vectorB;
    for (const auto& sources : test.sourcesB) {
      SourceListDirective* member = new SourceListDirective(
          test.isScriptSrc ? "script-src" : "style-src", sources, cspB);
      vectorB.push_back(member);
    }

    EXPECT_EQ(A.subsumes(vectorB), test.expected);
  }
}

TEST_F(SourceListDirectiveTest, IsNone) {
  struct TestCase {
    String sources;
    bool expected;
  } cases[] = {
      // Source list is 'none'.
      {"'none'", true},
      {"", true},
      {"   ", true},
      // Source list is not 'none'.
      {"http://example1.com/foo/", false},
      {"'sha512-321cba'", false},
      {"'nonce-yay'", false},
      {"'strict-dynamic'", false},
      {"'sha512-321cba' http://example1.com/foo/", false},
      {"http://example1.com/foo/ 'sha512-321cba'", false},
      {"http://example1.com/foo/ 'nonce-yay'", false},
      {"'none' 'sha512-321cba' http://example1.com/foo/", false},
      {"'none' http://example1.com/foo/ 'sha512-321cba'", false},
      {"'none' http://example1.com/foo/ 'nonce-yay'", false},
      {"'sha512-321cba' 'nonce-yay'", false},
      {"http://example1.com/foo/ 'sha512-321cba' 'nonce-yay'", false},
      {"http://example1.com/foo/ 'sha512-321cba' 'nonce-yay'", false},
      {" 'sha512-321cba' 'nonce-yay' 'strict-dynamic'", false},
  };

  for (const auto& test : cases) {
    SourceListDirective scriptSrc("script-src", test.sources, csp.get());
    EXPECT_EQ(scriptSrc.isNone(), test.expected);

    SourceListDirective styleSrc("form-action", test.sources, csp.get());
    EXPECT_EQ(styleSrc.isNone(), test.expected);

    SourceListDirective imgSrc("frame-src", test.sources, csp.get());
    EXPECT_EQ(styleSrc.isNone(), test.expected);
  }
}

TEST_F(SourceListDirectiveTest, GetIntersectNonces) {
  SourceListDirective listA(
      "script-src",
      "http://example.com 'nonce-abc' 'nonce-xyz' 'nonce-' 'unsafe-inline'",
      csp.get());
  struct TestCase {
    String sources;
    String expected;
  } cases[] = {
      {"http:", ""},
      {"http://example.com", ""},
      {"example.com", ""},
      {"'unsafe-inline'", ""},
      {"'nonce-abc'", "'nonce-abc'"},
      {"'nonce-xyz'", "'nonce-xyz'"},
      {"'nonce-123'", ""},
      {"'nonce-abc' 'nonce-xyz'", "'nonce-abc' 'nonce-xyz'"},
      {"'nonce-abc' 'nonce-xyz' 'nonce'", "'nonce-abc' 'nonce-xyz'"},
      {"'nonce-abc' 'nonce-123'", "'nonce-abc'"},
      {"'nonce-123' 'nonce-123'", ""},
      {"'nonce-123' 'nonce-abc'", "'nonce-abc'"},
      {"'nonce-123' 'nonce-xyz'", "'nonce-xyz'"},
      {"'nonce-123' 'nonce-xyx'", ""},
  };

  for (const auto& test : cases) {
    SourceListDirective listB("script-src", test.sources, csp.get());
    HashSet<String> normalized = listA.getIntersectNonces(listB.m_nonces);

    SourceListDirective expectedList("script-src", test.expected, csp.get());
    HashSet<String> expected = expectedList.m_nonces;
    EXPECT_EQ(normalized.size(), expected.size());
    for (const auto& nonce : normalized) {
      EXPECT_TRUE(expected.contains(nonce));
    }
  }
}

TEST_F(SourceListDirectiveTest, GetIntersectHashes) {
  SourceListDirective listA(
      "script-src",
      "http://example.com 'sha256-abc123' 'sha384-' 'sha512-321cba' 'self'",
      csp.get());
  struct TestCase {
    String sources;
    String expected;
  } cases[] = {
      {"http:", ""},
      {"http://example.com", ""},
      {"example.com", ""},
      {"'unsafe-inline'", ""},
      {"'sha384-abc'", ""},
      {"'sha384-'", ""},
      {"'sha256-abc123'", "'sha256-abc123'"},
      {"'sha256-abc123' 'sha384-'", "'sha256-abc123'"},
      {"'sha256-abc123' 'sha512-321cba'", "'sha512-321cba' 'sha256-abc123'"},
      {"'sha256-abc123' 'sha384-' 'sha512-321cba'",
       "'sha256-abc123' 'sha512-321cba' "},
      {"'sha256-else' 'sha384-' 'sha512-321cba'", "'sha512-321cba' "},
      {"'hash-123'", ""},
      {"'sha256-123'", ""},
  };

  for (const auto& test : cases) {
    SourceListDirective listB("script-src", test.sources, csp.get());
    HashSet<CSPHashValue> normalized = listA.getIntersectHashes(listB.m_hashes);

    SourceListDirective expectedList("script-src", test.expected, csp.get());
    HashSet<CSPHashValue> expected = expectedList.m_hashes;
    EXPECT_EQ(normalized.size(), expected.size());
    for (const auto& hash : normalized) {
      EXPECT_TRUE(expected.contains(hash));
    }
  }
}

TEST_F(SourceListDirectiveTest, SubsumesNoncesAndHashes) {
  struct TestCase {
    bool isScriptSrc;
    String sourcesA;
    std::vector<String> sourcesB;
    bool expected;
  } cases[] = {
      // Check nonces.
      {true,
       "http://example1.com/foo/ 'unsafe-inline' 'nonce-abc'",
       {"'unsafe-inline'"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'nonce-abc'",
       {"'nonce-abc'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline'",
       {"'unsafe-inline' 'nonce-yay'", "'nonce-yay'"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'nonce-yay'",
       {"'unsafe-inline' 'nonce-yay'", "'nonce-yay'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'nonce-abc' 'nonce-yay'",
       {"'unsafe-inline' https://example.test/"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'nonce-abc' 'nonce-yay'",
       {"'nonce-abc' https://example1.com/foo/"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'nonce-yay' "
       "'strict-dynamic'",
       {"https://example.test/ 'nonce-yay'"},
       false},
      {false,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'nonce-yay' "
       "'strict-dynamic'",
       {"'nonce-yay' https://example1.com/foo/"},
       true},
      // Check hashes.
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321cba'",
       {"http://example1.com/foo/page.html 'strict-dynamic'",
        "https://example1.com/foo/ 'sha512-321cba'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321cba'",
       {"http://some-other.com/ 'strict-dynamic' 'sha512-321cba'",
        "http://example1.com/foo/ 'unsafe-inline' 'sha512-321cba'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321cba'",
       {"http://example1.com/foo/ 'sha512-321abc' 'sha512-321cba'",
        "http://example1.com/foo/ 'sha512-321abc' 'sha512-321cba'"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321cba'",
       {"http://example1.com/foo/ 'unsafe-inline'",
        "http://example1.com/foo/ 'sha512-321cba'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321abc'",
       {"http://example1.com/foo/ 'unsafe-inline' 'sha512-321abc'",
        "http://example1.com/foo/ 'sha512-321abc'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321abc'",
       {"'unsafe-inline' 'sha512-321abc'",
        "http://example1.com/foo/ 'sha512-321abc'"},
       true},
      // Nonces and hashes together.
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321abc' "
       "'nonce-abc'",
       {"'unsafe-inline' 'sha512-321abc' 'self'",
        "'unsafe-inline''sha512-321abc' https://example.test/"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321abc' "
       "'nonce-abc'",
       {"'unsafe-inline' 'sha512-321abc' 'self' 'nonce-abc'",
        "'sha512-321abc' https://example.test/"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321abc' "
       "'nonce-abc'",
       {"'unsafe-inline' 'sha512-321abc' 'self'",
        " 'sha512-321abc' https://example.test/ 'nonce-abc'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321abc' "
       "'nonce-abc'",
       {"'unsafe-inline' 'sha512-321abc' 'self' 'nonce-xyz'",
        "unsafe-inline' 'sha512-321abc' https://example.test/ 'nonce-xyz'"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321abc' "
       "'nonce-abc'",
       {"'unsafe-inline' 'sha512-321abc' 'self' 'sha512-xyz'",
        "unsafe-inline' 'sha512-321abc' https://example.test/ 'sha512-xyz'"},
       false},

  };

  for (const auto& test : cases) {
    SourceListDirective A(test.isScriptSrc ? "script-src" : "style-src",
                          test.sourcesA, csp.get());
    ContentSecurityPolicy* cspB =
        SetUpWithOrigin("https://another.test/image.png");

    HeapVector<Member<SourceListDirective>> vectorB;
    for (const auto& sources : test.sourcesB) {
      SourceListDirective* member = new SourceListDirective(
          test.isScriptSrc ? "script-src" : "style-src", sources, cspB);
      vectorB.push_back(member);
    }

    EXPECT_EQ(A.subsumes(vectorB), test.expected);
  }
}

TEST_F(SourceListDirectiveTest, SubsumesStrictDynamic) {
  struct TestCase {
    bool isScriptSrc;
    String sourcesA;
    std::vector<String> sourcesB;
    bool expected;
  } cases[] = {
      // Neither A nor effective policy of list B has `strict-dynamic`.
      {false,
       "http://example1.com/foo/ 'self' 'nonce-yay' 'strict-dynamic'",
       {"'strict-dynamic' 'nonce-yay'", "'nonce-yay' 'strict-dynamic'"},
       true},
      {false,
       "http://example1.com/foo/ 'self' 'nonce-yay' 'strict-dynamic'",
       {"'strict-dynamic' 'nonce-yay'", "'nonce-abc' 'strict-dynamic'"},
       true},
      {false,
       "http://example1.com/foo/ 'self' 'sha512-321abc'",
       {"'strict-dynamic' 'nonce-yay' 'sha512-321abc'",
        "'sha512-321abc' 'strict-dynamic'"},
       true},
      {false,
       "http://example1.com/foo/ 'self' 'sha512-321abc'",
       {"'strict-dynamic' 'nonce-yay' 'sha512-321abc'",
        "'sha512-321abc' 'strict-dynamic'", "'strict-dynamic'"},
       true},
      {false,
       "http://example1.com/foo/ 'self' 'sha512-321abc'",
       {"'strict-dynamic' 'nonce-yay' http://example1.com/",
        "http://example1.com/ 'strict-dynamic'"},
       false},
      // A has `strict-dynamic`, effective policy of list B does not.
      {true,
       "http://example1.com/foo/ 'self' 'nonce-yay' 'strict-dynamic'",
       {"'strict-dynamic' 'nonce-yay'", "'nonce-yay'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'sha512-321abc' 'strict-dynamic'",
       {"'strict-dynamic' 'sha512-321abc'", "'unsafe-inline' 'sha512-321abc'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'sha512-321abc' 'strict-dynamic'",
       {"'sha512-321abc'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'sha512-321abc' 'strict-dynamic'",
       {"http://example1.com/foo/ 'sha512-321abc'"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'sha512-321abc' 'strict-dynamic'",
       {"'self' 'sha512-321abc'"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic'",
       {"'strict-dynamic' 'nonce-yay'", "'nonce-yay'"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic'",
       {"http://example1.com/ 'sha512-321abc'",
        "http://example1.com/ 'sha512-321abc'"},
       false},
      {true,
       "http://example1.com/foo/ 'sha512-321abc' 'strict-dynamic'",
       {"https://example1.com/foo/ 'sha512-321abc'",
        "http://example1.com/foo/ 'sha512-321abc'"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'nonce-yay' 'strict-dynamic'",
       {"'strict-dynamic' 'nonce-yay'", "'nonce-yay'", "'sha512-321abc'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-hashed-attributes' "
       "'strict-dynamic'",
       {"'strict-dynamic' 'unsafe-hashed-attributes'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'nonce-yay' 'strict-dynamic'",
       {"'strict-dynamic' 'nonce-yay' 'unsafe-hashed-attributes'"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-eval' 'strict-dynamic'",
       {"'strict-dynamic' 'unsafe-eval'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'nonce-yay' 'strict-dynamic'",
       {"'strict-dynamic' 'nonce-yay' 'unsafe-eval'"},
       false},
      // A does not have `strict-dynamic`, but effective policy of list B does.
      // Note that any subsumption in this set-up should be `false`.
      {true,
       "http://example1.com/foo/ 'self' 'nonce-yay'",
       {"'strict-dynamic' 'nonce-yay'", "'sha512-321abc' 'strict-dynamic'"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'sha512-321abc'",
       {"'strict-dynamic' 'sha512-321abc'", "'strict-dynamic' 'sha512-321abc'"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline'",
       {"'strict-dynamic'"},
       false},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'sha512-321abc'",
       {"'strict-dynamic'"},
       false},
      // Both A and effective policy of list B has `strict-dynamic`.
      {true,
       "'strict-dynamic'",
       {"'strict-dynamic'", "'strict-dynamic'", "'strict-dynamic'"},
       true},
      {true,
       "'strict-dynamic'",
       {"'strict-dynamic'", "'strict-dynamic' 'nonce-yay'",
        "'strict-dynamic' 'nonce-yay'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic'",
       {"'strict-dynamic' 'nonce-yay'", "'strict-dynamic'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic'",
       {"'strict-dynamic' http://another.com/",
        "http://another.com/ 'strict-dynamic'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'unsafe-inline' 'strict-dynamic'",
       {"'self' 'sha512-321abc' 'strict-dynamic'",
        "'self' 'strict-dynamic' 'sha512-321abc'"},
       false},
      {true,
       "http://example1.com/foo/ 'sha512-321abc' 'strict-dynamic'",
       {"'self' 'sha512-321abc' 'strict-dynamic'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'sha512-321abc' 'strict-dynamic'",
       {"'unsafe-inline' 'strict-dynamic'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'sha512-321abc' 'strict-dynamic'",
       {"'unsafe-inline' 'sha512-123xyz' 'strict-dynamic'"},
       false},
      {true,
       "'unsafe-eval' 'self' 'sha512-321abc' 'strict-dynamic'",
       {"'unsafe-eval' 'strict-dynamic'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'sha512-321abc' 'strict-dynamic'",
       {"'unsafe-eval' 'strict-dynamic'"},
       false},
      {true,
       "'unsafe-hashed-attributes' 'self' 'sha512-321abc' 'strict-dynamic'",
       {"'unsafe-hashed-attributes' 'strict-dynamic'"},
       true},
      {true,
       "http://example1.com/foo/ 'self' 'sha512-321abc' 'strict-dynamic'",
       {"'unsafe-hashed-attributes' 'strict-dynamic'"},
       false},
  };

  for (const auto& test : cases) {
    SourceListDirective A(test.isScriptSrc ? "script-src" : "style-src",
                          test.sourcesA, csp.get());
    ContentSecurityPolicy* cspB =
        SetUpWithOrigin("https://another.test/image.png");

    HeapVector<Member<SourceListDirective>> vectorB;
    for (const auto& sources : test.sourcesB) {
      SourceListDirective* member = new SourceListDirective(
          test.isScriptSrc ? "script-src" : "style-src", sources, cspB);
      vectorB.push_back(member);
    }

    EXPECT_EQ(A.subsumes(vectorB), test.expected);
  }
}

TEST_F(SourceListDirectiveTest, SubsumesListWildcard) {
  struct TestCase {
    const char* sourcesA;
    std::vector<const char*> sourcesB;
    bool expected;
  } cases[] = {
      // `A` subsumes `policiesB`..
      {"*", {""}, true},
      {"*", {"'none'"}, true},
      {"*", {"*"}, true},
      {"*", {"*", "*", "*"}, true},
      {"*", {"*", "* https: http: ftp: ws: wss:"}, true},
      {"*", {"*", "https: http: ftp: ws: wss:"}, true},
      {"https: http: ftp: ws: wss:", {"*", "https: http: ftp: ws: wss:"}, true},
      {"http: ftp: ws:", {"*", "https: http: ftp: ws: wss:"}, true},
      {"http: ftp: ws:", {"*", "https: 'strict-dynamic'"}, true},
      {"http://another.test", {"*", "'self'"}, true},
      {"http://another.test/", {"*", "'self'"}, true},
      {"http://another.test", {"https:", "'self'"}, true},
      {"'self'", {"*", "'self'"}, true},
      {"'unsafe-eval' * ", {"'unsafe-eval'"}, true},
      {"'unsafe-hashed-attributes' * ", {"'unsafe-hashed-attributes'"}, true},
      {"'unsafe-inline' * ", {"'unsafe-inline'"}, true},
      {"*", {"*", "http://a.com ws://b.com ftp://c.com"}, true},
      {"*", {"* data: blob:", "http://a.com ws://b.com ftp://c.com"}, true},
      {"*", {"data: blob:", "http://a.com ws://b.com ftp://c.com"}, true},
      {"*", {"*", "data://a.com ws://b.com ftp://c.com"}, true},
      {"* data:",
       {"data: blob: *", "data://a.com ws://b.com ftp://c.com"},
       true},
      {"http://a.com ws://b.com ftp://c.com",
       {"*", "http://a.com ws://b.com ftp://c.com"},
       true},
      // `A` does not subsume `policiesB`..
      {"*", std::vector<const char*>(), false},
      {"", {"*"}, false},
      {"'none'", {"*"}, false},
      {"*", {"data:"}, false},
      {"*", {"blob:"}, false},
      {"http: ftp: ws:",
       {"* 'strict-dynamic'", "https: 'strict-dynamic'"},
       false},
      {"https://another.test", {"*"}, false},
      {"*", {"* 'unsafe-eval'"}, false},
      {"*", {"* 'unsafe-hashed-attributes'"}, false},
      {"*", {"* 'unsafe-inline'"}, false},
      {"'unsafe-eval'", {"* 'unsafe-eval'"}, false},
      {"'unsafe-hashed-attributes'", {"* 'unsafe-hashed-attributes'"}, false},
      {"'unsafe-inline'", {"* 'unsafe-inline'"}, false},
      {"*", {"data: blob:", "data://a.com ws://b.com ftp://c.com"}, false},
      {"* data:",
       {"data: blob:", "blob://a.com ws://b.com ftp://c.com"},
       false},
  };

  for (const auto& test : cases) {
    SourceListDirective A("script-src", test.sourcesA, csp.get());
    ContentSecurityPolicy* cspB =
        SetUpWithOrigin("https://another.test/image.png");

    HeapVector<Member<SourceListDirective>> vectorB;
    for (const auto& sources : test.sourcesB) {
      SourceListDirective* member =
          new SourceListDirective("script-src", sources, cspB);
      vectorB.push_back(member);
    }

    EXPECT_EQ(A.subsumes(vectorB), test.expected);
  }
}

TEST_F(SourceListDirectiveTest, GetSources) {
  struct TestCase {
    const char* sources;
    const char* expected;
  } cases[] = {
      {"", ""},
      {"*", "ftp: ws: http: https:"},
      {"* data:", "data: ftp: ws: http: https:"},
      {"blob: *", "blob: ftp: ws: http: https:"},
      {"* 'self'", "ftp: ws: http: https:"},
      {"https: 'self'", "https: https://example.test"},
      {"https://b.com/bar/", "https://b.com/bar/"},
      {"'self' http://a.com/foo/ https://b.com/bar/",
       "http://a.com/foo/ https://b.com/bar/ https://example.test"},
      {"http://a.com/foo/ https://b.com/bar/ 'self'",
       "http://a.com/foo/ https://b.com/bar/ https://example.test"},
  };

  for (const auto& test : cases) {
    SourceListDirective list("script-src", test.sources, csp.get());
    HeapVector<Member<CSPSource>> normalized =
        list.getSources(csp.get()->getSelfSource());

    SourceListDirective expectedList("script-src", test.expected, csp.get());
    HeapVector<Member<CSPSource>> expected = expectedList.m_list;
    EXPECT_EQ(normalized.size(), expected.size());
    for (size_t i = 0; i < expected.size(); i++) {
      Source a = {expected[i]->m_scheme,       expected[i]->m_host,
                  expected[i]->m_port,         expected[i]->m_path,
                  expected[i]->m_hostWildcard, expected[i]->m_portWildcard};
      Source b = {normalized[i]->m_scheme,       normalized[i]->m_host,
                  normalized[i]->m_port,         normalized[i]->m_path,
                  normalized[i]->m_hostWildcard, normalized[i]->m_portWildcard};
      EXPECT_TRUE(equalSources(a, b));
    }
  }
}

TEST_F(SourceListDirectiveTest, ParseHost) {
  struct TestCase {
    String sources;
    bool expected;
  } cases[] = {
      // Wildcard.
      {"*", true},
      {"*.", false},
      {"*.a", true},
      {"a.*.a", false},
      {"a.*", false},

      // Dots.
      {"a.b.c", true},
      {"a.b.", false},
      {".b.c", false},
      {"a..c", false},

      // Valid/Invalid characters.
      {"az09-", true},
      {"+", false},
  };

  for (const auto& test : cases) {
    String host;
    CSPSource::WildcardDisposition disposition = CSPSource::NoWildcard;
    Vector<UChar> characters;
    test.sources.appendTo(characters);
    const UChar* start = characters.data();
    const UChar* end = start + characters.size();
    EXPECT_EQ(test.expected,
              SourceListDirective::parseHost(start, end, host, disposition))
        << "SourceListDirective::parseHost fail to parse: " << test.sources;
  }
}

TEST_F(SourceListDirectiveTest, AllowHostWildcard) {
  KURL base;
  // When the host-part is "*", the port must still be checked.
  // See crbug.com/682673.
  {
    String sources = "http://*:111";
    SourceListDirective sourceList("default-src", sources, csp.get());
    EXPECT_TRUE(sourceList.allows(KURL(base, "http://a.com:111")));
    EXPECT_FALSE(sourceList.allows(KURL(base, "http://a.com:222")));
  }
  // When the host-part is "*", the path must still be checked.
  // See crbug.com/682673.
  {
    String sources = "http://*/welcome.html";
    SourceListDirective sourceList("default-src", sources, csp.get());
    EXPECT_TRUE(sourceList.allows(KURL(base, "http://a.com/welcome.html")));
    EXPECT_FALSE(sourceList.allows(KURL(base, "http://a.com/passwords.txt")));
  }
  // When the host-part is "*" and the expression-source is not "*", then every
  // host are allowed. See crbug.com/682673.
  {
    String sources = "http://*";
    SourceListDirective sourceList("default-src", sources, csp.get());
    EXPECT_TRUE(sourceList.allows(KURL(base, "http://a.com")));
  }
}

}  // namespace blink

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/CSPSource.h"

#include "core/dom/Document.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class CSPSourceTest : public ::testing::Test {
 public:
  CSPSourceTest() : csp(ContentSecurityPolicy::create()) {}

 protected:
  Persistent<ContentSecurityPolicy> csp;
  struct Source {
    String scheme;
    String host;
    String path;
    // port is 0 if it was not specified so the default port for a given scheme
    // will be used.
    const int port;
    CSPSource::WildcardDisposition hostWildcard;
    CSPSource::WildcardDisposition portWildcard;
  };

  bool equalSources(const Source& a, const Source& b) {
    return a.scheme == b.scheme && a.host == b.host && a.port == b.port &&
           a.path == b.path && a.hostWildcard == b.hostWildcard &&
           a.portWildcard == b.portWildcard;
  }
};

TEST_F(CSPSourceTest, BasicMatching) {
  KURL base;
  CSPSource source(csp.get(), "http", "example.com", 8000, "/foo/",
                   CSPSource::NoWildcard, CSPSource::NoWildcard);

  EXPECT_TRUE(source.matches(KURL(base, "http://example.com:8000/foo/")));
  EXPECT_TRUE(source.matches(KURL(base, "http://example.com:8000/foo/bar")));
  EXPECT_TRUE(source.matches(KURL(base, "HTTP://EXAMPLE.com:8000/foo/BAR")));

  EXPECT_FALSE(source.matches(KURL(base, "http://example.com:8000/bar/")));
  EXPECT_FALSE(source.matches(KURL(base, "https://example.com:8000/bar/")));
  EXPECT_FALSE(source.matches(KURL(base, "http://example.com:9000/bar/")));
  EXPECT_FALSE(source.matches(KURL(base, "HTTP://example.com:8000/FOO/bar")));
  EXPECT_FALSE(source.matches(KURL(base, "HTTP://example.com:8000/FOO/BAR")));
}

TEST_F(CSPSourceTest, BasicPathMatching) {
  KURL base;
  CSPSource A(csp.get(), "http", "example.com", 8000, "/",
              CSPSource::NoWildcard, CSPSource::NoWildcard);

  EXPECT_TRUE(A.matches(KURL(base, "http://example.com:8000")));
  EXPECT_TRUE(A.matches(KURL(base, "http://example.com:8000/")));
  EXPECT_TRUE(A.matches(KURL(base, "http://example.com:8000/foo/bar")));

  EXPECT_FALSE(A.matches(KURL(base, "http://example.com:8000path")));
  EXPECT_FALSE(A.matches(KURL(base, "http://example.com:9000/")));

  CSPSource B(csp.get(), "http", "example.com", 8000, "", CSPSource::NoWildcard,
              CSPSource::NoWildcard);
  EXPECT_TRUE(B.matches(KURL(base, "http://example.com:8000")));
  EXPECT_TRUE(B.matches(KURL(base, "http://example.com:8000/")));
  EXPECT_TRUE(A.matches(KURL(base, "http://example.com:8000/foo/bar")));

  EXPECT_FALSE(B.matches(KURL(base, "http://example.com:8000path")));
  EXPECT_FALSE(B.matches(KURL(base, "http://example.com:9000/")));
}

TEST_F(CSPSourceTest, WildcardMatching) {
  KURL base;
  CSPSource source(csp.get(), "http", "example.com", 0, "/",
                   CSPSource::HasWildcard, CSPSource::HasWildcard);

  EXPECT_TRUE(source.matches(KURL(base, "http://foo.example.com:8000/")));
  EXPECT_TRUE(source.matches(KURL(base, "http://foo.example.com:8000/foo")));
  EXPECT_TRUE(source.matches(KURL(base, "http://foo.example.com:9000/foo/")));
  EXPECT_TRUE(
      source.matches(KURL(base, "HTTP://FOO.EXAMPLE.com:8000/foo/BAR")));

  EXPECT_FALSE(source.matches(KURL(base, "http://example.com:8000/")));
  EXPECT_FALSE(source.matches(KURL(base, "http://example.com:8000/foo")));
  EXPECT_FALSE(source.matches(KURL(base, "http://example.com:9000/foo/")));
  EXPECT_FALSE(source.matches(KURL(base, "http://example.foo.com:8000/")));
  EXPECT_FALSE(source.matches(KURL(base, "https://example.foo.com:8000/")));
  EXPECT_FALSE(source.matches(KURL(base, "https://example.com:8000/bar/")));
}

TEST_F(CSPSourceTest, RedirectMatching) {
  KURL base;
  CSPSource source(csp.get(), "http", "example.com", 8000, "/bar/",
                   CSPSource::NoWildcard, CSPSource::NoWildcard);

  EXPECT_TRUE(
      source.matches(KURL(base, "http://example.com:8000/"),
                     ResourceRequest::RedirectStatus::FollowedRedirect));
  EXPECT_TRUE(
      source.matches(KURL(base, "http://example.com:8000/foo"),
                     ResourceRequest::RedirectStatus::FollowedRedirect));
  EXPECT_TRUE(
      source.matches(KURL(base, "https://example.com:8000/foo"),
                     ResourceRequest::RedirectStatus::FollowedRedirect));

  EXPECT_FALSE(
      source.matches(KURL(base, "http://not-example.com:8000/foo"),
                     ResourceRequest::RedirectStatus::FollowedRedirect));
  EXPECT_FALSE(source.matches(KURL(base, "http://example.com:9000/foo/"),
                              ResourceRequest::RedirectStatus::NoRedirect));
}

TEST_F(CSPSourceTest, InsecureSchemeMatchesSecureScheme) {
  KURL base;
  CSPSource source(csp.get(), "http", "", 0, "/", CSPSource::NoWildcard,
                   CSPSource::HasWildcard);

  EXPECT_TRUE(source.matches(KURL(base, "http://example.com:8000/")));
  EXPECT_TRUE(source.matches(KURL(base, "https://example.com:8000/")));
  EXPECT_TRUE(source.matches(KURL(base, "http://not-example.com:8000/")));
  EXPECT_TRUE(source.matches(KURL(base, "https://not-example.com:8000/")));
  EXPECT_FALSE(source.matches(KURL(base, "ftp://example.com:8000/")));
}

TEST_F(CSPSourceTest, InsecureHostSchemeMatchesSecureScheme) {
  KURL base;
  CSPSource source(csp.get(), "http", "example.com", 0, "/",
                   CSPSource::NoWildcard, CSPSource::HasWildcard);

  EXPECT_TRUE(source.matches(KURL(base, "http://example.com:8000/")));
  EXPECT_FALSE(source.matches(KURL(base, "http://not-example.com:8000/")));
  EXPECT_TRUE(source.matches(KURL(base, "https://example.com:8000/")));
  EXPECT_FALSE(source.matches(KURL(base, "https://not-example.com:8000/")));
}

TEST_F(CSPSourceTest, SchemeIsEmpty) {
  KURL base;

  // Self scheme is http.
  {
    Persistent<ContentSecurityPolicy> csp(ContentSecurityPolicy::create());
    csp->setupSelf(*SecurityOrigin::createFromString("http://a.com/"));
    CSPSource source(csp.get(), "", "a.com", 0, "/", CSPSource::NoWildcard,
                     CSPSource::NoWildcard);
    EXPECT_TRUE(source.matches(KURL(base, "http://a.com")));
    EXPECT_TRUE(source.matches(KURL(base, "https://a.com")));
    EXPECT_TRUE(source.matches(KURL(base, "http-so://a.com")));
    EXPECT_TRUE(source.matches(KURL(base, "https-so://a.com")));
    EXPECT_FALSE(source.matches(KURL(base, "ftp://a.com")));
  }

  // Self scheme is https.
  {
    Persistent<ContentSecurityPolicy> csp(ContentSecurityPolicy::create());
    csp->setupSelf(*SecurityOrigin::createFromString("https://a.com/"));
    CSPSource source(csp.get(), "", "a.com", 0, "/", CSPSource::NoWildcard,
                     CSPSource::NoWildcard);
    EXPECT_FALSE(source.matches(KURL(base, "http://a.com")));
    EXPECT_TRUE(source.matches(KURL(base, "https://a.com")));
    EXPECT_FALSE(source.matches(KURL(base, "http-so://a.com")));
    // TODO(mkwst, arthursonzogni): Maybe it should return true.
    // See http://crbug.com/692442
    EXPECT_FALSE(source.matches(KURL(base, "https-so://a.com")));
    EXPECT_FALSE(source.matches(KURL(base, "ftp://a.com")));
  }

  // Self scheme is not in the http familly.
  {
    Persistent<ContentSecurityPolicy> csp(ContentSecurityPolicy::create());
    csp->setupSelf(*SecurityOrigin::createFromString("ftp://a.com/"));
    CSPSource source(csp.get(), "", "a.com", 0, "/", CSPSource::NoWildcard,
                     CSPSource::NoWildcard);
    EXPECT_FALSE(source.matches(KURL(base, "http://a.com")));
    EXPECT_TRUE(source.matches(KURL(base, "ftp://a.com")));
  }

  // Self scheme is unique
  {
    Persistent<ContentSecurityPolicy> csp(ContentSecurityPolicy::create());
    csp->setupSelf(
        *SecurityOrigin::createFromString("non-standard-scheme://a.com/"));
    CSPSource source(csp.get(), "", "a.com", 0, "/", CSPSource::NoWildcard,
                     CSPSource::NoWildcard);
    EXPECT_FALSE(source.matches(KURL(base, "http://a.com")));

    // The reason matching fails is because the host is parsed as "" when
    // using a non standard scheme even though it should be parsed as "a.com"
    // After adding it to the list of standard schemes it now gets parsed
    // correctly. This does not matter in practice though because there is
    // no way to render/load anything like "non-standard-scheme://a.com"
    EXPECT_FALSE(source.matches(KURL(base, "non-standard-scheme://a.com")));
  }
}

TEST_F(CSPSourceTest, InsecureHostSchemePortMatchesSecurePort) {
  KURL base;

  // source scheme is "http"
  {
    CSPSource source(csp.get(), "http", "example.com", 80, "/",
                     CSPSource::NoWildcard, CSPSource::NoWildcard);
    EXPECT_TRUE(source.matches(KURL(base, "http://example.com/")));
    EXPECT_TRUE(source.matches(KURL(base, "http://example.com:80/")));
    // TODO(mkwst, arthursonzogni): It is weird to upgrade the port without the
    // sheme. See http://crbug.com/692499
    EXPECT_TRUE(source.matches(KURL(base, "http://example.com:443/")));
    EXPECT_TRUE(source.matches(KURL(base, "https://example.com/")));
    // TODO(mkwst, arthursonzogni): It is weird to upgrade the scheme without
    // the port. See http://crbug.com/692499
    EXPECT_TRUE(source.matches(KURL(base, "https://example.com:80/")));
    EXPECT_TRUE(source.matches(KURL(base, "https://example.com:443/")));

    EXPECT_FALSE(source.matches(KURL(base, "http://example.com:8443/")));
    EXPECT_FALSE(source.matches(KURL(base, "https://example.com:8443/")));

    EXPECT_FALSE(source.matches(KURL(base, "http://not-example.com/")));
    EXPECT_FALSE(source.matches(KURL(base, "http://not-example.com:80/")));
    EXPECT_FALSE(source.matches(KURL(base, "http://not-example.com:443/")));
    EXPECT_FALSE(source.matches(KURL(base, "https://not-example.com/")));
    EXPECT_FALSE(source.matches(KURL(base, "https://not-example.com:80/")));
    EXPECT_FALSE(source.matches(KURL(base, "https://not-example.com:443/")));
  }

  // source scheme is empty
  {
    Persistent<ContentSecurityPolicy> csp(ContentSecurityPolicy::create());
    csp->setupSelf(*SecurityOrigin::createFromString("http://example.com"));
    CSPSource source(csp.get(), "", "example.com", 80, "/",
                     CSPSource::NoWildcard, CSPSource::NoWildcard);
    EXPECT_TRUE(source.matches(KURL(base, "http://example.com/")));
    EXPECT_TRUE(source.matches(KURL(base, "https://example.com:443")));
    // TODO(mkwst, arthursonzogni): It is weird to upgrade the port without the
    // sheme. See http://crbug.com/692499
    EXPECT_TRUE(source.matches(KURL(base, "http://example.com:443")));
  }
}

TEST_F(CSPSourceTest, HostMatches) {
  KURL base;
  Persistent<ContentSecurityPolicy> csp(ContentSecurityPolicy::create());
  csp->setupSelf(*SecurityOrigin::createFromString("http://a.com"));

  // Host is * (source-expression = "http://*")
  {
    CSPSource source(csp.get(), "http", "", 0, "", CSPSource::HasWildcard,
                     CSPSource::NoWildcard);
    EXPECT_TRUE(source.matches(KURL(base, "http://a.com")));
    EXPECT_TRUE(source.matches(KURL(base, "http://.")));
  }

  // Host is *.foo.bar
  {
    CSPSource source(csp.get(), "", "foo.bar", 0, "", CSPSource::HasWildcard,
                     CSPSource::NoWildcard);
    EXPECT_FALSE(source.matches(KURL(base, "http://a.com")));
    EXPECT_FALSE(source.matches(KURL(base, "http://bar")));
    EXPECT_FALSE(source.matches(KURL(base, "http://foo.bar")));
    EXPECT_FALSE(source.matches(KURL(base, "http://o.bar")));
    EXPECT_TRUE(source.matches(KURL(base, "http://*.foo.bar")));
    EXPECT_TRUE(source.matches(KURL(base, "http://sub.foo.bar")));
    EXPECT_TRUE(source.matches(KURL(base, "http://sub.sub.foo.bar")));
    // Please see http://crbug.com/692505
    EXPECT_TRUE(source.matches(KURL(base, "http://.foo.bar")));
  }

  // Host is exact.
  {
    CSPSource source(csp.get(), "", "foo.bar", 0, "", CSPSource::NoWildcard,
                     CSPSource::NoWildcard);
    EXPECT_TRUE(source.matches(KURL(base, "http://foo.bar")));
    EXPECT_FALSE(source.matches(KURL(base, "http://sub.foo.bar")));
    EXPECT_FALSE(source.matches(KURL(base, "http://bar")));
    // Please see http://crbug.com/692505
    EXPECT_FALSE(source.matches(KURL(base, "http://.foo.bar")));
  }
}

TEST_F(CSPSourceTest, DoesNotSubsume) {
  struct Source {
    const char* scheme;
    const char* host;
    const char* path;
    const int port;
  };
  struct TestCase {
    const Source a;
    const Source b;
  } cases[] = {
      {{"http", "example.com", "/", 0}, {"http", "another.com", "/", 0}},
      {{"wss", "example.com", "/", 0}, {"http", "example.com", "/", 0}},
      {{"wss", "example.com", "/", 0}, {"about", "example.com", "/", 0}},
      {{"http", "example.com", "/", 0}, {"about", "example.com", "/", 0}},
      {{"http", "example.com", "/1.html", 0},
       {"http", "example.com", "/2.html", 0}},
      {{"http", "example.com", "/", 443}, {"about", "example.com", "/", 800}},
  };
  for (const auto& test : cases) {
    CSPSource* returned = new CSPSource(
        csp.get(), test.a.scheme, test.a.host, test.a.port, test.a.path,
        CSPSource::NoWildcard, CSPSource::NoWildcard);

    CSPSource* required = new CSPSource(
        csp.get(), test.b.scheme, test.b.host, test.b.port, test.b.path,
        CSPSource::NoWildcard, CSPSource::NoWildcard);

    EXPECT_FALSE(required->subsumes(returned));
    // Verify the same test with a and b swapped.
    EXPECT_FALSE(required->subsumes(returned));
  }
}

TEST_F(CSPSourceTest, Subsumes) {
  struct Source {
    const char* scheme;
    const char* path;
    const int port;
  };
  struct TestCase {
    const Source a;
    const Source b;
    bool expected;
    bool expectedWhenSwapped;
  } cases[] = {
      // Equal signals
      {{"http", "/", 0}, {"http", "/", 0}, true, true},
      {{"https", "/", 0}, {"https", "/", 0}, true, true},
      {{"https", "/page1.html", 0}, {"https", "/page1.html", 0}, true, true},
      {{"http", "/", 70}, {"http", "/", 70}, true, true},
      {{"https", "/", 70}, {"https", "/", 70}, true, true},
      {{"https", "/page1.html", 0}, {"https", "/page1.html", 0}, true, true},
      {{"http", "/page1.html", 70}, {"http", "/page1.html", 70}, true, true},
      {{"https", "/page1.html", 70}, {"https", "/page1.html", 70}, true, true},
      {{"http", "/", 0}, {"http", "", 0}, true, true},
      {{"http", "/", 80}, {"http", "", 80}, true, true},
      {{"http", "/", 80}, {"https", "", 443}, false, true},
      // One stronger signal in the first CSPSource
      {{"https", "/", 0}, {"http", "/", 0}, true, false},
      {{"http", "/page1.html", 0}, {"http", "/", 0}, true, false},
      {{"http", "/", 80}, {"http", "/", 0}, true, true},
      {{"http", "/", 700}, {"http", "/", 0}, false, false},
      // Two stronger signals in the first CSPSource
      {{"https", "/page1.html", 0}, {"http", "/", 0}, true, false},
      {{"https", "/", 80}, {"http", "/", 0}, false, false},
      {{"http", "/page1.html", 80}, {"http", "/", 0}, true, false},
      // Three stronger signals in the first CSPSource
      {{"https", "/page1.html", 70}, {"http", "/", 0}, false, false},
      // Mixed signals
      {{"https", "/", 0}, {"http", "/page1.html", 0}, false, false},
      {{"https", "/", 0}, {"http", "/", 70}, false, false},
      {{"http", "/page1.html", 0}, {"http", "/", 70}, false, false},
  };

  for (const auto& test : cases) {
    CSPSource* returned = new CSPSource(
        csp.get(), test.a.scheme, "example.com", test.a.port, test.a.path,
        CSPSource::NoWildcard, CSPSource::NoWildcard);

    CSPSource* required = new CSPSource(
        csp.get(), test.b.scheme, "example.com", test.b.port, test.b.path,
        CSPSource::NoWildcard, CSPSource::NoWildcard);

    EXPECT_EQ(required->subsumes(returned), test.expected);
    // Verify the same test with a and b swapped.
    EXPECT_EQ(returned->subsumes(required), test.expectedWhenSwapped);
  }

  // When returned CSP has a wildcard but the required csp doesn't, then it is
  // not subsumed.
  for (const auto& test : cases) {
    CSPSource* returned = new CSPSource(
        csp.get(), test.a.scheme, "example.com", test.a.port, test.a.path,
        CSPSource::HasWildcard, CSPSource::NoWildcard);
    CSPSource* required = new CSPSource(
        csp.get(), test.b.scheme, "example.com", test.b.port, test.b.path,
        CSPSource::NoWildcard, CSPSource::NoWildcard);

    EXPECT_FALSE(required->subsumes(returned));

    // If required csp also allows a wildcard in host, then the answer should be
    // as expected.
    CSPSource* required2 = new CSPSource(
        csp.get(), test.b.scheme, "example.com", test.b.port, test.b.path,
        CSPSource::HasWildcard, CSPSource::NoWildcard);
    EXPECT_EQ(required2->subsumes(returned), test.expected);
  }
}

TEST_F(CSPSourceTest, WildcardsSubsumes) {
  struct Wildcards {
    CSPSource::WildcardDisposition hostDispotion;
    CSPSource::WildcardDisposition portDispotion;
  };
  struct TestCase {
    const Wildcards a;
    const Wildcards b;
    bool expected;
  } cases[] = {
      // One out of four possible wildcards.
      {{CSPSource::HasWildcard, CSPSource::NoWildcard},
       {CSPSource::NoWildcard, CSPSource::NoWildcard},
       false},
      {{CSPSource::NoWildcard, CSPSource::HasWildcard},
       {CSPSource::NoWildcard, CSPSource::NoWildcard},
       false},
      {{CSPSource::NoWildcard, CSPSource::NoWildcard},
       {CSPSource::NoWildcard, CSPSource::HasWildcard},
       true},
      {{CSPSource::NoWildcard, CSPSource::NoWildcard},
       {CSPSource::HasWildcard, CSPSource::NoWildcard},
       true},
      // Two out of four possible wildcards.
      {{CSPSource::HasWildcard, CSPSource::HasWildcard},
       {CSPSource::NoWildcard, CSPSource::NoWildcard},
       false},
      {{CSPSource::HasWildcard, CSPSource::NoWildcard},
       {CSPSource::HasWildcard, CSPSource::NoWildcard},
       true},
      {{CSPSource::HasWildcard, CSPSource::NoWildcard},
       {CSPSource::NoWildcard, CSPSource::HasWildcard},
       false},
      {{CSPSource::NoWildcard, CSPSource::HasWildcard},
       {CSPSource::HasWildcard, CSPSource::NoWildcard},
       false},
      {{CSPSource::NoWildcard, CSPSource::HasWildcard},
       {CSPSource::NoWildcard, CSPSource::HasWildcard},
       true},
      {{CSPSource::NoWildcard, CSPSource::NoWildcard},
       {CSPSource::HasWildcard, CSPSource::HasWildcard},
       true},
      // Three out of four possible wildcards.
      {{CSPSource::HasWildcard, CSPSource::HasWildcard},
       {CSPSource::HasWildcard, CSPSource::NoWildcard},
       false},
      {{CSPSource::HasWildcard, CSPSource::HasWildcard},
       {CSPSource::NoWildcard, CSPSource::HasWildcard},
       false},
      {{CSPSource::HasWildcard, CSPSource::NoWildcard},
       {CSPSource::HasWildcard, CSPSource::HasWildcard},
       true},
      {{CSPSource::NoWildcard, CSPSource::HasWildcard},
       {CSPSource::HasWildcard, CSPSource::HasWildcard},
       true},
      // Four out of four possible wildcards.
      {{CSPSource::HasWildcard, CSPSource::HasWildcard},
       {CSPSource::HasWildcard, CSPSource::HasWildcard},
       true},
  };

  // There are different cases for wildcards but now also the second CSPSource
  // has a more specific path.
  for (const auto& test : cases) {
    CSPSource* returned =
        new CSPSource(csp.get(), "http", "example.com", 0, "/",
                      test.a.hostDispotion, test.a.portDispotion);
    CSPSource* required =
        new CSPSource(csp.get(), "http", "example.com", 0, "/",
                      test.b.hostDispotion, test.b.portDispotion);
    EXPECT_EQ(required->subsumes(returned), test.expected);

    // Wildcards should not matter when required csp is stricter than returned
    // csp.
    CSPSource* required2 =
        new CSPSource(csp.get(), "https", "example.com", 0, "/",
                      test.b.hostDispotion, test.b.portDispotion);
    EXPECT_FALSE(required2->subsumes(returned));
  }
}

TEST_F(CSPSourceTest, SchemesOnlySubsumes) {
  struct TestCase {
    String aScheme;
    String bScheme;
    bool expected;
  } cases[] = {
      // HTTP
      {"http", "http", true},
      {"http", "https", false},
      {"https", "http", true},
      {"https", "https", true},
      // WSS
      {"ws", "ws", true},
      {"ws", "wss", false},
      {"wss", "ws", true},
      {"wss", "wss", true},
      // Unequal
      {"ws", "http", false},
      {"http", "ws", false},
      {"http", "about", false},
  };

  for (const auto& test : cases) {
    CSPSource* returned =
        new CSPSource(csp.get(), test.aScheme, "example.com", 0, "/",
                      CSPSource::NoWildcard, CSPSource::NoWildcard);
    CSPSource* required =
        new CSPSource(csp.get(), test.bScheme, "example.com", 0, "/",
                      CSPSource::NoWildcard, CSPSource::NoWildcard);
    EXPECT_EQ(required->subsumes(returned), test.expected);
  }
}

TEST_F(CSPSourceTest, IsSimilar) {
  struct Source {
    const char* scheme;
    const char* host;
    const char* path;
    const int port;
  };
  struct TestCase {
    const Source a;
    const Source b;
    bool isSimilar;
  } cases[] = {
      // Similar
      {{"http", "example.com", "/", 0}, {"http", "example.com", "/", 0}, true},
      // Schemes
      {{"https", "example.com", "/", 0},
       {"https", "example.com", "/", 0},
       true},
      {{"https", "example.com", "/", 0}, {"http", "example.com", "/", 0}, true},
      {{"ws", "example.com", "/", 0}, {"wss", "example.com", "/", 0}, true},
      // Ports
      {{"http", "example.com", "/", 90},
       {"http", "example.com", "/", 90},
       true},
      {{"wss", "example.com", "/", 0},
       {"wss", "example.com", "/", 0},
       true},  // use default port
      {{"http", "example.com", "/", 80}, {"http", "example.com", "/", 0}, true},
      {{"http", "example.com", "/", 80},
       {"https", "example.com", "/", 443},
       true},
      {{"http", "example.com", "/", 80},
       {"https", "example.com", "/", 444},
       false},
      // Paths
      {{"http", "example.com", "/", 0},
       {"http", "example.com", "/1.html", 0},
       true},
      {{"http", "example.com", "/", 0}, {"http", "example.com", "", 0}, true},
      {{"http", "example.com", "/", 0},
       {"http", "example.com", "/a/b/", 0},
       true},
      {{"http", "example.com", "/a/", 0},
       {"http", "example.com", "/a/", 0},
       true},
      {{"http", "example.com", "/a/", 0},
       {"http", "example.com", "/a/b/", 0},
       true},
      {{"http", "example.com", "/a/", 0},
       {"http", "example.com", "/a/b/1.html", 0},
       true},
      {{"http", "example.com", "/1.html", 0},
       {"http", "example.com", "/1.html", 0},
       true},
      // Mixed
      {{"http", "example.com", "/1.html", 90},
       {"http", "example.com", "/", 90},
       true},
      {{"https", "example.com", "/", 0}, {"http", "example.com", "/", 0}, true},
      {{"http", "example.com", "/a/", 90},
       {"https", "example.com", "", 90},
       true},
      {{"wss", "example.com", "/a/", 90},
       {"ws", "example.com", "/a/b/", 90},
       true},
      {{"https", "example.com", "/a/", 90},
       {"https", "example.com", "/a/b/", 90},
       true},
      // Not Similar
      {{"http", "example.com", "/a/", 0},
       {"https", "example.com", "", 90},
       false},
      {{"https", "example.com", "/", 0},
       {"https", "example.com", "/", 90},
       false},
      {{"http", "example.com", "/", 0}, {"http", "another.com", "/", 0}, false},
      {{"wss", "example.com", "/", 0}, {"http", "example.com", "/", 0}, false},
      {{"wss", "example.com", "/", 0}, {"about", "example.com", "/", 0}, false},
      {{"http", "example.com", "/", 0},
       {"about", "example.com", "/", 0},
       false},
      {{"http", "example.com", "/1.html", 0},
       {"http", "example.com", "/2.html", 0},
       false},
      {{"http", "example.com", "/a/1.html", 0},
       {"http", "example.com", "/a/b/", 0},
       false},
      {{"http", "example.com", "/", 443},
       {"about", "example.com", "/", 800},
       false},
  };

  for (const auto& test : cases) {
    CSPSource* returned = new CSPSource(
        csp.get(), test.a.scheme, test.a.host, test.a.port, test.a.path,
        CSPSource::NoWildcard, CSPSource::NoWildcard);

    CSPSource* required = new CSPSource(
        csp.get(), test.b.scheme, test.b.host, test.b.port, test.b.path,
        CSPSource::NoWildcard, CSPSource::NoWildcard);

    EXPECT_EQ(returned->isSimilar(required), test.isSimilar);
    // Verify the same test with a and b swapped.
    EXPECT_EQ(required->isSimilar(returned), test.isSimilar);
  }
}

TEST_F(CSPSourceTest, FirstSubsumesSecond) {
  struct Source {
    const char* scheme;
    const char* host;
    const int port;
    const char* path;
  };
  struct TestCase {
    const Source sourceB;
    String schemeA;
    bool expected;
  } cases[] = {
      // Subsumed.
      {{"http", "example.com", 0, "/"}, "http", true},
      {{"http", "example.com", 0, "/page.html"}, "http", true},
      {{"http", "second-example.com", 80, "/"}, "http", true},
      {{"https", "second-example.com", 0, "/"}, "http", true},
      {{"http", "second-example.com", 0, "/page.html"}, "http", true},
      {{"https", "second-example.com", 80, "/page.html"}, "http", true},
      {{"https", "second-example.com", 0, "/"}, "https", true},
      {{"https", "second-example.com", 0, "/page.html"}, "https", true},
      {{"http", "example.com", 900, "/"}, "http", true},
      // NOT subsumed.
      {{"http", "second-example.com", 0, "/"}, "wss", false},
      {{"http", "non-example.com", 900, "/"}, "http", false},
      {{"http", "second-example.com", 0, "/"}, "https", false},
  };

  CSPSource* noWildcards =
      new CSPSource(csp.get(), "http", "example.com", 0, "/",
                    CSPSource::NoWildcard, CSPSource::NoWildcard);
  CSPSource* hostWildcard =
      new CSPSource(csp.get(), "http", "third-example.com", 0, "/",
                    CSPSource::HasWildcard, CSPSource::NoWildcard);
  CSPSource* portWildcard =
      new CSPSource(csp.get(), "http", "third-example.com", 0, "/",
                    CSPSource::NoWildcard, CSPSource::HasWildcard);
  CSPSource* bothWildcards =
      new CSPSource(csp.get(), "http", "third-example.com", 0, "/",
                    CSPSource::HasWildcard, CSPSource::HasWildcard);
  CSPSource* httpOnly =
      new CSPSource(csp.get(), "http", "", 0, "", CSPSource::NoWildcard,
                    CSPSource::NoWildcard);
  CSPSource* httpsOnly =
      new CSPSource(csp.get(), "https", "", 0, "", CSPSource::NoWildcard,
                    CSPSource::NoWildcard);

  for (const auto& test : cases) {
    // Setup default vectors.
    HeapVector<Member<CSPSource>> listA;
    HeapVector<Member<CSPSource>> listB;
    listB.push_back(noWildcards);
    // Empty `listA` implies `none` is allowed.
    EXPECT_FALSE(CSPSource::firstSubsumesSecond(listA, listB));

    listA.push_back(noWildcards);
    // Add CSPSources based on the current test.
    listB.push_back(new CSPSource(
        csp.get(), test.sourceB.scheme, test.sourceB.host, 0, test.sourceB.path,
        CSPSource::NoWildcard, CSPSource::NoWildcard));
    listA.push_back(new CSPSource(csp.get(), test.schemeA, "second-example.com",
                                  0, "/", CSPSource::NoWildcard,
                                  CSPSource::NoWildcard));
    // listB contains: ["http://example.com/", test.listB]
    // listA contains: ["http://example.com/",
    // test.schemeA + "://second-example.com/"]
    EXPECT_EQ(test.expected, CSPSource::firstSubsumesSecond(listA, listB));

    // If we add another source to `listB` with a host wildcard,
    // then the result should definitely be false.
    listB.push_back(hostWildcard);

    // If we add another source to `listA` with a port wildcard,
    // it does not make `listB` to be subsumed under `listA`.
    listB.push_back(portWildcard);
    EXPECT_FALSE(CSPSource::firstSubsumesSecond(listA, listB));

    // If however we add another source to `listA` with both wildcards,
    // that CSPSource is subsumed, so the answer should be as expected
    // before.
    listA.push_back(bothWildcards);
    EXPECT_EQ(test.expected, CSPSource::firstSubsumesSecond(listA, listB));

    // If we add a scheme-source expression of 'https' to `listB`, then it
    // should not be subsumed.
    listB.push_back(httpsOnly);
    EXPECT_FALSE(CSPSource::firstSubsumesSecond(listA, listB));

    // If we add a scheme-source expression of 'http' to `listA`, then it should
    // subsume all current epxression in `listB`.
    listA.push_back(httpOnly);
    EXPECT_TRUE(CSPSource::firstSubsumesSecond(listA, listB));
  }
}

TEST_F(CSPSourceTest, Intersect) {
  struct TestCase {
    const Source a;
    const Source b;
    const Source normalized;
  } cases[] = {
      {{"http", "example.com", "/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard}},
      {{"ws", "example.com", "/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"wss", "example.com", "/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"wss", "example.com", "/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard}},
      // Wildcards
      {{"http", "example.com", "/", 0, CSPSource::HasWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard}},
      {{"http", "example.com", "/", 0, CSPSource::HasWildcard,
        CSPSource::HasWildcard},
       {"http", "example.com", "/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard}},
      {{"http", "example.com", "/", 0, CSPSource::HasWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/", 0, CSPSource::NoWildcard,
        CSPSource::HasWildcard},
       {"http", "example.com", "/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard}},
      // Ports
      {{"http", "example.com", "/", 80, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/", 80, CSPSource::NoWildcard,
        CSPSource::NoWildcard}},
      {{"http", "example.com", "/", 80, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"https", "example.com", "/", 443, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"https", "example.com", "/", 443, CSPSource::NoWildcard,
        CSPSource::NoWildcard}},
      {{"https", "example.com", "/", 443, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/", 80, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"https", "example.com", "/", 443, CSPSource::NoWildcard,
        CSPSource::NoWildcard}},
      // Paths
      {{"http", "example.com", "/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/1.html", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/1.html", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard}},
      {{"http", "example.com", "/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard}},
      {{"http", "example.com", "/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/a/b/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/a/b/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard}},
      {{"http", "example.com", "/a/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/a/b/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/a/b/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard}},
      {{"http", "example.com", "/a/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/a/b/1.html", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/a/b/1.html", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard}},
      // Mixed
      {{"http", "example.com", "/1.html", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/", 80, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/1.html", 80, CSPSource::NoWildcard,
        CSPSource::NoWildcard}},
  };

  for (const auto& test : cases) {
    CSPSource* A =
        new CSPSource(csp.get(), test.a.scheme, test.a.host, test.a.port,
                      test.a.path, test.a.hostWildcard, test.a.portWildcard);
    CSPSource* B =
        new CSPSource(csp.get(), test.b.scheme, test.b.host, test.b.port,
                      test.b.path, test.b.hostWildcard, test.b.portWildcard);

    CSPSource* normalized = A->intersect(B);
    Source intersectAB = {
        normalized->m_scheme,       normalized->m_host,
        normalized->m_path,         normalized->m_port,
        normalized->m_hostWildcard, normalized->m_portWildcard};
    EXPECT_TRUE(equalSources(intersectAB, test.normalized));

    // Verify the same test with A and B swapped. The result should be
    // identical.
    normalized = B->intersect(A);
    Source intersectBA = {
        normalized->m_scheme,       normalized->m_host,
        normalized->m_path,         normalized->m_port,
        normalized->m_hostWildcard, normalized->m_portWildcard};
    EXPECT_TRUE(equalSources(intersectBA, test.normalized));
  }
}

TEST_F(CSPSourceTest, IntersectSchemesOnly) {
  struct TestCase {
    const Source a;
    const Source b;
    const Source normalized;
  } cases[] = {
      // Both sources are schemes only.
      {{"http", "", "", 0, CSPSource::NoWildcard, CSPSource::NoWildcard},
       {"http", "", "", 0, CSPSource::NoWildcard, CSPSource::NoWildcard},
       {"http", "", "", 0, CSPSource::NoWildcard, CSPSource::NoWildcard}},
      {{"http", "", "", 0, CSPSource::NoWildcard, CSPSource::NoWildcard},
       {"https", "", "", 0, CSPSource::NoWildcard, CSPSource::NoWildcard},
       {"https", "", "", 0, CSPSource::NoWildcard, CSPSource::NoWildcard}},
      {{"ws", "", "", 0, CSPSource::NoWildcard, CSPSource::NoWildcard},
       {"wss", "", "", 0, CSPSource::NoWildcard, CSPSource::NoWildcard},
       {"wss", "", "", 0, CSPSource::NoWildcard, CSPSource::NoWildcard}},
      // One source is a scheme only and the other one has no wildcards.
      {{"http", "", "", 0, CSPSource::NoWildcard, CSPSource::NoWildcard},
       {"http", "example.com", "/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"http", "example.com", "/", 0, CSPSource::NoWildcard,
        CSPSource::NoWildcard}},
      {{"http", "", "", 0, CSPSource::NoWildcard, CSPSource::NoWildcard},
       {"https", "example.com", "/", 80, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"https", "example.com", "/", 80, CSPSource::NoWildcard,
        CSPSource::NoWildcard}},
      {{"https", "", "", 0, CSPSource::NoWildcard, CSPSource::NoWildcard},
       {"http", "example.com", "/page.html", 80, CSPSource::NoWildcard,
        CSPSource::NoWildcard},
       {"https", "example.com", "/page.html", 80, CSPSource::NoWildcard,
        CSPSource::NoWildcard}},
      // One source is a scheme only and the other has one or two wildcards.
      {{"https", "", "", 0, CSPSource::NoWildcard, CSPSource::NoWildcard},
       {"http", "example.com", "/page.html", 80, CSPSource::HasWildcard,
        CSPSource::NoWildcard},
       {"https", "example.com", "/page.html", 80, CSPSource::HasWildcard,
        CSPSource::NoWildcard}},
      {{"https", "", "", 0, CSPSource::NoWildcard, CSPSource::NoWildcard},
       {"http", "example.com", "/page.html", 80, CSPSource::NoWildcard,
        CSPSource::HasWildcard},
       {"https", "example.com", "/page.html", 80, CSPSource::NoWildcard,
        CSPSource::HasWildcard}},
      {{"https", "", "", 0, CSPSource::NoWildcard, CSPSource::NoWildcard},
       {"http", "example.com", "/page.html", 80, CSPSource::HasWildcard,
        CSPSource::HasWildcard},
       {"https", "example.com", "/page.html", 80, CSPSource::HasWildcard,
        CSPSource::HasWildcard}},
  };

  for (const auto& test : cases) {
    CSPSource* A =
        new CSPSource(csp.get(), test.a.scheme, test.a.host, test.a.port,
                      test.a.path, test.a.hostWildcard, test.a.portWildcard);

    CSPSource* B =
        new CSPSource(csp.get(), test.b.scheme, test.b.host, test.b.port,
                      test.b.path, test.b.hostWildcard, test.b.portWildcard);

    CSPSource* normalized = A->intersect(B);
    Source intersectAB = {
        normalized->m_scheme,       normalized->m_host,
        normalized->m_path,         normalized->m_port,
        normalized->m_hostWildcard, normalized->m_portWildcard};
    EXPECT_TRUE(equalSources(intersectAB, test.normalized));

    // Verify the same test with A and B swapped. The result should be
    // identical.
    normalized = B->intersect(A);
    Source intersectBA = {
        normalized->m_scheme,       normalized->m_host,
        normalized->m_path,         normalized->m_port,
        normalized->m_hostWildcard, normalized->m_portWildcard};
    EXPECT_TRUE(equalSources(intersectBA, test.normalized));
  }
}

}  // namespace blink

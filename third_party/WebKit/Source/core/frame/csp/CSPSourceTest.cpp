// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/CSPSource.h"

#include "core/dom/Document.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class CSPSourceTest : public ::testing::Test {
public:
    CSPSourceTest()
        : csp(ContentSecurityPolicy::create())
    {
    }

protected:
    RefPtrWillBePersistent<ContentSecurityPolicy> csp;
};

TEST_F(CSPSourceTest, BasicMatching)
{
    KURL base;
    CSPSource source(csp.get(), "http", "example.com", 8000, "/foo/", CSPSource::NoWildcard, CSPSource::NoWildcard);

    EXPECT_TRUE(source.matches(KURL(base, "http://example.com:8000/foo/")));
    EXPECT_TRUE(source.matches(KURL(base, "http://example.com:8000/foo/bar")));
    EXPECT_TRUE(source.matches(KURL(base, "HTTP://EXAMPLE.com:8000/foo/BAR")));

    EXPECT_FALSE(source.matches(KURL(base, "http://example.com:8000/bar/")));
    EXPECT_FALSE(source.matches(KURL(base, "https://example.com:8000/bar/")));
    EXPECT_FALSE(source.matches(KURL(base, "http://example.com:9000/bar/")));
}

TEST_F(CSPSourceTest, WildcardMatching)
{
    KURL base;
    CSPSource source(csp.get(), "http", "example.com", 0, "/", CSPSource::HasWildcard, CSPSource::HasWildcard);

    EXPECT_TRUE(source.matches(KURL(base, "http://foo.example.com:8000/")));
    EXPECT_TRUE(source.matches(KURL(base, "http://foo.example.com:8000/foo")));
    EXPECT_TRUE(source.matches(KURL(base, "http://foo.example.com:9000/foo/")));
    EXPECT_TRUE(source.matches(KURL(base, "HTTP://FOO.EXAMPLE.com:8000/foo/BAR")));

    EXPECT_FALSE(source.matches(KURL(base, "http://example.com:8000/")));
    EXPECT_FALSE(source.matches(KURL(base, "http://example.com:8000/foo")));
    EXPECT_FALSE(source.matches(KURL(base, "http://example.com:9000/foo/")));
    EXPECT_FALSE(source.matches(KURL(base, "http://example.foo.com:8000/")));
    EXPECT_FALSE(source.matches(KURL(base, "https://example.foo.com:8000/")));
    EXPECT_FALSE(source.matches(KURL(base, "https://example.com:8000/bar/")));
}

TEST_F(CSPSourceTest, RedirectMatching)
{
    KURL base;
    CSPSource source(csp.get(), "http", "example.com", 8000, "/bar/", CSPSource::NoWildcard, CSPSource::NoWildcard);

    EXPECT_TRUE(source.matches(KURL(base, "http://example.com:8000/"), ContentSecurityPolicy::DidRedirect));
    EXPECT_TRUE(source.matches(KURL(base, "http://example.com:8000/foo"), ContentSecurityPolicy::DidRedirect));
    EXPECT_TRUE(source.matches(KURL(base, "https://example.com:8000/foo"), ContentSecurityPolicy::DidRedirect));

    EXPECT_FALSE(source.matches(KURL(base, "http://not-example.com:8000/foo"), ContentSecurityPolicy::DidRedirect));
    EXPECT_FALSE(source.matches(KURL(base, "http://example.com:9000/foo/"), ContentSecurityPolicy::DidNotRedirect));
}

TEST_F(CSPSourceTest, InsecureSourceMatchesSecure)
{
    KURL base;
    CSPSource source(csp.get(), "http", "", 0, "/", CSPSource::NoWildcard, CSPSource::HasWildcard);

    EXPECT_TRUE(source.matches(KURL(base, "http://example.com:8000/")));
    EXPECT_TRUE(source.matches(KURL(base, "https://example.com:8000/")));
    EXPECT_TRUE(source.matches(KURL(base, "http://not-example.com:8000/")));
    EXPECT_TRUE(source.matches(KURL(base, "https://not-example.com:8000/")));
    EXPECT_FALSE(source.matches(KURL(base, "ftp://example.com:8000/")));
}

TEST_F(CSPSourceTest, InsecureHostMatchesSecure)
{
    KURL base;
    CSPSource source(csp.get(), "http", "example.com", 0, "/", CSPSource::NoWildcard, CSPSource::HasWildcard);

    EXPECT_TRUE(source.matches(KURL(base, "http://example.com:8000/")));
    EXPECT_FALSE(source.matches(KURL(base, "http://not-example.com:8000/")));
    EXPECT_TRUE(source.matches(KURL(base, "https://example.com:8000/")));
    EXPECT_FALSE(source.matches(KURL(base, "https://not-example.com:8000/")));
}

} // namespace

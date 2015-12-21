// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/ContentSecurityPolicy.h"

#include "core/dom/Document.h"
#include "core/loader/DocumentLoader.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ContentSecurityPolicyTest : public ::testing::Test {
public:
    ContentSecurityPolicyTest()
        : csp(ContentSecurityPolicy::create())
        , secureURL(ParsedURLString, "https://example.test/image.png")
        , secureOrigin(SecurityOrigin::create(secureURL))
    {
    }

protected:
    virtual void SetUp()
    {
        document = Document::create();
        document->setSecurityOrigin(secureOrigin);
    }

    RefPtrWillBePersistent<ContentSecurityPolicy> csp;
    KURL secureURL;
    RefPtr<SecurityOrigin> secureOrigin;
    RefPtrWillBePersistent<Document> document;
};

TEST_F(ContentSecurityPolicyTest, ParseUpgradeInsecureRequestsEnabled)
{
    csp->didReceiveHeader("upgrade-insecure-requests", ContentSecurityPolicyHeaderTypeEnforce, ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(SecurityContext::InsecureRequestsUpgrade, csp->insecureRequestsPolicy());

    csp->bindToExecutionContext(document.get());
    EXPECT_EQ(SecurityContext::InsecureRequestsUpgrade, document->insecureRequestsPolicy());
    EXPECT_TRUE(document->insecureNavigationsToUpgrade()->contains(secureOrigin->host().impl()->hash()));
}

TEST_F(ContentSecurityPolicyTest, ParseMonitorInsecureRequestsEnabled)
{
    csp->didReceiveHeader("upgrade-insecure-requests", ContentSecurityPolicyHeaderTypeReport, ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(SecurityContext::InsecureRequestsDoNotUpgrade, csp->insecureRequestsPolicy());

    csp->bindToExecutionContext(document.get());
    EXPECT_EQ(SecurityContext::InsecureRequestsDoNotUpgrade, document->insecureRequestsPolicy());
    EXPECT_FALSE(document->insecureNavigationsToUpgrade()->contains(secureOrigin->host().impl()->hash()));
}

TEST_F(ContentSecurityPolicyTest, CopyStateFrom)
{
    csp->didReceiveHeader("script-src 'none'; plugin-types application/x-type-1", ContentSecurityPolicyHeaderTypeEnforce, ContentSecurityPolicyHeaderSourceHTTP);
    csp->didReceiveHeader("img-src http://example.com", ContentSecurityPolicyHeaderTypeEnforce, ContentSecurityPolicyHeaderSourceHTTP);

    KURL exampleUrl(KURL(), "http://example.com");
    KURL notExampleUrl(KURL(), "http://not-example.com");

    RefPtrWillBeRawPtr<ContentSecurityPolicy> csp2 = ContentSecurityPolicy::create();
    csp2->copyStateFrom(csp.get());
    EXPECT_FALSE(csp2->allowScriptFromSource(exampleUrl, ContentSecurityPolicy::DidNotRedirect, ContentSecurityPolicy::SuppressReport));
    EXPECT_TRUE(csp2->allowPluginType("application/x-type-1", "application/x-type-1", exampleUrl, ContentSecurityPolicy::SuppressReport));
    EXPECT_TRUE(csp2->allowImageFromSource(exampleUrl, ContentSecurityPolicy::DidNotRedirect, ContentSecurityPolicy::SuppressReport));
    EXPECT_FALSE(csp2->allowImageFromSource(notExampleUrl, ContentSecurityPolicy::DidNotRedirect, ContentSecurityPolicy::SuppressReport));
    EXPECT_FALSE(csp2->allowPluginType("application/x-type-2", "application/x-type-2", exampleUrl, ContentSecurityPolicy::SuppressReport));
}

TEST_F(ContentSecurityPolicyTest, CopyPluginTypesFrom)
{
    csp->didReceiveHeader("script-src 'none'; plugin-types application/x-type-1", ContentSecurityPolicyHeaderTypeEnforce, ContentSecurityPolicyHeaderSourceHTTP);
    csp->didReceiveHeader("img-src http://example.com", ContentSecurityPolicyHeaderTypeEnforce, ContentSecurityPolicyHeaderSourceHTTP);

    KURL exampleUrl(KURL(), "http://example.com");
    KURL notExampleUrl(KURL(), "http://not-example.com");

    RefPtrWillBeRawPtr<ContentSecurityPolicy> csp2 = ContentSecurityPolicy::create();
    csp2->copyPluginTypesFrom(csp.get());
    EXPECT_TRUE(csp2->allowScriptFromSource(exampleUrl, ContentSecurityPolicy::DidNotRedirect, ContentSecurityPolicy::SuppressReport));
    EXPECT_TRUE(csp2->allowPluginType("application/x-type-1", "application/x-type-1", exampleUrl, ContentSecurityPolicy::SuppressReport));
    EXPECT_TRUE(csp2->allowImageFromSource(exampleUrl, ContentSecurityPolicy::DidNotRedirect, ContentSecurityPolicy::SuppressReport));
    EXPECT_TRUE(csp2->allowImageFromSource(notExampleUrl, ContentSecurityPolicy::DidNotRedirect, ContentSecurityPolicy::SuppressReport));
    EXPECT_FALSE(csp2->allowPluginType("application/x-type-2", "application/x-type-2", exampleUrl, ContentSecurityPolicy::SuppressReport));
}

TEST_F(ContentSecurityPolicyTest, IsFrameAncestorsEnforced)
{
    csp->didReceiveHeader("script-src 'none';", ContentSecurityPolicyHeaderTypeEnforce, ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_FALSE(csp->isFrameAncestorsEnforced());

    csp->didReceiveHeader("frame-ancestors 'self'", ContentSecurityPolicyHeaderTypeReport, ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_FALSE(csp->isFrameAncestorsEnforced());

    csp->didReceiveHeader("frame-ancestors 'self'", ContentSecurityPolicyHeaderTypeEnforce, ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_TRUE(csp->isFrameAncestorsEnforced());
}

TEST_F(ContentSecurityPolicyTest, MultipleReferrerDirectives)
{
    csp->didReceiveHeader("referrer unsafe-url; referrer origin;", ContentSecurityPolicyHeaderTypeEnforce, ContentSecurityPolicyHeaderSourceHTTP);
    csp->bindToExecutionContext(document.get());
    EXPECT_EQ(ReferrerPolicyOrigin, document->referrerPolicy());
}

TEST_F(ContentSecurityPolicyTest, MultipleReferrerPolicies)
{
    csp->didReceiveHeader("referrer unsafe-url;", ContentSecurityPolicyHeaderTypeEnforce, ContentSecurityPolicyHeaderSourceHTTP);
    csp->bindToExecutionContext(document.get());
    EXPECT_EQ(ReferrerPolicyAlways, document->referrerPolicy());
    document->processReferrerPolicy("origin");
    EXPECT_EQ(ReferrerPolicyOrigin, document->referrerPolicy());
}

TEST_F(ContentSecurityPolicyTest, UnknownReferrerDirective)
{
    csp->didReceiveHeader("referrer unsafe-url; referrer blahblahblah", ContentSecurityPolicyHeaderTypeEnforce, ContentSecurityPolicyHeaderSourceHTTP);
    csp->bindToExecutionContext(document.get());
    EXPECT_EQ(ReferrerPolicyAlways, document->referrerPolicy());
    document->processReferrerPolicy("origin");
    document->processReferrerPolicy("blahblahblah");
    EXPECT_EQ(ReferrerPolicyOrigin, document->referrerPolicy());
}

TEST_F(ContentSecurityPolicyTest, EmptyReferrerDirective)
{
    csp->didReceiveHeader("referrer;", ContentSecurityPolicyHeaderTypeEnforce, ContentSecurityPolicyHeaderSourceHTTP);
    csp->bindToExecutionContext(document.get());
    EXPECT_EQ(ReferrerPolicyNever, document->referrerPolicy());
}

} // namespace

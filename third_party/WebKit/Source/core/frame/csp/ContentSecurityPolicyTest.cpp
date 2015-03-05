// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/csp/ContentSecurityPolicy.h"

#include "core/dom/Document.h"
#include "core/loader/DocumentLoader.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include <gtest/gtest.h>

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

    RefPtr<ContentSecurityPolicy> csp;
    KURL secureURL;
    RefPtr<SecurityOrigin> secureOrigin;
    RefPtrWillBePersistent<Document> document;
};

TEST_F(ContentSecurityPolicyTest, ParseUpgradeInsecureRequestsDisabled)
{
    RuntimeEnabledFeatures::setExperimentalContentSecurityPolicyFeaturesEnabled(false);
    csp->didReceiveHeader("upgrade-insecure-requests", ContentSecurityPolicyHeaderTypeEnforce, ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(SecurityContext::InsecureRequestsDoNotUpgrade, csp->insecureRequestsPolicy());

    csp->bindToExecutionContext(document.get());
    EXPECT_EQ(SecurityContext::InsecureRequestsDoNotUpgrade, document->insecureRequestsPolicy());
}

TEST_F(ContentSecurityPolicyTest, ParseUpgradeInsecureRequestsEnabled)
{
    RuntimeEnabledFeatures::setExperimentalContentSecurityPolicyFeaturesEnabled(true);
    csp->didReceiveHeader("upgrade-insecure-requests", ContentSecurityPolicyHeaderTypeEnforce, ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(SecurityContext::InsecureRequestsUpgrade, csp->insecureRequestsPolicy());

    csp->bindToExecutionContext(document.get());
    EXPECT_EQ(SecurityContext::InsecureRequestsUpgrade, document->insecureRequestsPolicy());
}

TEST_F(ContentSecurityPolicyTest, ParseMonitorInsecureRequestsDisabled)
{
    RuntimeEnabledFeatures::setExperimentalContentSecurityPolicyFeaturesEnabled(false);
    csp->didReceiveHeader("upgrade-insecure-requests", ContentSecurityPolicyHeaderTypeReport, ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(SecurityContext::InsecureRequestsDoNotUpgrade, csp->insecureRequestsPolicy());

    csp->bindToExecutionContext(document.get());
    EXPECT_EQ(SecurityContext::InsecureRequestsDoNotUpgrade, document->insecureRequestsPolicy());
}

TEST_F(ContentSecurityPolicyTest, ParseMonitorInsecureRequestsEnabled)
{
    RuntimeEnabledFeatures::setExperimentalContentSecurityPolicyFeaturesEnabled(true);
    csp->didReceiveHeader("upgrade-insecure-requests", ContentSecurityPolicyHeaderTypeReport, ContentSecurityPolicyHeaderSourceHTTP);
    EXPECT_EQ(SecurityContext::InsecureRequestsDoNotUpgrade, csp->insecureRequestsPolicy());

    csp->bindToExecutionContext(document.get());
    EXPECT_EQ(SecurityContext::InsecureRequestsDoNotUpgrade, document->insecureRequestsPolicy());
}

} // namespace

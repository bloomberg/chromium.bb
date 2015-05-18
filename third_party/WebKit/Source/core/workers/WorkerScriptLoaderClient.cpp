// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/workers/WorkerScriptLoaderClient.h"

#include "core/frame/csp/ContentSecurityPolicy.h"
#include "platform/network/ContentSecurityPolicyResponseHeaders.h"
#include "platform/network/ResourceResponse.h"

namespace blink {

WorkerScriptLoaderClient::~WorkerScriptLoaderClient()
{
}

PassRefPtr<ContentSecurityPolicy> WorkerScriptLoaderClient::contentSecurityPolicy()
{
    return m_contentSecurityPolicy;
}

void WorkerScriptLoaderClient::processContentSecurityPolicy(const ResourceResponse& response)
{
    if (!response.url().protocolIs("blob") && !response.url().protocolIs("file") && !response.url().protocolIs("filesystem")) {
        m_contentSecurityPolicy = ContentSecurityPolicy::create();
        m_contentSecurityPolicy->setOverrideURLForSelf(response.url());
        m_contentSecurityPolicy->didReceiveHeaders(ContentSecurityPolicyResponseHeaders(response));
    }
}

void WorkerScriptLoaderClient::setContentSecurityPolicy(PassRefPtr<ContentSecurityPolicy> contentSecurityPolicy)
{
    m_contentSecurityPolicy = contentSecurityPolicy;
}

} // namespace blink

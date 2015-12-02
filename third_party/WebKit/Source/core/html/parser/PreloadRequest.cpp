// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/html/parser/PreloadRequest.h"

#include "core/dom/Document.h"
#include "core/fetch/FetchInitiatorInfo.h"

namespace blink {

bool PreloadRequest::isSafeToSendToAnotherThread() const
{
    return m_initiatorName.isSafeToSendToAnotherThread()
        && m_charset.isSafeToSendToAnotherThread()
        && m_resourceURL.isSafeToSendToAnotherThread()
        && m_baseURL.isSafeToSendToAnotherThread();
}

KURL PreloadRequest::completeURL(Document* document)
{
    if (!m_baseURL.isEmpty())
        return document->completeURLWithOverride(m_resourceURL, m_baseURL);
    return document->completeURL(m_resourceURL);
}

FetchRequest PreloadRequest::resourceRequest(Document* document)
{
    ASSERT(isMainThread());
    FetchInitiatorInfo initiatorInfo;
    initiatorInfo.name = AtomicString(m_initiatorName);
    initiatorInfo.position = m_initiatorPosition;
    ResourceRequest resourceRequest(completeURL(document));
    resourceRequest.setHTTPReferrer(SecurityPolicy::generateReferrer(m_referrerPolicy, resourceRequest.url(), document->outgoingReferrer()));
    FetchRequest request(resourceRequest, initiatorInfo);

    if (m_resourceType == Resource::ImportResource) {
        SecurityOrigin* securityOrigin = document->contextDocument()->securityOrigin();
        bool sameOrigin = securityOrigin->canRequest(request.url());
        request.setCrossOriginAccessControl(securityOrigin,
            sameOrigin ? AllowStoredCredentials : DoNotAllowStoredCredentials,
            ClientDidNotRequestCredentials);
    }

    if (m_isCORSEnabled)
        request.setCrossOriginAccessControl(document->securityOrigin(), m_allowCredentials);

    request.setDefer(m_defer);
    request.setResourceWidth(m_resourceWidth);
    request.clientHintsPreferences().updateFrom(m_clientHintsPreferences);

    return request;
}

}

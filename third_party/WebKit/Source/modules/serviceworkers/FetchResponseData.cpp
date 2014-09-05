// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "FetchResponseData.h"

#include "core/fetch/CrossOriginAccessControl.h"
#include "modules/serviceworkers/FetchHeaderList.h"
#include "public/platform/WebServiceWorkerResponse.h"

namespace blink {

FetchResponseData* FetchResponseData::create()
{
    // "Unless stated otherwise, a response's url is null, status is 200, status
    // message is `OK`, header list is an empty header list, and body is null."
    return new FetchResponseData(DefaultType, 200, "OK");
}

FetchResponseData* FetchResponseData::createNetworkErrorResponse()
{
    // "A network error is a response whose status is always 0, status message
    // is always the empty byte sequence, header list is aways an empty list,
    // and body is always null."
    return new FetchResponseData(ErrorType, 0, "");
}

FetchResponseData* FetchResponseData::createBasicFilteredResponse()
{
    // "A basic filtered response is a filtered response whose type is |basic|,
    // header list excludes any headers in internal response's header list whose
    // name is `Set-Cookie` or `Set-Cookie2`."
    FetchResponseData* response = new FetchResponseData(BasicType, m_status, m_statusMessage);
    response->m_url = m_url;
    for (size_t i = 0; i < m_headerList->size(); ++i) {
        const FetchHeaderList::Header* header = m_headerList->list()[i].get();
        if (header->first == "set-cookie" || header->first == "set-cookie2")
            continue;
        response->m_headerList->append(header->first, header->second);
    }
    response->m_blobDataHandle = m_blobDataHandle;
    response->m_internalResponse = this;
    return response;
}

FetchResponseData* FetchResponseData::createCORSFilteredResponse()
{
    // "A CORS filtered response is a filtered response whose type is |CORS|,
    // header list excludes all headers in internal response's header list,
    // except those whose name is either one of `Cache-Control`,
    // `Content-Language`, `Content-Type`, `Expires`, `Last-Modified`, and
    // `Pragma`, and except those whose name is one of the values resulting from
    // parsing `Access-Control-Expose-Headers` in internal response's header
    // list."
    FetchResponseData* response = new FetchResponseData(CORSType, m_status, m_statusMessage);
    response->m_url = m_url;
    HTTPHeaderSet accessControlExposeHeaderSet;
    String accessControlExposeHeaders;
    if (m_headerList->get("access-control-expose-headers", accessControlExposeHeaders))
        parseAccessControlExposeHeadersAllowList(accessControlExposeHeaders, accessControlExposeHeaderSet);
    for (size_t i = 0; i < m_headerList->size(); ++i) {
        const FetchHeaderList::Header* header = m_headerList->list()[i].get();
        if (!isOnAccessControlResponseHeaderWhitelist(header->first) && !accessControlExposeHeaderSet.contains(header->first))
            continue;
        response->m_headerList->append(header->first, header->second);
    }
    response->m_blobDataHandle = m_blobDataHandle;
    response->m_internalResponse = this;
    return response;
}

FetchResponseData* FetchResponseData::createOpaqueFilteredResponse()
{
    // "An opaque filtered response is a filtered response whose type is
    // |opaque|, status is 0, status message is the empty byte sequence, header
    // list is an empty list, and body is null."
    FetchResponseData* response = new FetchResponseData(OpaqueType, 0, "");
    response->m_internalResponse = this;
    return response;
}

void FetchResponseData::populateWebServiceWorkerResponse(WebServiceWorkerResponse& response)
{
    if (m_internalResponse) {
        m_internalResponse->populateWebServiceWorkerResponse(response);
        return;
    }
    response.setURL(url());
    response.setStatus(status());
    response.setStatusText(statusMessage());
    for (size_t i = 0; i < headerList()->size(); ++i) {
        const FetchHeaderList::Header* header = headerList()->list()[i].get();
        response.setHeader(header->first, header->second);
    }
    response.setBlobDataHandle(blobDataHandle());
}

FetchResponseData::FetchResponseData(Type type, unsigned short status, AtomicString statusMessage)
    : m_type(type)
    , m_status(status)
    , m_statusMessage(statusMessage)
    , m_headerList(FetchHeaderList::create())
{
}

void FetchResponseData::trace(Visitor* visitor)
{
    visitor->trace(m_headerList);
    visitor->trace(m_internalResponse);
}

} // namespace blink

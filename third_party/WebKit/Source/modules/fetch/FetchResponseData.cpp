// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/fetch/FetchResponseData.h"

#include "core/dom/DOMArrayBuffer.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/FetchHeaderList.h"
#include "public/platform/WebServiceWorkerResponse.h"

namespace blink {

namespace {

WebServiceWorkerResponseType fetchTypeToWebType(FetchResponseData::Type fetchType)
{
    WebServiceWorkerResponseType webType = WebServiceWorkerResponseTypeDefault;
    switch (fetchType) {
    case FetchResponseData::BasicType:
        webType = WebServiceWorkerResponseTypeBasic;
        break;
    case FetchResponseData::CORSType:
        webType = WebServiceWorkerResponseTypeCORS;
        break;
    case FetchResponseData::DefaultType:
        webType = WebServiceWorkerResponseTypeDefault;
        break;
    case FetchResponseData::ErrorType:
        webType = WebServiceWorkerResponseTypeError;
        break;
    case FetchResponseData::OpaqueType:
        webType = WebServiceWorkerResponseTypeOpaque;
        break;
    }
    return webType;
}

class StreamTeePump : public BodyStreamBuffer::Observer {
public:
    StreamTeePump(BodyStreamBuffer* inBuffer, BodyStreamBuffer* outBuffer1, BodyStreamBuffer* outBuffer2)
        : m_inBuffer(inBuffer)
        , m_outBuffer1(outBuffer1)
        , m_outBuffer2(outBuffer2)
    {
    }
    void onWrite() override
    {
        while (RefPtr<DOMArrayBuffer> buf = m_inBuffer->read()) {
            m_outBuffer1->write(buf);
            m_outBuffer2->write(buf);
        }
    }
    void onClose() override
    {
        m_outBuffer1->close();
        m_outBuffer2->close();
        cleanup();
    }
    void onError() override
    {
        m_outBuffer1->error(m_inBuffer->exception());
        m_outBuffer2->error(m_inBuffer->exception());
        cleanup();
    }
    void trace(Visitor* visitor) override
    {
        BodyStreamBuffer::Observer::trace(visitor);
        visitor->trace(m_inBuffer);
        visitor->trace(m_outBuffer1);
        visitor->trace(m_outBuffer2);
    }
    void start()
    {
        m_inBuffer->registerObserver(this);
        onWrite();
        if (m_inBuffer->hasError())
            return onError();
        if (m_inBuffer->isClosed())
            return onClose();
    }

private:
    void cleanup()
    {
        m_inBuffer->unregisterObserver();
        m_inBuffer.clear();
        m_outBuffer1.clear();
        m_outBuffer2.clear();
    }
    Member<BodyStreamBuffer> m_inBuffer;
    Member<BodyStreamBuffer> m_outBuffer1;
    Member<BodyStreamBuffer> m_outBuffer2;
};

} // namespace

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

FetchResponseData* FetchResponseData::createWithBuffer(BodyStreamBuffer* buffer)
{
    FetchResponseData* response = FetchResponseData::create();
    response->m_buffer = buffer;
    return response;
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
    response->m_buffer = m_buffer;
    response->m_contentTypeForBuffer = m_contentTypeForBuffer;
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
    response->m_buffer = m_buffer;
    response->m_contentTypeForBuffer = m_contentTypeForBuffer;
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

String FetchResponseData::contentTypeForBuffer() const
{
    return m_contentTypeForBuffer;
}

PassRefPtr<BlobDataHandle> FetchResponseData::internalBlobDataHandle() const
{
    if (m_internalResponse) {
        return m_internalResponse->m_blobDataHandle;
    }
    return m_blobDataHandle;
}

BodyStreamBuffer* FetchResponseData::internalBuffer() const
{
    if (m_internalResponse) {
        return m_internalResponse->m_buffer;
    }
    return m_buffer;
}

String FetchResponseData::internalContentTypeForBuffer() const
{
    if (m_internalResponse) {
        return m_internalResponse->contentTypeForBuffer();
    }
    return m_contentTypeForBuffer;
}

FetchResponseData* FetchResponseData::clone()
{
    FetchResponseData* newResponse = create();
    newResponse->m_type = m_type;
    if (m_terminationReason) {
        newResponse->m_terminationReason = adoptPtr(new TerminationReason);
        *newResponse->m_terminationReason = *m_terminationReason;
    }
    newResponse->m_url = m_url;
    newResponse->m_status = m_status;
    newResponse->m_statusMessage = m_statusMessage;
    newResponse->m_headerList = m_headerList->createCopy();
    newResponse->m_blobDataHandle = m_blobDataHandle;
    newResponse->m_contentTypeForBuffer = m_contentTypeForBuffer;
    if (!m_internalResponse) {
        if (!m_buffer)
            return newResponse;
        BodyStreamBuffer* original = m_buffer;
        m_buffer = new BodyStreamBuffer();
        newResponse->m_buffer = new BodyStreamBuffer();
        StreamTeePump* teePump = new StreamTeePump(original, m_buffer, newResponse->m_buffer);
        teePump->start();
        return newResponse;
    }

    ASSERT(!m_buffer || m_buffer == m_internalResponse->m_buffer);
    newResponse->m_internalResponse = m_internalResponse->clone();
    if (m_buffer) {
        m_buffer = m_internalResponse->m_buffer;
        newResponse->m_buffer = newResponse->m_internalResponse->m_buffer;
    }
    return newResponse;
}

void FetchResponseData::populateWebServiceWorkerResponse(WebServiceWorkerResponse& response)
{
    if (m_internalResponse) {
        m_internalResponse->populateWebServiceWorkerResponse(response);
        response.setResponseType(fetchTypeToWebType(m_type));
        return;
    }

    response.setURL(url());
    response.setStatus(status());
    response.setStatusText(statusMessage());
    response.setResponseType(fetchTypeToWebType(m_type));
    for (size_t i = 0; i < headerList()->size(); ++i) {
        const FetchHeaderList::Header* header = headerList()->list()[i].get();
        response.appendHeader(header->first, header->second);
    }
    response.setBlobDataHandle(m_blobDataHandle);
}

FetchResponseData::FetchResponseData(Type type, unsigned short status, AtomicString statusMessage)
    : m_type(type)
    , m_status(status)
    , m_statusMessage(statusMessage)
    , m_headerList(FetchHeaderList::create())
{
}

void FetchResponseData::setBlobDataHandle(PassRefPtr<BlobDataHandle> blobDataHandle)
{
    ASSERT(!m_buffer);
    m_blobDataHandle = blobDataHandle;
}

void FetchResponseData::trace(Visitor* visitor)
{
    visitor->trace(m_headerList);
    visitor->trace(m_internalResponse);
    visitor->trace(m_buffer);
}

} // namespace blink

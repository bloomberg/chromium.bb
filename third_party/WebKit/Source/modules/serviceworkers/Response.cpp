// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "Response.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/fileapi/Blob.h"
#include "modules/serviceworkers/FetchBodyStream.h"
#include "modules/serviceworkers/ResponseInit.h"
#include "public/platform/WebServiceWorkerResponse.h"

namespace blink {

namespace {

FetchResponseData* createFetchResponseDataFromWebResponse(const WebServiceWorkerResponse& webResponse)
{
    FetchResponseData* response = 0;
    if (200 <= webResponse.status() && webResponse.status() < 300)
        response = FetchResponseData::create();
    else
        response = FetchResponseData::createNetworkErrorResponse();

    response->setURL(webResponse.url());
    response->setStatus(webResponse.status());
    response->setStatusMessage(webResponse.statusText());
    return response;
}

Headers* createHeadersFromWebResponse(const WebServiceWorkerResponse& webResponse)
{
    Headers* headers = Headers::create();
    TrackExceptionState exceptionState;
    for (HashMap<String, String>::const_iterator i = webResponse.headers().begin(), end = webResponse.headers().end(); i != end; ++i) {
        headers->append(i->key, i->value, exceptionState);
        if (exceptionState.hadException())
            return 0;
    }
    return headers;
}

}

Response* Response::create(Blob* body, const Dictionary& responseInit, ExceptionState& exceptionState)
{
    return create(body, ResponseInit(responseInit), exceptionState);
}

Response* Response::create(const String& body, const Dictionary& responseInit, ExceptionState& exceptionState)
{
    OwnPtr<BlobData> blobData = BlobData::create();
    blobData->appendText(body, false);
    // "Set |Content-Type| to `text/plain;charset=UTF-8`."
    blobData->setContentType("text/plain;charset=UTF-8");
    const long long length = blobData->length();
    RefPtrWillBeRawPtr<Blob> blob = Blob::create(BlobDataHandle::create(blobData.release(), length));
    return create(blob.get(), ResponseInit(responseInit), exceptionState);
}

Response* Response::create(Blob* body, const ResponseInit& responseInit, ExceptionState& exceptionState)
{
    // "1. If |init|'s status member is not in the range 200 to 599, throw a
    // RangeError."
    if (responseInit.status < 200 || 599 < responseInit.status) {
        exceptionState.throwRangeError("Invalid status");
        return 0;
    }

    // FIXME: "2. If |init|'s statusText member does not match the Reason-Phrase
    //        token production, throw a TypeError."

    // "3. Let |r| be a new Response object, associated with a new response,
    // Headers object, and FetchBodyStream object."
    Response* r = new Response();

    // "4. Set |r|'s response's status to |init|'s status member."
    r->m_response->setStatus(responseInit.status);

    // "5. Set |r|'s response's status message to |init|'s statusText member."
    r->m_response->setStatusMessage(AtomicString(responseInit.statusText));

    // "6. If |init|'s headers member is present, run these substeps:"
    if (responseInit.headers) {
        // "1. Empty |r|'s response's header list."
        r->m_response->headerList()->clearList();
        // "2. Fill |r|'s Headers object with |init|'s headers member. Rethrow
        // any exceptions."
        r->m_headers->fillWith(responseInit.headers.get(), exceptionState);
        if (exceptionState.hadException())
            return 0;
    } else if (!responseInit.headersDictionary.isUndefinedOrNull()) {
        // "1. Empty |r|'s response's header list."
        r->m_response->headerList()->clearList();
        // "2. Fill |r|'s Headers object with |init|'s headers member. Rethrow
        // any exceptions."
        r->m_headers->fillWith(responseInit.headersDictionary, exceptionState);
        if (exceptionState.hadException())
            return 0;
    }
    // "7. If body is given, run these substeps:"
    if (body) {
        // "1. Let |stream| and |Content-Type| be the result of extracting body."
        // "2. Set |r|'s response's body to |stream|."
        // "3. If |Content-Type| is non-null and |r|'s response's header list
        // contains no header named `Content-Type`, append `Content-Type`/
        // |Content-Type| to |r|'s response's header list."
        r->m_response->setBlobDataHandle(body->blobDataHandle());
        if (!body->type().isNull() && !r->m_response->headerList()->has("Content-Type"))
            r->m_response->headerList()->append("Content-Type", body->type());
    }

    // FIXME: "8. Set |r|'s FetchBodyStream object's MIME type to the result of
    //        extracting a MIME type from |r|'s response's header list."

    // "9. Return |r|."
    return r;
}

Response* Response::create(FetchResponseData* response)
{
    return new Response(response);
}

Response* Response::create(const WebServiceWorkerResponse& webResponse)
{
    return new Response(webResponse);
}

String Response::type() const
{
    // "The type attribute's getter must return response's type."
    switch (m_response->type()) {
    case FetchResponseData::BasicType:
        return "basic";
    case FetchResponseData::CORSType:
        return "cors";
    case FetchResponseData::DefaultType:
        return "default";
    case FetchResponseData::ErrorType:
        return "error";
    case FetchResponseData::OpaqueType:
        return "opaque";
    }
    ASSERT_NOT_REACHED();
    return "";
}

String Response::url() const
{
    // "The url attribute's getter must return the empty string if response's
    // url is null and response's url, serialized with the exclude fragment
    // flag set, otherwise."
    if (!m_response->url().hasFragmentIdentifier())
        return m_response->url();
    KURL url(m_response->url());
    url.removeFragmentIdentifier();
    return url;
}

unsigned short Response::status() const
{
    // "The status attribute's getter must return response's status."
    return m_response->status();
}

String Response::statusText() const
{
    // "The statusText attribute's getter must return response's status message."
    return m_response->statusMessage();
}

Headers* Response::headers() const
{
    // "The headers attribute's getter must return the associated Headers object."
    return m_headers;
}

FetchBodyStream* Response::body(ExecutionContext* context)
{
    if (!m_response->blobDataHandle())
        return 0;
    if (!m_fetchBodyStream)
        m_fetchBodyStream = FetchBodyStream::create(context, m_response->blobDataHandle());
    return m_fetchBodyStream;
}

void Response::populateWebServiceWorkerResponse(WebServiceWorkerResponse& response)
{
    m_response->populateWebServiceWorkerResponse(response);
}

Response::Response()
    : m_response(FetchResponseData::create())
    , m_headers(Headers::create(m_response->headerList()))
{
    m_headers->setGuard(Headers::ResponseGuard);
    ScriptWrappable::init(this);
}

Response::Response(FetchResponseData* response)
    : m_response(response)
    , m_headers(Headers::create(m_response->headerList()))
{
    m_headers->setGuard(Headers::ResponseGuard);
    ScriptWrappable::init(this);
}

// FIXME: Handle response body data.
Response::Response(const WebServiceWorkerResponse& webResponse)
    : m_response(createFetchResponseDataFromWebResponse(webResponse))
    , m_headers(createHeadersFromWebResponse(webResponse))
{
    m_headers->setGuard(Headers::ResponseGuard);
    ScriptWrappable::init(this);
}

void Response::trace(Visitor* visitor)
{
    visitor->trace(m_response);
    visitor->trace(m_headers);
    visitor->trace(m_fetchBodyStream);
}

} // namespace blink

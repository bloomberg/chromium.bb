// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "Request.h"

#include "bindings/core/v8/Dictionary.h"
#include "core/dom/ExecutionContext.h"
#include "core/fetch/FetchUtils.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/loader/ThreadableLoader.h"
#include "core/xml/XMLHttpRequest.h"
#include "modules/serviceworkers/FetchManager.h"
#include "modules/serviceworkers/RequestInit.h"
#include "platform/NotImplemented.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/Referrer.h"
#include "public/platform/WebServiceWorkerRequest.h"

namespace blink {

Request* Request::createRequestWithRequestData(ExecutionContext* context, FetchRequestData* request, const RequestInit& init, FetchRequestData::Mode mode, FetchRequestData::Credentials credentials, ExceptionState& exceptionState)
{
    // "7. Let |mode| be |init|'s mode member if it is present, and
    // |fallbackMode| otherwise."
    // "8. If |mode| is non-null, set |request|'s mode to |mode|."
    if (init.mode == "same-origin") {
        request->setMode(FetchRequestData::SameOriginMode);
    } else if (init.mode == "no-cors") {
        request->setMode(mode = FetchRequestData::NoCORSMode);
    } else if (init.mode == "cors") {
        request->setMode(FetchRequestData::CORSMode);
    } else {
        // Instead of using null as a special fallback value, we pass the
        // current mode in Request::create(). So we just set here.
        request->setMode(mode);
    }

    // "9. Let |credentials| be |init|'s credentials member if it is present,
    // and |fallbackCredentials| otherwise."
    // "10. If |credentials| is non-null, set |request|'s credentials mode to
    // |credentials|.
    if (init.credentials == "omit") {
        request->setCredentials(FetchRequestData::OmitCredentials);
    } else if (init.credentials == "same-origin") {
        request->setCredentials(FetchRequestData::SameOriginCredentials);
    } else if (init.credentials == "include") {
        request->setCredentials(FetchRequestData::IncludeCredentials);
    } else {
        // Instead of using null as a special fallback value, we pass the
        // current credentials in Request::create(). So we just set here.
        request->setCredentials(credentials);
    }

    // "11. If |init|'s method member is present, let |method| be it and run
    // these substeps:"
    if (!init.method.isEmpty()) {
        // "1. If |method| is not a useful method, throw a TypeError."
        if (!FetchUtils::isUsefulMethod(init.method)) {
            exceptionState.throwTypeError("'" + init.method + "' HTTP method is unsupported.");
            return 0;
        }
        if (!isValidHTTPToken(init.method)) {
            exceptionState.throwTypeError("'" + init.method + "' is not a valid HTTP method.");
            return 0;
        }
        // FIXME: "2. Add case correction as in XMLHttpRequest?"
        // "3. Set |request|'s method to |method|."
        request->setMethod(XMLHttpRequest::uppercaseKnownHTTPMethod(AtomicString(init.method)));
    }
    // "12. Let |r| be a new Request object associated with |request|, Headers
    // object."
    Request* r = Request::create(context, request);

    // "13. Let |headers| be a copy of |r|'s Headers object."
    // "14. If |init|'s headers member is present, set |headers| to |init|'s
    // headers member."
    // We don't create a copy of r's Headers object when init's headers member
    // is present.
    Headers* headers = 0;
    if (!init.headers && init.headersDictionary.isUndefinedOrNull()) {
        headers = r->headers()->createCopy();
    }
    // "15. Empty |r|'s request's header list."
    r->clearHeaderList();

    // "16. If |r|'s request's mode is no CORS, run these substeps:
    if (r->request()->mode() == FetchRequestData::NoCORSMode) {
        // "1. If |r|'s request's method is not a simple method, throw a
        // TypeError."
        if (!FetchUtils::isSimpleMethod(r->request()->method())) {
            exceptionState.throwTypeError("'" + r->request()->method() + "' is unsupported in no-cors mode.");
            return 0;
        }
        // "Set |r|'s Headers object's guard to |request-no-CORS|.
        r->headers()->setGuard(Headers::RequestNoCORSGuard);
    }

    // "17. Fill |r|'s Headers object with |headers|. Rethrow any exceptions."
    if (init.headers) {
        ASSERT(init.headersDictionary.isUndefinedOrNull());
        r->headers()->fillWith(init.headers.get(), exceptionState);
    } else if (!init.headersDictionary.isUndefinedOrNull()) {
        r->headers()->fillWith(init.headersDictionary, exceptionState);
    } else {
        ASSERT(headers);
        r->headers()->fillWith(headers, exceptionState);
    }
    if (exceptionState.hadException())
        return 0;
    // "18. If |init|'s body member is present, run these substeps:"
    if (init.bodyBlobHandle) {
        // "1. Let |stream| and |Content-Type| be the result of extracting
        // |init|'s body member."
        // "2. Set |r|'s request's body to |stream|."
        // "3.If |Content-Type| is non-null and |r|'s request's header list
        //  contains no header named `Content-Type`, append
        // `Content-Type`/|Content-Type| to |r|'s Headers object. Rethrow any
        // exception."
        r->setBodyBlobHandle(init.bodyBlobHandle);
        if (!init.bodyBlobHandle->type().isEmpty() && !r->headers()->has("Content-Type", exceptionState)) {
            r->headers()->append("Content-Type", init.bodyBlobHandle->type(), exceptionState);
        }
        if (exceptionState.hadException())
            return 0;
    }
    // "19. Set |r|'s MIME type to the result of extracting a MIME type from
    // |r|'s request's header list."
    // FIXME: We don't have MIME type in Request object yet.

    // "20. Return |r|."
    return r;
}

Request* Request::create(ExecutionContext* context, const String& input, ExceptionState& exceptionState)
{
    return create(context, input, Dictionary(), exceptionState);
}

Request* Request::create(ExecutionContext* context, const String& input, const Dictionary& init, ExceptionState& exceptionState)
{
    // "2. Let |request| be |input|'s associated request, if |input| is a
    // Request object, and a new request otherwise."
    FetchRequestData* request(FetchRequestData::create(context));
    // "3. Set |request| to a restricted copy of itself."
    request = request->createRestrictedCopy(context, SecurityOrigin::create(context->url()));
    // "6. If |input| is a string, run these substeps:"
    // "1. Let |parsedURL| be the result of parsing |input| with entry settings
    // object's API base URL."
    KURL parsedURL = context->completeURL(input);
    // "2. If |parsedURL| is failure, throw a TypeError."
    if (!parsedURL.isValid()) {
        exceptionState.throwTypeError("Invalid URL");
        return 0;
    }
    // "3. Set |request|'s url to |parsedURL|."
    request->setURL(parsedURL);
    // "4. Set |fallbackMode| to CORS."
    // "5. Set |fallbackCredentials| to omit."
    return createRequestWithRequestData(context, request, RequestInit(context, init, exceptionState), FetchRequestData::CORSMode, FetchRequestData::OmitCredentials, exceptionState);
}

Request* Request::create(ExecutionContext* context, Request* input, ExceptionState& exceptionState)
{
    return create(context, input, Dictionary(), exceptionState);
}

Request* Request::create(ExecutionContext* context, Request* input, const Dictionary& init, ExceptionState& exceptionState)
{
    // "1. If input is a Request object, run these substeps:"
    // "  1. If input's used flag is set, throw a TypeError."
    // "  2. Set input's used flag."
    if (input->bodyUsed()) {
        exceptionState.throwTypeError(
            "Cannot construct a Request with a Request object that has already been used.");
        return 0;
    }
    input->setBodyUsed();
    // "2. Let |request| be |input|'s associated request, if |input| is a
    // Request object, and a new request otherwise."
    // "3. Set |request| to a restricted copy of itself."
    FetchRequestData* request(input->request()->createRestrictedCopy(context, SecurityOrigin::create(context->url())));
    // "4. Let |fallbackMode| be null."
    // "5. Let |fallbackCredentials| be null."
    // Instead of using null as a special fallback value, just pass the current
    // mode and credentials; it has the same effect.
    const FetchRequestData::Mode currentMode = request->mode();
    const FetchRequestData::Credentials currentCredentials = request->credentials();
    return createRequestWithRequestData(context, request, RequestInit(context, init, exceptionState), currentMode, currentCredentials, exceptionState);
}

Request* Request::create(ExecutionContext* context, FetchRequestData* request)
{
    Request* r = new Request(context, request);
    r->suspendIfNeeded();
    return r;
}

Request::Request(ExecutionContext* context, FetchRequestData* request)
    : Body(context)
    , m_request(request)
    , m_headers(Headers::create(m_request->headerList()))
{
    m_headers->setGuard(Headers::RequestGuard);
}

Request* Request::create(ExecutionContext* context, const WebServiceWorkerRequest& webRequest)
{
    Request* r = new Request(context, webRequest);
    r->suspendIfNeeded();
    return r;
}

Request* Request::create(const Request& copyFrom)
{
    Request* r = new Request(copyFrom);
    r->suspendIfNeeded();
    return r;
}

Request::Request(ExecutionContext* context, const WebServiceWorkerRequest& webRequest)
    : Body(context)
    , m_request(FetchRequestData::create(webRequest))
    , m_headers(Headers::create(m_request->headerList()))
{
    m_headers->setGuard(Headers::RequestGuard);
}

Request::Request(const Request& copy_from)
    : Body(copy_from)
    , m_request(copy_from.m_request)
    , m_headers(copy_from.m_headers->createCopy())
{
}


String Request::method() const
{
    // "The method attribute's getter must return request's method."
    return m_request->method();
}

String Request::url() const
{
    // The url attribute's getter must return request's url, serialized with the exclude fragment flag set.
    if (!m_request->url().hasFragmentIdentifier())
        return m_request->url();
    KURL url(m_request->url());
    url.removeFragmentIdentifier();
    return url;
}

String Request::referrer() const
{
    // "The referrer attribute's getter must return the empty string if
    // request's referrer is none, and request's referrer, serialized,
    // otherwise."
    return m_request->referrer().referrer().referrer;
}

String Request::mode() const
{
    // "The mode attribute's getter must return the value corresponding to the
    // first matching statement, switching on request's mode:"
    switch (m_request->mode()) {
    case FetchRequestData::SameOriginMode:
        return "same-origin";
    case FetchRequestData::NoCORSMode:
        return "no-cors";
    case FetchRequestData::CORSMode:
    case FetchRequestData::CORSWithForcedPreflight:
        return "cors";
    }
    ASSERT_NOT_REACHED();
    return "";
}

String Request::credentials() const
{
    // "The credentials attribute's getter must return the value corresponding
    // to the first matching statement, switching on request's credentials
    // mode:"
    switch (m_request->credentials()) {
    case FetchRequestData::OmitCredentials:
        return "omit";
    case FetchRequestData::SameOriginCredentials:
        return "same-origin";
    case FetchRequestData::IncludeCredentials:
        return "include";
    }
    ASSERT_NOT_REACHED();
    return "";
}

Request* Request::clone() const
{
    return Request::create(*this);
}

void Request::populateWebServiceWorkerRequest(WebServiceWorkerRequest& webRequest)
{
    webRequest.setMethod(method());
    webRequest.setURL(m_request->url());

    const FetchHeaderList* headerList = m_headers->headerList();
    for (size_t i = 0, size = headerList->size(); i < size; ++i) {
        const FetchHeaderList::Header& header = headerList->entry(i);
        webRequest.appendHeader(header.first, header.second);
    }

    webRequest.setReferrer(m_request->referrer().referrer().referrer, static_cast<WebReferrerPolicy>(m_request->referrer().referrer().referrerPolicy));
    // FIXME: How can we set isReload properly? What is the correct place to load it in to the Request object? We should investigate the right way
    // to plumb this information in to here.
}

void Request::setBodyBlobHandle(PassRefPtr<BlobDataHandle> blobDataHandle)
{
    m_request->setBlobDataHandle(blobDataHandle);
}

void Request::clearHeaderList()
{
    m_request->headerList()->clearList();
}

PassRefPtr<BlobDataHandle> Request::blobDataHandle()
{
    return m_request->blobDataHandle();
}

void Request::trace(Visitor* visitor)
{
    Body::trace(visitor);
    visitor->trace(m_request);
    visitor->trace(m_headers);
}

} // namespace blink

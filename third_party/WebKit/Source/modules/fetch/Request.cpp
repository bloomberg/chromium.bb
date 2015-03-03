// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/fetch/Request.h"

#include "bindings/core/v8/Dictionary.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/fetch/FetchUtils.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/loader/ThreadableLoader.h"
#include "modules/fetch/FetchManager.h"
#include "modules/fetch/RequestInit.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/Referrer.h"
#include "public/platform/WebServiceWorkerRequest.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

FetchRequestData* createCopyOfFetchRequestDataForFetch(ExecutionContext* context, const FetchRequestData* original)
{
    FetchRequestData* request = FetchRequestData::create();
    request->setURL(original->url());
    request->setMethod(original->method());
    request->setHeaderList(original->headerList()->createCopy());
    request->setUnsafeRequestFlag(true);
    request->setBlobDataHandle(original->blobDataHandle());
    // FIXME: Set client.
    request->setOrigin(SecurityOrigin::create(context->url()));
    // FIXME: Set ForceOriginHeaderFlag.
    request->setSameOriginDataURLFlag(true);
    request->mutableReferrer()->setClient();
    request->setContext(WebURLRequest::RequestContextFetch);
    request->setMode(original->mode());
    request->setCredentials(original->credentials());
    // FIXME: Set cache mode.
    return request;
}

Request* Request::createRequestWithRequestOrString(ExecutionContext* context, Request* inputRequest, const String& inputString, const RequestInit& init, ExceptionState& exceptionState)
{
    // "1. If input is a Request object, run these substeps:"
    if (inputRequest) {
        // "1. If input's used flag is set, throw a TypeError."
        if (inputRequest->bodyUsed()) {
            exceptionState.throwTypeError("Cannot construct a Request with a Request object that has already been used.");
            return 0;
        }
        // "2. Set input's used flag."
        inputRequest->setBodyUsed();
    }

    // "2. Let |request| be |input|'s associated request, if |input| is a
    // Request object, and a new request otherwise."
    // "3. Set |request| to a new request whose url is |request|'s url, method
    // is |request|'s method, header list is a copy of |request|'s header list,
    // unsafe request flag is set, body is |request|'s body, client is entry
    // settings object, origin is entry settings object's origin, force Origin
    // header flag is set, same-origin data URL flag is set, referrer is client,
    // context is fetch, mode is |request|'s mode, credentials mode is
    // |request|'s credentials mode, and cache mode is |request|'s cache mode."
    FetchRequestData* request = createCopyOfFetchRequestDataForFetch(context, inputRequest ? inputRequest->request() : FetchRequestData::create());

    // "4. Let |fallbackMode| be null."
    // "5. Let |fallbackCredentials| be null."
    // "6. Let |fallbackCache| be null."
    // We don't use fallback values. We set these flags directly in below.

    // "7. If |input| is a string, run these substeps:"
    if (!inputRequest) {
        // "1. Let |parsedURL| be the result of parsing |input| with entry
        // settings object's API base URL."
        KURL parsedURL = context->completeURL(inputString);
        // "2. If |parsedURL| is failure, throw a TypeError."
        if (!parsedURL.isValid()) {
            exceptionState.throwTypeError("Failed to parse URL from " + inputString);
            return 0;
        }
        // "3. Set |request|'s url to |parsedURL|."
        request->setURL(parsedURL);
        // "4. Set |fallbackMode| to CORS."
        // "5. Set |fallbackCredentials| to omit."
        // "6. Set |fallbackCache| to default."
        // We don't use fallback values. We set these flags directly in below.
    }

    // "8. Let |mode| be |init|'s mode member if it is present, and
    // |fallbackMode| otherwise."
    // "9. If |mode| is non-null, set |request|'s mode to |mode|."
    if (init.mode == "same-origin") {
        request->setMode(WebURLRequest::FetchRequestModeSameOrigin);
    } else if (init.mode == "no-cors") {
        request->setMode(WebURLRequest::FetchRequestModeNoCORS);
    } else if (init.mode == "cors") {
        request->setMode(WebURLRequest::FetchRequestModeCORS);
    } else {
        if (!inputRequest)
            request->setMode(WebURLRequest::FetchRequestModeCORS);
    }

    // "10. Let |credentials| be |init|'s credentials member if it is present,
    // and |fallbackCredentials| otherwise."
    // "11. If |credentials| is non-null, set |request|'s credentials mode to
    // |credentials|.
    if (init.credentials == "omit") {
        request->setCredentials(WebURLRequest::FetchCredentialsModeOmit);
    } else if (init.credentials == "same-origin") {
        request->setCredentials(WebURLRequest::FetchCredentialsModeSameOrigin);
    } else if (init.credentials == "include") {
        request->setCredentials(WebURLRequest::FetchCredentialsModeInclude);
    } else {
        if (!inputRequest)
            request->setCredentials(WebURLRequest::FetchCredentialsModeOmit);
    }

    // FIXME: "12. Let |cache| be |init|'s cache member if it is present, and
    // |fallbackCache| otherwise."
    // FIXME: "13. If |cache| is non-null, set |request|'s cache mode to
    // |cache|."

    // "14. If |init|'s method member is present, let |method| be it and run
    // these substeps:"
    if (!init.method.isEmpty()) {
        // "1. If |method| is not a method or method is a forbidden method,
        // throw a TypeError."
        if (!isValidHTTPToken(init.method)) {
            exceptionState.throwTypeError("'" + init.method + "' is not a valid HTTP method.");
            return 0;
        }
        if (FetchUtils::isForbiddenMethod(init.method)) {
            exceptionState.throwTypeError("'" + init.method + "' HTTP method is unsupported.");
            return 0;
        }
        // "2. Normalize |method|."
        // "3. Set |request|'s method to |method|."
        request->setMethod(FetchUtils::normalizeMethod(AtomicString(init.method)));
    }
    // "15. Let |r| be a new Request object associated with |request|, and a new
    // Headers object."
    Request* r = Request::create(context, request);
    // "16. Let |headers| be a copy of |r|'s Headers object."
    // "17. If |init|'s headers member is present, set |headers| to |init|'s
    // headers member."
    // We don't create a copy of r's Headers object when init's headers member
    // is present.
    Headers* headers = 0;
    if (!init.headers && init.headersDictionary.isUndefinedOrNull()) {
        headers = r->headers()->createCopy();
    }
    // "18. Empty |r|'s request's header list."
    r->clearHeaderList();
    // "19. If |r|'s request's mode is no CORS, run these substeps:
    if (r->request()->mode() == WebURLRequest::FetchRequestModeNoCORS) {
        // "1. If |r|'s request's method is not a simple method, throw a
        // TypeError."
        if (!FetchUtils::isSimpleMethod(r->request()->method())) {
            exceptionState.throwTypeError("'" + r->request()->method() + "' is unsupported in no-cors mode.");
            return 0;
        }
        // "Set |r|'s Headers object's guard to |request-no-CORS|.
        r->headers()->setGuard(Headers::RequestNoCORSGuard);
    }
    // "20. Fill |r|'s Headers object with |headers|. Rethrow any exceptions."
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

    // "21. If |init|'s body member is present, run these substeps:"
    if (init.bodyBlobHandle) {
        // "1. If request's method is `GET` or `HEAD`, throw a TypeError."
        // "2. Let |stream| and |Content-Type| be the result of extracting
        //  |init|'s body member."
        // "3. Set |r|'s request's body to |stream|."
        // "4. If |Content-Type| is non-null and |r|'s request's header list
        //  contains no header named `Content-Type`, append
        //  `Content-Type`/|Content-Type| to |r|'s Headers object. Rethrow any
        //  exception."
        if (request->method() == "GET" || request->method() == "HEAD") {
            exceptionState.throwTypeError("Request with GET/HEAD method cannot have body.");
            return 0;
        }
        r->setBodyBlobHandle(init.bodyBlobHandle);
        if (!init.bodyBlobHandle->type().isEmpty() && !r->headers()->has("Content-Type", exceptionState)) {
            r->headers()->append("Content-Type", init.bodyBlobHandle->type(), exceptionState);
        }
        if (exceptionState.hadException())
            return 0;
    }
    // "22. Set |r|'s MIME type to the result of extracting a MIME type from
    // |r|'s request's header list."
    // FIXME: We don't have MIME type in Request object yet.

    // "23. Return |r|."
    return r;
}

Request* Request::create(ExecutionContext* context, const RequestInfo& input, const Dictionary& init, ExceptionState& exceptionState)
{
    ASSERT(!input.isNull());
    if (input.isUSVString())
        return create(context, input.getAsUSVString(), init, exceptionState);
    return create(context, input.getAsRequest(), init, exceptionState);
}

Request* Request::create(ExecutionContext* context, const String& input, ExceptionState& exceptionState)
{
    return create(context, input, Dictionary(), exceptionState);
}

Request* Request::create(ExecutionContext* context, const String& input, const Dictionary& init, ExceptionState& exceptionState)
{
    return createRequestWithRequestOrString(context, 0, input, RequestInit(context, init, exceptionState), exceptionState);
}

Request* Request::create(ExecutionContext* context, Request* input, ExceptionState& exceptionState)
{
    return create(context, input, Dictionary(), exceptionState);
}

Request* Request::create(ExecutionContext* context, Request* input, const Dictionary& init, ExceptionState& exceptionState)
{
    return createRequestWithRequestOrString(context, input, String(), RequestInit(context, init, exceptionState), exceptionState);
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

Request::Request(ExecutionContext* context, const WebServiceWorkerRequest& webRequest)
    : Body(context)
    , m_request(FetchRequestData::create(webRequest))
    , m_headers(Headers::create(m_request->headerList()))
{
    m_headers->setGuard(Headers::RequestGuard);
}

Request::Request(const Request& cloneFrom)
    : Body(cloneFrom)
    , m_request(cloneFrom.m_request->createCopy())
    , m_headers(Headers::create(m_request->headerList()))
{
    m_headers->setGuard(cloneFrom.headers()->guard());
}

String Request::method() const
{
    // "The method attribute's getter must return request's method."
    return m_request->method();
}

KURL Request::url() const
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
    // request's referrer is no referrer, "about:client" if request's referrer
    // is client and request's referrer, serialized, otherwise."
    if (m_request->referrer().isNoReferrer())
        return String();
    if (m_request->referrer().isClient())
        return String("about:client");
    return m_request->referrer().referrer().referrer;
}

String Request::mode() const
{
    // "The mode attribute's getter must return the value corresponding to the
    // first matching statement, switching on request's mode:"
    switch (m_request->mode()) {
    case WebURLRequest::FetchRequestModeSameOrigin:
        return "same-origin";
    case WebURLRequest::FetchRequestModeNoCORS:
        return "no-cors";
    case WebURLRequest::FetchRequestModeCORS:
    case WebURLRequest::FetchRequestModeCORSWithForcedPreflight:
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
    case WebURLRequest::FetchCredentialsModeOmit:
        return "omit";
    case WebURLRequest::FetchCredentialsModeSameOrigin:
        return "same-origin";
    case WebURLRequest::FetchCredentialsModeInclude:
        return "include";
    }
    ASSERT_NOT_REACHED();
    return "";
}

Request* Request::clone(ExceptionState& exceptionState) const
{
    if (bodyUsed()) {
        exceptionState.throwTypeError("Request body is already used");
        return nullptr;
    }
    // FIXME: We throw an error while cloning the Response which body was
    // partially read. But in Request case, we don't. When the behavior of the
    // partially read streams will be well defined in the spec, we have to
    // implement the behavior correctly.
    Request* r = new Request(*this);
    r->suspendIfNeeded();
    return r;
}

void Request::populateWebServiceWorkerRequest(WebServiceWorkerRequest& webRequest) const
{
    webRequest.setMethod(method());
    // This strips off the fragment part.
    webRequest.setURL(url());

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

PassRefPtr<BlobDataHandle> Request::blobDataHandle() const
{
    return m_request->blobDataHandle();
}

BodyStreamBuffer* Request::buffer() const
{
    return nullptr;
}

String Request::contentTypeForBuffer() const
{
    // We don't support BodyStreamBuffer for Request yet.
    ASSERT_NOT_REACHED();
    return String();
}

DEFINE_TRACE(Request)
{
    Body::trace(visitor);
    visitor->trace(m_request);
    visitor->trace(m_headers);
}

} // namespace blink

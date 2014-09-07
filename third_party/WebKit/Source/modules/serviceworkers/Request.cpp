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
#include "modules/serviceworkers/HeadersForEachCallback.h"
#include "modules/serviceworkers/RequestInit.h"
#include "platform/NotImplemented.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/Referrer.h"
#include "public/platform/WebServiceWorkerRequest.h"

namespace blink {

namespace {

class FillWebRequestHeaders : public HeadersForEachCallback {
public:
    FillWebRequestHeaders(WebServiceWorkerRequest* webRequest) : m_webRequest(webRequest) { }

    virtual bool handleItem(ScriptValue, const String&, const String&, Headers*)
    {
        ASSERT_NOT_REACHED();
        return false;
    }

    virtual bool handleItem(const String& value, const String& key, Headers*)
    {
        m_webRequest->setHeader(key, value);
        return true;
    }

private:
    WebServiceWorkerRequest* m_webRequest;
};

Request* createRequestWithRequestData(FetchRequestData* request, const RequestInit& init, FetchRequestData::Mode mode, FetchRequestData::Credentials credentials, ExceptionState& exceptionState)
{
    // "6. Let |mode| be |init|'s mode member if it is present, and
    // |fallbackMode| otherwise."
    // "7. If |mode| is non-null, set |request|'s mode to |mode|."
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

    // "8. Let |credentials| be |init|'s credentials member if it is present,
    // and |fallbackCredentials| otherwise."
    // "9. If |credentials| is non-null, set |request|'s credentials mode to
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

    // "10. If |init|'s method member is present, let |method| be it and run
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
    // "11. Let |r| be a new Request object associated with |request|, Headers
    // object, and FetchBodyStream object."
    Request* r = Request::create(request);

    // "12. Let |headers| be a copy of |r|'s Headers object."
    // "13. If |init|'s headers member is present, set |headers| to |init|'s
    // headers member."
    // We don't create a copy of r's Headers object when init's headers member
    // is present.
    Headers* headers = 0;
    if (!init.headers && init.headersDictionary.isUndefinedOrNull()) {
        headers = r->headers()->createCopy();
    }
    // "14. Empty |r|'s request's header list."
    r->request()->headerList()->clearList();

    // "15. If |r|'s request's mode is no CORS, run these substeps:
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

    // "16. Fill |r|'s Headers object with |headers|. Rethrow any exceptions."
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
    // "17. If |init|'s body member is present, run these substeps:"
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
    // "18. Set |r|'s FetchBodyStream object's MIME type to the result of
    // extracting a MIME type from |r|'s request's header list."
    // FIXME: We don't have MIME type in FetchBodyStream object yet.

    // "19. Return |r|."
    return r;
}

} // namespace

Request* Request::create(ExecutionContext* context, const String& input, ExceptionState& exceptionState)
{
    return create(context, input, Dictionary(), exceptionState);
}

Request* Request::create(ExecutionContext* context, const String& input, const Dictionary& init, ExceptionState& exceptionState)
{
    // "1. Let |request| be |input|'s associated request, if |input| is a
    // Request object, and a new request otherwise."
    FetchRequestData* request(FetchRequestData::create(context));
    // "2. Set |request| to a restricted copy of itself."
    request = request->createRestrictedCopy(context, SecurityOrigin::create(context->url()));
    // "5. If |input| is a string, run these substeps:"
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
    return createRequestWithRequestData(request, RequestInit(context, init, exceptionState), FetchRequestData::CORSMode, FetchRequestData::OmitCredentials, exceptionState);
}

Request* Request::create(ExecutionContext* context, Request* input, ExceptionState& exceptionState)
{
    return create(context, input, Dictionary(), exceptionState);
}

Request* Request::create(ExecutionContext* context, Request* input, const Dictionary& init, ExceptionState& exceptionState)
{
    // "1. Let |request| be |input|'s associated request, if |input| is a
    // Request object, and a new request otherwise."
    // "2. Set |request| to a restricted copy of itself."
    FetchRequestData* request(input->request()->createRestrictedCopy(context, SecurityOrigin::create(context->url())));
    // "3. Let |fallbackMode| be null."
    // "4. Let |fallbackCredentials| be null."
    // Instead of using null as a special fallback value, just pass the current
    // mode and credentials; it has the same effect.
    const FetchRequestData::Mode currentMode = request->mode();
    const FetchRequestData::Credentials currentCredentials = request->credentials();
    return createRequestWithRequestData(request, RequestInit(context, init, exceptionState), currentMode, currentCredentials, exceptionState);
}

Request* Request::create(FetchRequestData* request)
{
    return new Request(request);
}

Request::Request(FetchRequestData* request)
    : m_request(request)
    , m_headers(Headers::create(m_request->headerList()))
{
    m_headers->setGuard(Headers::RequestGuard);
}

Request* Request::create(const WebServiceWorkerRequest& webRequest)
{
    return new Request(webRequest);
}

Request::Request(const WebServiceWorkerRequest& webRequest)
    : m_request(FetchRequestData::create(webRequest))
    , m_headers(Headers::create(m_request->headerList()))
{
    m_headers->setGuard(Headers::RequestGuard);
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

FetchBodyStream* Request::body(ExecutionContext* context)
{
    if (!m_request->blobDataHandle())
        return 0;
    if (!m_fetchBodyStream)
        m_fetchBodyStream = FetchBodyStream::create(context, m_request->blobDataHandle());
    return m_fetchBodyStream;
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

void Request::populateWebServiceWorkerRequest(WebServiceWorkerRequest& webRequest)
{
    webRequest.setMethod(method());
    webRequest.setURL(m_request->url());
    m_headers->forEach(adoptPtrWillBeNoop(new FillWebRequestHeaders(&webRequest)));
    webRequest.setReferrer(m_request->referrer().referrer().referrer, static_cast<WebReferrerPolicy>(m_request->referrer().referrer().referrerPolicy));
    // FIXME: How can we set isReload properly? What is the correct place to load it in to the Request object? We should investigate the right way
    // to plumb this information in to here.
}

void Request::setBodyBlobHandle(PassRefPtr<BlobDataHandle>blobHandle)
{
    m_request->setBlobDataHandle(blobHandle);
}

void Request::trace(Visitor* visitor)
{
    visitor->trace(m_request);
    visitor->trace(m_headers);
    visitor->trace(m_fetchBodyStream);
}

} // namespace blink

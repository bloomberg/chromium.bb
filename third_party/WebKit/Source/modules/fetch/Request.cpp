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
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/FetchBlobDataConsumerHandle.h"
#include "modules/fetch/FetchManager.h"
#include "modules/fetch/RequestInit.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/Referrer.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRequest.h"

namespace blink {

FetchRequestData* createCopyOfFetchRequestDataForFetch(ScriptState* scriptState, const FetchRequestData* original)
{
    FetchRequestData* request = FetchRequestData::create();
    request->setURL(original->url());
    request->setMethod(original->method());
    request->setHeaderList(original->headerList()->clone());
    request->setUnsafeRequestFlag(true);
    // FIXME: Set client.
    DOMWrapperWorld& world = scriptState->world();
    if (world.isIsolatedWorld())
        request->setOrigin(world.isolatedWorldSecurityOrigin());
    else
        request->setOrigin(scriptState->executionContext()->securityOrigin());
    // FIXME: Set ForceOriginHeaderFlag.
    request->setSameOriginDataURLFlag(true);
    request->setReferrer(original->referrer());
    request->setMode(original->mode());
    request->setCredentials(original->credentials());
    request->setRedirect(original->redirect());
    request->setIntegrity(original->integrity());
    // FIXME: Set cache mode.
    // TODO(yhirano): Set redirect mode.
    return request;
}

Request* Request::createRequestWithRequestOrString(ScriptState* scriptState, Request* inputRequest, const String& inputString, RequestInit& init, ExceptionState& exceptionState)
{
    // "1. Let |temporaryBody| be null."
    BodyStreamBuffer* temporaryBody = nullptr;

    if (inputRequest) {
        // We check bodyUsed even when the body is null in spite of the
        // spec. See https://github.com/whatwg/fetch/issues/61 for details.
        if (inputRequest->bodyUsed()) {
            exceptionState.throwTypeError("Cannot construct a Request with a Request object that has already been used.");
            return nullptr;
        }
    }

    // "2. If |input| is a Request object and |input|'s body is non-null, run
    // these substeps:"
    if (inputRequest && inputRequest->hasBody()) {
        // "1. If |input|'s used flag is set, throw a TypeError."
        // "2. Set |temporaryBody| to |input|'s body."
        if (inputRequest->bodyUsed()) {
            exceptionState.throwTypeError("Cannot construct a Request with a Request object that has already been used.");
            return nullptr;
        }
        temporaryBody = inputRequest->bodyBuffer();
    }

    // "3. Let |request| be |input|'s request, if |input| is a Request object,
    // and a new request otherwise."
    // "4. Let origin be entry settings object's origin."
    RefPtr<SecurityOrigin> origin = scriptState->executionContext()->securityOrigin();

    // TODO(yhirano):
    //   "5. Let window be client."
    //   "6. If request's window is an environment settings object and its
    //   origin is same origin with origin, set window to request's window."
    //   "7. If init's window member is present and it is not null, throw a
    //   TypeError."
    //   "8. If init's window member is present, set window to no-window."
    // "9. Set |request| to a new request whose url is |request|'s current url,
    // method is |request|'s method, header list is a copy of |request|'s header
    // list, unsafe-request flag is set, client is entry settings object, window
    // is window, origin is origin, force-Origin-header flag is set, same-origin
    // data-URL flag is set, referrer is request's referrer, referrer policy is
    // |request|'s referrer policy, context is the empty string, mode is
    // |request|'s mode, credentials mode is |request|'s credentials mode, cache
    // mode is |request|'s cache mode, redirect mode is |request|'s redirect
    // mode, and integrity metadata is |request|'s integrity metadata."
    FetchRequestData* request = createCopyOfFetchRequestDataForFetch(scriptState, inputRequest ? inputRequest->request() : FetchRequestData::create());

    // "10. Let |fallbackMode| be null."
    // "11. Let |fallbackCredentials| be null."
    // "12. Let |baseURL| be entry settings object's API base URL."
    // We don't use fallback values. We set these flags directly in below.

    // "13. If |input| is a string, run these substeps:"
    if (!inputRequest) {
        // "1. Let |parsedURL| be the result of parsing |input| with |baseURL|."
        KURL parsedURL = scriptState->executionContext()->completeURL(inputString);
        // "2. If |parsedURL| is failure, throw a TypeError."
        if (!parsedURL.isValid()) {
            exceptionState.throwTypeError("Failed to parse URL from " + inputString);
            return nullptr;
        }
        // TODO(yhirano): "3. If |parsedURL| includes credentials, throw a
        // TypeError."
        // "4. Set |request|'s url to |parsedURL|."
        request->setURL(parsedURL);
        // "5. Set |fallbackMode| to CORS."
        // "6. Set |fallbackCredentials| to omit."
        // We don't use fallback values. We set these flags directly in below.
    }

    //   14. If any of |init|'s members are present, set |request|'s referrer
    // to client, and |request|'s referrer policy to the empty string.
    //   => RequestInit::RequestInit.
    //   15. If |init|'s referrer member is present, run these substeps:
    // Note that JS null and undefined are encoded as an empty string and thus
    // a null string means referrer member is not set.
    //   16. If |init|'s referrerPolicy member is present, set |request|'s
    // referrer policy to it.
    if (init.isReferrerSet) {
        // 1. Let |referrer| be |init|'s referrer member.
        if (init.referrer.referrer.isEmpty()) {
            // 2. if |referrer| is the empty string, set |request|'s referrer to
            // "no-referrer" and terminate these substeps.
            request->setReferrerString(FetchRequestData::noReferrerString());
        } else {
            // 3. Let |parsedReferrer| be the result of parsing |referrer| with
            // |baseURL|.
            KURL parsedReferrer = scriptState->executionContext()->completeURL(init.referrer.referrer);
            if (!parsedReferrer.isValid()) {
                // 4. If |parsedReferrer| is failure, throw a TypeError.
                exceptionState.throwTypeError("Referrer '" + init.referrer.referrer + "' is not a valid URL.");
                return nullptr;
            }
            if (parsedReferrer.protocolIsAbout() && parsedReferrer.host().isEmpty() && parsedReferrer.path() == "client") {
                // 5. If |parsedReferrer|'s non-relative flag is set, scheme is
                // "about", and path contains a single string "client", set
                // request's referrer to "client" and terminate these substeps.
                request->setReferrerString(FetchRequestData::clientReferrerString());
            } else if (!origin->isSameSchemeHostPortAndSuborigin(SecurityOrigin::create(parsedReferrer).get())) {
                // 6. If |parsedReferrer|'s origin is not same origin with
                // |origin|, throw a TypeError.
                exceptionState.throwTypeError("The origin of '" + init.referrer.referrer + "' should be same as '" + origin->toString() + "'");
                return nullptr;
            } else {
                // 7. Set |request|'s referrer to |parsedReferrer|.
                request->setReferrerString(AtomicString(parsedReferrer.string()));
            }
        }
        request->setReferrerPolicy(init.referrer.referrerPolicy);
    }


    // "17. Let |mode| be |init|'s mode member if it is present, and
    // |fallbackMode| otherwise."
    // "18. If |mode| is non-null, set |request|'s mode to |mode|."
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

    // "19. Let |credentials| be |init|'s credentials member if it is present,
    // and |fallbackCredentials| otherwise."
    // "20. If |credentials| is non-null, set |request|'s credentials mode to
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

    // TODO(yhirano): "21. If |init|'s cache member is present, set |request|'s
    // cache mode to it."

    // "22. If |init|'s redirect member is present, set |request|'s redirect
    // mode to it."
    if (init.redirect == "follow") {
        request->setRedirect(WebURLRequest::FetchRedirectModeFollow);
    } else if (init.redirect == "error") {
        request->setRedirect(WebURLRequest::FetchRedirectModeError);
    } else if (init.redirect == "manual") {
        request->setRedirect(WebURLRequest::FetchRedirectModeManual);
    }

    // "If |init|'s integrity member is present, set |request|'s
    // integrity metadata to it."
    if (!init.integrity.isNull())
        request->setIntegrity(init.integrity);

    // "24. If |init|'s method member is present, let |method| be it and run
    // these substeps:"
    if (!init.method.isNull()) {
        // "1. If |method| is not a method or method is a forbidden method,
        // throw a TypeError."
        if (!isValidHTTPToken(init.method)) {
            exceptionState.throwTypeError("'" + init.method + "' is not a valid HTTP method.");
            return nullptr;
        }
        if (FetchUtils::isForbiddenMethod(init.method)) {
            exceptionState.throwTypeError("'" + init.method + "' HTTP method is unsupported.");
            return nullptr;
        }
        // "2. Normalize |method|."
        // "3. Set |request|'s method to |method|."
        request->setMethod(FetchUtils::normalizeMethod(AtomicString(init.method)));
    }
    // "25. Let |r| be a new Request object associated with |request| and a new
    // Headers object whose guard is request."
    Request* r = Request::create(scriptState->executionContext(), request);
    // "26. Let |headers| be a copy of |r|'s Headers object."
    // "27. If |init|'s headers member is present, set |headers| to |init|'s
    // headers member."
    // We don't create a copy of r's Headers object when init's headers member
    // is present.
    Headers* headers = nullptr;
    if (!init.headers && init.headersDictionary.isUndefinedOrNull()) {
        headers = r->headers()->clone();
    }
    // "28. Empty |r|'s request's header list."
    r->m_request->headerList()->clearList();
    // "29. If |r|'s request's mode is no CORS, run these substeps:
    if (r->request()->mode() == WebURLRequest::FetchRequestModeNoCORS) {
        // "1. If |r|'s request's method is not a simple method, throw a
        // TypeError."
        if (!FetchUtils::isSimpleMethod(r->request()->method())) {
            exceptionState.throwTypeError("'" + r->request()->method() + "' is unsupported in no-cors mode.");
            return nullptr;
        }
        // "2. If |request|'s integrity metadata is not the empty string, throw
        // a TypeError."
        if (!request->integrity().isEmpty()) {
            exceptionState.throwTypeError("The integrity attribute is unsupported in no-cors mode.");
            return nullptr;
        }
        // "3. Set |r|'s Headers object's guard to |request-no-CORS|.
        r->headers()->setGuard(Headers::RequestNoCORSGuard);
    }
    // "30. Fill |r|'s Headers object with |headers|. Rethrow any exceptions."
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
        return nullptr;

    // "31. If either |init|'s body member is present or |temporaryBody| is
    // non-null, and |request|'s method is `GET` or `HEAD`, throw a TypeError.
    if (init.body || temporaryBody) {
        if (request->method() == "GET" || request->method() == "HEAD") {
            exceptionState.throwTypeError("Request with GET/HEAD method cannot have body.");
            return nullptr;
        }
    }

    // "32. If |init|'s body member is present, run these substeps:"
    if (init.body) {
        // "1. Let |stream| and |Content-Type| be the result of extracting
        //  |init|'s body member."
        // "2. Set |temporaryBody| to |stream|.
        // "3. If |Content-Type| is non-null and |r|'s request's header list
        //  contains no header named `Content-Type`, append
        //  `Content-Type`/|Content-Type| to |r|'s Headers object. Rethrow any
        //  exception."
        temporaryBody = new BodyStreamBuffer(init.body.release());
        if (!init.contentType.isEmpty() && !r->headers()->has("Content-Type", exceptionState)) {
            r->headers()->append("Content-Type", init.contentType, exceptionState);
        }
        if (exceptionState.hadException())
            return nullptr;
    }

    // "33. Set |r|'s body to |temporaryBody|.
    if (temporaryBody)
        r->m_request->setBuffer(temporaryBody);

    // https://w3c.github.io/webappsec/specs/credentialmanagement/#monkey-patching-fetch-2
    if (init.opaque || (inputRequest && inputRequest->opaque()))
        r->makeOpaque();

    // "34. Set |r|'s MIME type to the result of extracting a MIME type from
    // |r|'s request's header list."
    r->m_request->setMIMEType(r->m_request->headerList()->extractMIMEType());

    // "35. If |input| is a Request object and |input|'s body is non-null, run
    // these substeps:"
    // We set bodyUsed even when the body is null in spite of the
    // spec. See https://github.com/whatwg/fetch/issues/61 for details.
    if (inputRequest) {
        // "1. Set |input|'s body to null."
        inputRequest->m_request->setBuffer(nullptr);
        // "2. Set |input|'s used flag."
        inputRequest->setBodyPassed();
    }

    // "36. Return |r|."
    return r;
}

Request* Request::create(ScriptState* scriptState, const RequestInfo& input, const Dictionary& init, ExceptionState& exceptionState)
{
    ASSERT(!input.isNull());
    if (input.isUSVString())
        return create(scriptState, input.getAsUSVString(), init, exceptionState);
    return create(scriptState, input.getAsRequest(), init, exceptionState);
}

Request* Request::create(ScriptState* scriptState, const String& input, ExceptionState& exceptionState)
{
    return create(scriptState, input, Dictionary(), exceptionState);
}

Request* Request::create(ScriptState* scriptState, const String& input, const Dictionary& init, ExceptionState& exceptionState)
{
    RequestInit requestInit(scriptState->executionContext(), init, exceptionState);
    return createRequestWithRequestOrString(scriptState, nullptr, input, requestInit, exceptionState);
}

Request* Request::create(ScriptState* scriptState, Request* input, ExceptionState& exceptionState)
{
    return create(scriptState, input, Dictionary(), exceptionState);
}

Request* Request::create(ScriptState* scriptState, Request* input, const Dictionary& init, ExceptionState& exceptionState)
{
    RequestInit requestInit(scriptState->executionContext(), init, exceptionState);
    return createRequestWithRequestOrString(scriptState, input, String(), requestInit, exceptionState);
}

Request* Request::create(ExecutionContext* context, FetchRequestData* request)
{
    return new Request(context, request);
}

Request::Request(ExecutionContext* context, FetchRequestData* request)
    : Body(context)
    , m_request(request)
    , m_headers(Headers::create(m_request->headerList()))
{
    m_headers->setGuard(Headers::RequestGuard);
}

Request::Request(ExecutionContext* context, FetchRequestData* request, Headers* headers)
    : Body(context) , m_request(request) , m_headers(headers) {}

Request* Request::create(ExecutionContext* context, const WebServiceWorkerRequest& webRequest)
{
    return new Request(context, webRequest);
}

Request::Request(ExecutionContext* context, const WebServiceWorkerRequest& webRequest)
    : Body(context)
    , m_request(FetchRequestData::create(context, webRequest))
    , m_headers(Headers::create(m_request->headerList()))
{
    m_headers->setGuard(Headers::RequestGuard);
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

String Request::context() const
{
    // "The context attribute's getter must return request's context"
    switch (m_request->context()) {
    case WebURLRequest::RequestContextUnspecified:
        return "";
    case WebURLRequest::RequestContextAudio:
        return "audio";
    case WebURLRequest::RequestContextBeacon:
        return "beacon";
    case WebURLRequest::RequestContextCSPReport:
        return "cspreport";
    case WebURLRequest::RequestContextDownload:
        return "download";
    case WebURLRequest::RequestContextEmbed:
        return "embed";
    case WebURLRequest::RequestContextEventSource:
        return "eventsource";
    case WebURLRequest::RequestContextFavicon:
        return "favicon";
    case WebURLRequest::RequestContextFetch:
        return "fetch";
    case WebURLRequest::RequestContextFont:
        return "font";
    case WebURLRequest::RequestContextForm:
        return "form";
    case WebURLRequest::RequestContextFrame:
        return "frame";
    case WebURLRequest::RequestContextHyperlink:
        return "hyperlink";
    case WebURLRequest::RequestContextIframe:
        return "iframe";
    case WebURLRequest::RequestContextImage:
        return "image";
    case WebURLRequest::RequestContextImageSet:
        return "imageset";
    case WebURLRequest::RequestContextImport:
        return "import";
    case WebURLRequest::RequestContextInternal:
        return "internal";
    case WebURLRequest::RequestContextLocation:
        return "location";
    case WebURLRequest::RequestContextManifest:
        return "manifest";
    case WebURLRequest::RequestContextObject:
        return "object";
    case WebURLRequest::RequestContextPing:
        return "ping";
    case WebURLRequest::RequestContextPlugin:
        return "plugin";
    case WebURLRequest::RequestContextPrefetch:
        return "prefetch";
    case WebURLRequest::RequestContextScript:
        return "script";
    case WebURLRequest::RequestContextServiceWorker:
        return "serviceworker";
    case WebURLRequest::RequestContextSharedWorker:
        return "sharedworker";
    case WebURLRequest::RequestContextSubresource:
        return "subresource";
    case WebURLRequest::RequestContextStyle:
        return "style";
    case WebURLRequest::RequestContextTrack:
        return "track";
    case WebURLRequest::RequestContextVideo:
        return "video";
    case WebURLRequest::RequestContextWorker:
        return "worker";
    case WebURLRequest::RequestContextXMLHttpRequest:
        return "xmlhttprequest";
    case WebURLRequest::RequestContextXSLT:
        return "xslt";
    }
    ASSERT_NOT_REACHED();
    return "";
}

String Request::referrer() const
{
    // "The referrer attribute's getter must return the empty string if
    // request's referrer is no referrer, "about:client" if request's referrer
    // is client and request's referrer, serialized, otherwise."
    ASSERT(FetchRequestData::noReferrerString() == AtomicString());
    ASSERT(FetchRequestData::clientReferrerString() == AtomicString("about:client"));
    return m_request->referrerString();
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

String Request::redirect() const
{
    // "The redirect attribute's getter must return request's redirect mode."
    switch (m_request->redirect()) {
    case WebURLRequest::FetchRedirectModeFollow:
        return "follow";
    case WebURLRequest::FetchRedirectModeError:
        return "error";
    case WebURLRequest::FetchRedirectModeManual:
        return "manual";
    }
    ASSERT_NOT_REACHED();
    return "";
}

String Request::integrity() const
{
    return m_request->integrity();
}

Request* Request::clone(ExceptionState& exceptionState)
{
    if (bodyUsed()) {
        exceptionState.throwTypeError("Request body is already used");
        return nullptr;
    }

    FetchRequestData* request = m_request->clone(executionContext());
    Headers* headers = Headers::create(request->headerList());
    headers->setGuard(m_headers->guard());
    return new Request(executionContext(), request, headers);
}

FetchRequestData* Request::passRequestData()
{
    ASSERT(!bodyUsed());
    setBodyPassed();
    FetchRequestData* newRequestData = m_request->pass(executionContext());
    return newRequestData;
}

bool Request::hasBody() const
{
    return bodyBuffer();
}

void Request::populateWebServiceWorkerRequest(WebServiceWorkerRequest& webRequest) const
{
    webRequest.setMethod(method());
    webRequest.setRequestContext(m_request->context());
    // This strips off the fragment part.
    webRequest.setURL(url());

    const FetchHeaderList* headerList = m_headers->headerList();
    for (size_t i = 0, size = headerList->size(); i < size; ++i) {
        const FetchHeaderList::Header& header = headerList->entry(i);
        webRequest.appendHeader(header.first, header.second);
    }

    webRequest.setReferrer(m_request->referrerString(), static_cast<WebReferrerPolicy>(m_request->referrerPolicy()));
    // FIXME: How can we set isReload properly? What is the correct place to load it in to the Request object? We should investigate the right way
    // to plumb this information in to here.
}

String Request::mimeType() const
{
    return m_request->mimeType();
}

DEFINE_TRACE(Request)
{
    Body::trace(visitor);
    visitor->trace(m_request);
    visitor->trace(m_headers);
}

} // namespace blink

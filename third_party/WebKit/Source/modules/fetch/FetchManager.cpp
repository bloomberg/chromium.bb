// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/fetch/FetchManager.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/ExceptionCode.h"
#include "core/fetch/FetchUtils.h"
#include "core/fileapi/Blob.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "modules/fetch/Body.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/FetchRequestData.h"
#include "modules/fetch/Response.h"
#include "modules/fetch/ResponseInit.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/HashSet.h"

namespace blink {

class FetchManager::Loader final : public NoBaseWillBeGarbageCollectedFinalized<FetchManager::Loader>, public ThreadableLoaderClient, public ContextLifecycleObserver {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(FetchManager::Loader);
public:
    static PassOwnPtrWillBeRawPtr<Loader> create(ExecutionContext* executionContext, FetchManager* fetchManager, PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver, const FetchRequestData* request)
    {
        return adoptPtrWillBeNoop(new Loader(executionContext, fetchManager, resolver, request));
    }

    ~Loader() override;
    DECLARE_VIRTUAL_TRACE();

    void didReceiveResponse(unsigned long, const ResourceResponse&, PassOwnPtr<WebDataConsumerHandle>) override;
    void didReceiveData(const char*, unsigned) override;
    void didFinishLoading(unsigned long, double) override;
    void didFail(const ResourceError&) override;
    void didFailAccessControlCheck(const ResourceError&) override;
    void didFailRedirectCheck() override;

    void start();
    void dispose();

private:
    Loader(ExecutionContext*, FetchManager*, PassRefPtrWillBeRawPtr<ScriptPromiseResolver>, const FetchRequestData*);

    void performBasicFetch();
    void performNetworkError(const String& message);
    void performHTTPFetch(bool corsFlag, bool corsPreflightFlag);
    void failed(const String& message);
    void notifyFinished();

    RawPtrWillBeMember<FetchManager> m_fetchManager;
    RefPtrWillBeMember<ScriptPromiseResolver> m_resolver;
    PersistentWillBeMember<FetchRequestData> m_request;
    PersistentWillBeMember<BodyStreamBuffer> m_responseBuffer;
    RefPtr<ThreadableLoader> m_loader;
    bool m_failed;
    bool m_finished;
};

FetchManager::Loader::Loader(ExecutionContext* executionContext, FetchManager* fetchManager, PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver, const FetchRequestData* request)
    : ContextLifecycleObserver(executionContext)
    , m_fetchManager(fetchManager)
    , m_resolver(resolver)
    , m_request(request->createCopy())
    , m_failed(false)
    , m_finished(false)
{
}

FetchManager::Loader::~Loader()
{
    ASSERT(!m_loader);
}

DEFINE_TRACE(FetchManager::Loader)
{
    visitor->trace(m_fetchManager);
    visitor->trace(m_resolver);
    visitor->trace(m_request);
    visitor->trace(m_responseBuffer);
    ContextLifecycleObserver::trace(visitor);
}

void FetchManager::Loader::didReceiveResponse(unsigned long, const ResourceResponse& response, PassOwnPtr<WebDataConsumerHandle> handle)
{
    // FIXME: Use |handle|.
    ASSERT_UNUSED(handle, !handle);
    // Recompute the tainting if the request was redirected to a different
    // origin.
    if (!SecurityOrigin::create(response.url())->isSameSchemeHostPort(m_request->origin().get())) {
        switch (m_request->mode()) {
        case WebURLRequest::FetchRequestModeSameOrigin:
            ASSERT_NOT_REACHED();
            break;
        case WebURLRequest::FetchRequestModeNoCORS:
            m_request->setResponseTainting(FetchRequestData::OpaqueTainting);
            break;
        case WebURLRequest::FetchRequestModeCORS:
        case WebURLRequest::FetchRequestModeCORSWithForcedPreflight:
            m_request->setResponseTainting(FetchRequestData::CORSTainting);
            break;
        }
    }
    m_responseBuffer = new BodyStreamBuffer();
    FetchResponseData* responseData = FetchResponseData::createWithBuffer(m_responseBuffer);
    responseData->setStatus(response.httpStatusCode());
    responseData->setStatusMessage(response.httpStatusText());
    for (auto& it : response.httpHeaderFields())
        responseData->headerList()->append(it.key, it.value);
    responseData->setURL(response.url());
    responseData->setContentTypeForBuffer(response.mimeType());

    FetchResponseData* taintedResponse = responseData;
    switch (m_request->tainting()) {
    case FetchRequestData::BasicTainting:
        taintedResponse = responseData->createBasicFilteredResponse();
        break;
    case FetchRequestData::CORSTainting:
        taintedResponse = responseData->createCORSFilteredResponse();
        break;
    case FetchRequestData::OpaqueTainting:
        taintedResponse = responseData->createOpaqueFilteredResponse();
        break;
    }
    m_resolver->resolve(Response::create(m_resolver->executionContext(), taintedResponse));
    m_resolver.clear();
}

void FetchManager::Loader::didReceiveData(const char* data, unsigned size)
{
    m_responseBuffer->write(DOMArrayBuffer::create(data, size));
}

void FetchManager::Loader::didFinishLoading(unsigned long, double)
{
    ASSERT(m_responseBuffer);
    ASSERT(!m_failed);
    m_responseBuffer->close();
    m_responseBuffer.clear();
    m_finished = true;
    notifyFinished();
}

void FetchManager::Loader::didFail(const ResourceError& error)
{
    failed("Fetch API cannot load " + error.failingURL() + ". " + error.localizedDescription());
}

void FetchManager::Loader::didFailAccessControlCheck(const ResourceError& error)
{
    failed("Fetch API cannot load " + error.failingURL() + ". " + error.localizedDescription());
}

void FetchManager::Loader::didFailRedirectCheck()
{
    failed("Fetch API cannot load " + m_request->url().string() + ". Redirect failed.");
}

void FetchManager::Loader::start()
{
    if (!executionContext()->isServiceWorkerGlobalScope()
        && m_request->mode() == WebURLRequest::FetchRequestModeNoCORS) {
        performNetworkError("no-cors is disabled for non-serviceworker.");
        return;
    }

    // "1. If |request|'s url contains a Known HSTS Host, modify it per the
    // requirements of the 'URI [sic] Loading and Port Mapping' chapter of HTTP
    // Strict Transport Security."
    // FIXME: Implement this.

    // "2. If |request|'s referrer is not none, set |request|'s referrer to the
    // result of invoking determine |request|'s referrer."
    // We set the referrer using workerGlobalScope's URL in
    // WorkerThreadableLoader.

    // "3. If |request|'s synchronous flag is unset and fetch is not invoked
    // recursively, run the remaining steps asynchronously."
    // We don't support synchronous flag.

    // "4. Let response be the value corresponding to the first matching
    // statement:"

    // "- should fetching |request| be blocked as mixed content returns blocked"
    // We do mixed content checking in ResourceFetcher.

    // "- should fetching |request| be blocked as content security returns
    //    blocked"
    if (!ContentSecurityPolicy::shouldBypassMainWorld(executionContext()) && !executionContext()->contentSecurityPolicy()->allowConnectToSource(m_request->url())) {
        // "A network error."
        performNetworkError("Refused to connect to '" + m_request->url().elidedString() + "' because it violates the document's Content Security Policy.");
        return;
    }

    // "- |request|'s url's origin is |request|'s origin and the |CORS flag| is
    //    unset"
    // "- |request|'s url's scheme is 'data' and |request|'s same-origin data
    //    URL flag is set"
    // "- |request|'s url's scheme is 'about'"
    // Note we don't support to call this method with |CORS flag|.
    if ((SecurityOrigin::create(m_request->url())->isSameSchemeHostPort(m_request->origin().get()))
        || (m_request->url().protocolIsData() && m_request->sameOriginDataURLFlag())
        || (m_request->url().protocolIsAbout())) {
        // "The result of performing a basic fetch using request."
        performBasicFetch();
        return;
    }

    // "- |request|'s mode is |same-origin|"
    if (m_request->mode() == WebURLRequest::FetchRequestModeSameOrigin) {
        // "A network error."
        performNetworkError("Fetch API cannot load " + m_request->url().string() + ". Request mode is \"same-origin\" but the URL\'s origin is not same as the request origin " + m_request->origin()->toString() + ".");
        return;
    }

    // "- |request|'s mode is |no CORS|"
    if (m_request->mode() == WebURLRequest::FetchRequestModeNoCORS) {
        // "Set |request|'s response tainting to |opaque|."
        m_request->setResponseTainting(FetchRequestData::OpaqueTainting);
        // "The result of performing a basic fetch using |request|."
        performBasicFetch();
        return;
    }

    // "- |request|'s url's scheme is not one of 'http' and 'https'"
    if (!m_request->url().protocolIsInHTTPFamily()) {
        // "A network error."
        performNetworkError("Fetch API cannot load " + m_request->url().string() + ". URL scheme must be \"http\" or \"https\" for CORS request.");
        return;
    }

    // "- |request|'s mode is |CORS-with-forced-preflight|.
    // "- |request|'s unsafe request flag is set and either |request|'s method
    // is not a simple method or a header in |request|'s header list is not a
    // simple header"
    if (m_request->mode() == WebURLRequest::FetchRequestModeCORSWithForcedPreflight
        || (m_request->unsafeRequestFlag()
            && (!FetchUtils::isSimpleMethod(m_request->method())
                || m_request->headerList()->containsNonSimpleHeader()))) {
        // "Set |request|'s response tainting to |CORS|."
        m_request->setResponseTainting(FetchRequestData::CORSTainting);
        // "The result of performing an HTTP fetch using |request| with the
        // |CORS flag| and |CORS preflight flag| set."
        performHTTPFetch(true, true);
        return;
    }

    // "- Otherwise
    //     Set |request|'s response tainting to |CORS|."
    m_request->setResponseTainting(FetchRequestData::CORSTainting);
    // "The result of performing an HTTP fetch using |request| with the
    // |CORS flag| set."
    performHTTPFetch(true, false);
}

void FetchManager::Loader::dispose()
{
    // Prevent notification
    m_fetchManager = nullptr;
    if (m_loader) {
        m_loader->cancel();
        m_loader.clear();
    }
}

void FetchManager::Loader::performBasicFetch()
{
    // "To perform a basic fetch using |request|, switch on |request|'s url's
    // scheme, and run the associated steps:"
    if (m_request->url().protocolIsInHTTPFamily()) {
        // "Return the result of performing an HTTP fetch using |request|."
        performHTTPFetch(false, false);
    } else {
        // FIXME: implement other protocols.
        performNetworkError("Fetch API cannot load " + m_request->url().string() + ". URL scheme \"" + m_request->url().protocol() + "\" is not supported.");
    }
}

void FetchManager::Loader::performNetworkError(const String& message)
{
    failed(message);
}

void FetchManager::Loader::performHTTPFetch(bool corsFlag, bool corsPreflightFlag)
{
    ASSERT(m_request->url().protocolIsInHTTPFamily());
    // CORS preflight fetch procedure is implemented inside DocumentThreadableLoader.

    // "1. Let |HTTPRequest| be a copy of |request|, except that |HTTPRequest|'s
    //  body is a tee of |request|'s body."
    // We use ResourceRequest class for HTTPRequest.
    // FIXME: Support body.
    ResourceRequest request(m_request->url());
    request.setRequestContext(WebURLRequest::RequestContextFetch);
    request.setHTTPMethod(m_request->method());
    const Vector<OwnPtr<FetchHeaderList::Header>>& list = m_request->headerList()->list();
    for (size_t i = 0; i < list.size(); ++i) {
        request.addHTTPHeaderField(AtomicString(list[i]->first), AtomicString(list[i]->second));
    }

    if (m_request->method() != "GET" && m_request->method() != "HEAD") {
        RefPtr<BlobDataHandle> blobDataHandle = m_request->blobDataHandle();
        if (blobDataHandle.get()) {
            RefPtr<FormData> httpBody(FormData::create());
            httpBody->appendBlob(blobDataHandle->uuid(), blobDataHandle);
            request.setHTTPBody(httpBody);
        }
    }

    // "2. Append `Referer`/empty byte sequence, if |HTTPRequest|'s |referrer|
    // is none, and `Referer`/|HTTPRequest|'s referrer, serialized and utf-8
    // encoded, otherwise, to HTTPRequest's header list.
    // We set the referrer using workerGlobalScope's URL in
    // WorkerThreadableLoader.

    // "3. Append `Host`, ..."
    // FIXME: Implement this when the spec is fixed.

    // "4.If |HTTPRequest|'s force Origin header flag is set, append `Origin`/
    // |HTTPRequest|'s origin, serialized and utf-8 encoded, to |HTTPRequest|'s
    // header list."
    // We set Origin header in updateRequestForAccessControl() called from
    // DocumentThreadableLoader::makeCrossOriginAccessRequest

    // "5. Let |credentials flag| be set if either |HTTPRequest|'s credentials
    // mode is |include|, or |HTTPRequest|'s credentials mode is |same-origin|
    // and the |CORS flag| is unset, and unset otherwise.
    ResourceLoaderOptions resourceLoaderOptions;
    resourceLoaderOptions.dataBufferingPolicy = DoNotBufferData;
    if (m_request->credentials() == WebURLRequest::FetchCredentialsModeInclude
        || (m_request->credentials() == WebURLRequest::FetchCredentialsModeSameOrigin && !corsFlag)) {
        resourceLoaderOptions.allowCredentials = AllowStoredCredentials;
    }
    if (m_request->credentials() == WebURLRequest::FetchCredentialsModeInclude)
        resourceLoaderOptions.credentialsRequested = ClientRequestedCredentials;
    resourceLoaderOptions.securityOrigin = m_request->origin().get();

    ThreadableLoaderOptions threadableLoaderOptions;
    threadableLoaderOptions.contentSecurityPolicyEnforcement = ContentSecurityPolicy::shouldBypassMainWorld(executionContext()) ? DoNotEnforceContentSecurityPolicy : EnforceConnectSrcDirective;
    if (corsPreflightFlag)
        threadableLoaderOptions.preflightPolicy = ForcePreflight;
    switch (m_request->mode()) {
    case WebURLRequest::FetchRequestModeSameOrigin:
        threadableLoaderOptions.crossOriginRequestPolicy = DenyCrossOriginRequests;
        break;
    case WebURLRequest::FetchRequestModeNoCORS:
        threadableLoaderOptions.crossOriginRequestPolicy = AllowCrossOriginRequests;
        break;
    case WebURLRequest::FetchRequestModeCORS:
    case WebURLRequest::FetchRequestModeCORSWithForcedPreflight:
        threadableLoaderOptions.crossOriginRequestPolicy = UseAccessControl;
        break;
    }
    m_loader = ThreadableLoader::create(*executionContext(), this, request, threadableLoaderOptions, resourceLoaderOptions);
    if (!m_loader)
        performNetworkError("Can't create ThreadableLoader");
}

void FetchManager::Loader::failed(const String& message)
{
    if (m_failed || m_finished)
        return;
    m_failed = true;
    executionContext()->addConsoleMessage(ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, message));
    if (m_responseBuffer) {
        m_responseBuffer->error(DOMException::create(NetworkError, "Failed to fetch"));
        m_responseBuffer.clear();
    } else if (m_resolver) {
        if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
            return;
        ScriptState* state = m_resolver->scriptState();
        ScriptState::Scope scope(state);
        m_resolver->reject(V8ThrowException::createTypeError(state->isolate(), "Failed to fetch"));
    }
    notifyFinished();
}

void FetchManager::Loader::notifyFinished()
{
    if (m_fetchManager)
        m_fetchManager->onLoaderFinished(this);
}

FetchManager::FetchManager(ExecutionContext* executionContext)
    : m_executionContext(executionContext)
    , m_isStopped(false)
{
}

FetchManager::~FetchManager()
{
#if !ENABLE(OILPAN)
    if (!m_isStopped)
        stop();
#endif
}

ScriptPromise FetchManager::fetch(ScriptState* scriptState, const FetchRequestData* request)
{
    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    OwnPtrWillBeRawPtr<Loader> ownLoader = Loader::create(m_executionContext, this, resolver.release(), request);
    Loader* loader = m_loaders.add(ownLoader.release()).storedValue->get();
    loader->start();
    return promise;
}

void FetchManager::stop()
{
    ASSERT(!m_isStopped);
    m_isStopped = true;
    for (auto& loader : m_loaders)
        loader->dispose();
}

void FetchManager::onLoaderFinished(Loader* loader)
{
    // We don't use remove here, because it may cause recursive deletion.
    OwnPtrWillBeRawPtr<Loader> p = m_loaders.take(loader);
    ASSERT(p);
    p->dispose();
}

DEFINE_TRACE(FetchManager)
{
#if ENABLE(OILPAN)
    visitor->trace(m_executionContext);
    visitor->trace(m_loaders);
#endif
}

} // namespace blink

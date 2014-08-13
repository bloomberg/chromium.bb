// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "FetchManager.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/ExceptionCode.h"
#include "core/fetch/FetchUtils.h"
#include "core/fileapi/Blob.h"
#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "modules/serviceworkers/FetchRequestData.h"
#include "modules/serviceworkers/Response.h"
#include "modules/serviceworkers/ResponseInit.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/HashSet.h"

namespace blink {

class FetchManager::Loader : public ThreadableLoaderClient {
public:
    Loader(ExecutionContext*, FetchManager*, PassRefPtr<ScriptPromiseResolver>, PassRefPtrWillBeRawPtr<FetchRequestData>);
    ~Loader();
    virtual void didReceiveResponse(unsigned long, const ResourceResponse&);
    virtual void didFinishLoading(unsigned long, double);
    virtual void didFail(const ResourceError&);
    virtual void didFailAccessControlCheck(const ResourceError&);
    virtual void didFailRedirectCheck();
    virtual void didDownloadData(int);

    void start();
    void cleanup();

private:
    void performBasicFetch();
    void performNetworkError();
    void performHTTPFetch();
    void failed();
    void notifyFinished();

    ExecutionContext* m_executionContext;
    FetchManager* m_fetchManager;
    RefPtr<ScriptPromiseResolver> m_resolver;
    RefPtrWillBePersistent<FetchRequestData> m_request;
    RefPtr<ThreadableLoader> m_loader;
    ResourceResponse m_response;
    long long m_downloadedBlobLength;
    bool m_corsFlag;
    bool m_corsPreflightFlag;
    bool m_failed;
};

FetchManager::Loader::Loader(ExecutionContext* executionContext, FetchManager* fetchManager, PassRefPtr<ScriptPromiseResolver> resolver, PassRefPtrWillBeRawPtr<FetchRequestData> request)
    : m_executionContext(executionContext)
    , m_fetchManager(fetchManager)
    , m_resolver(resolver)
    , m_request(request->createCopy())
    , m_downloadedBlobLength(0)
    , m_corsFlag(false)
    , m_corsPreflightFlag(false)
    , m_failed(false)
{
}

FetchManager::Loader::~Loader()
{
    if (m_loader)
        m_loader->cancel();
}

void FetchManager::Loader::didReceiveResponse(unsigned long, const ResourceResponse& response)
{
    m_response = response;
}

void FetchManager::Loader::didFinishLoading(unsigned long, double)
{
    OwnPtr<BlobData> blobData = BlobData::create();
    String filePath = m_response.downloadedFilePath();
    if (!filePath.isEmpty() && m_downloadedBlobLength) {
        blobData->appendFile(filePath);
        blobData->setContentType(m_response.mimeType());
    }
    RefPtrWillBeRawPtr<FetchResponseData> response(FetchResponseData::create());
    response->setStatus(m_response.httpStatusCode());
    response->setStatusMessage(m_response.httpStatusText());
    HTTPHeaderMap::const_iterator end = m_response.httpHeaderFields().end();
    for (HTTPHeaderMap::const_iterator it = m_response.httpHeaderFields().begin(); it != end; ++it) {
        response->headerList()->append(it->key, it->value);
    }
    response->setBlobDataHandle(BlobDataHandle::create(blobData.release(), m_downloadedBlobLength));
    response->setURL(m_request->url());

    switch (m_request->tainting()) {
    case FetchRequestData::BasicTainting:
        response = response->createBasicFilteredResponse();
        break;
    case FetchRequestData::CORSTainting:
        response = response->createCORSFilteredResponse();
        break;
    case FetchRequestData::OpaqueTainting:
        response = response->createOpaqueFilteredResponse();
        break;
    }
    m_resolver->resolve(Response::create(response.release()));
    notifyFinished();
}

void FetchManager::Loader::didFail(const ResourceError& error)
{
    failed();
}

void FetchManager::Loader::didFailAccessControlCheck(const ResourceError& error)
{
    failed();
}

void FetchManager::Loader::didFailRedirectCheck()
{
    failed();
}

void FetchManager::Loader::didDownloadData(int dataLength)
{
    m_downloadedBlobLength += dataLength;
}

void FetchManager::Loader::start()
{
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

    // "- should fetching |request| be blocked as mixed content returns blocked
    //  - should fetching |request| be blocked as content security returns
    //    blocked
    //      A network error."
    // We do mixed content checking and CSP checking in ResourceFetcher.

    // "- |request|'s url's origin is |request|'s origin and the |CORS flag| is
    //    unset"
    // "- |request|'s url's scheme is 'data' and |request|'s same-origin data
    //    URL flag is set"
    // "- |request|'s url's scheme is 'about'"
    if ((SecurityOrigin::create(m_request->url())->isSameSchemeHostPort(m_request->origin().get()) && !m_corsFlag)
        || (m_request->url().protocolIsData() && m_request->sameOriginDataURLFlag())
        || (m_request->url().protocolIsAbout())) {
        // "The result of performing a basic fetch using request."
        performBasicFetch();
        return;
    }

    // "- |request|'s mode is |same-origin|"
    if (m_request->mode() == FetchRequestData::SameOriginMode) {
        // "A network error."
        performNetworkError();
        return;
    }

    // "- |request|'s mode is |no CORS|"
    if (m_request->mode() == FetchRequestData::NoCORSMode) {
        // "Set |request|'s response tainting to |opaque|."
        m_request->setResponseTainting(FetchRequestData::OpaqueTainting);
        // "The result of performing a basic fetch using |request|."
        performBasicFetch();
        return;
    }

    // "- |request|'s url's scheme is not one of 'http' and 'https'"
    if (!m_request->url().protocolIsInHTTPFamily()) {
        // "A network error."
        performNetworkError();
        return;
    }

    // "- |request|'s mode is |CORS-with-forced-preflight|.
    // "- |request|'s unsafe request flag is set and either |request|'s method
    // is not a simple method or a header in |request|'s header list is not a
    // simple header"
    if (m_request->mode() == FetchRequestData::CORSWithForcedPreflight
        || (m_request->unsafeRequestFlag()
            && (!FetchUtils::isSimpleMethod(m_request->method())
                || m_request->headerList()->containsNonSimpleHeader()))) {
        // "Set |request|'s response tainting to |CORS|."
        m_request->setResponseTainting(FetchRequestData::CORSTainting);
        // "The result of performing an HTTP fetch using |request| with the
        // |CORS flag| and |CORS preflight flag| set."
        m_corsFlag = true;
        m_corsPreflightFlag = true;
        performHTTPFetch();
        return;
    }

    // "- Otherwise
    //     Set |request|'s response tainting to |CORS|."
    m_request->setResponseTainting(FetchRequestData::CORSTainting);
    // "The result of performing an HTTP fetch using |request| with the
    // |CORS flag| set."
    m_corsFlag = true;
    m_corsPreflightFlag = false;
    performHTTPFetch();
}

void FetchManager::Loader::cleanup()
{
    // Prevent notification
    m_fetchManager = 0;

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
        m_corsFlag = false;
        m_corsPreflightFlag = false;
        performHTTPFetch();
    } else {
        // FIXME: implement other protocols.
        performNetworkError();
    }
}

void FetchManager::Loader::performNetworkError()
{
    failed();
}

void FetchManager::Loader::performHTTPFetch()
{
    // CORS preflight fetch procedure is implemented inside DocumentThreadableLoader.

    // "1. Let |HTTPRequest| be a copy of |request|, except that |HTTPRequest|'s
    //  body is a tee of |request|'s body."
    // We use ResourceRequest class for HTTPRequest.
    // FIXME: Support body.
    ResourceRequest request(m_request->url());
    request.setRequestContext(WebURLRequest::RequestContextFetch);
    request.setDownloadToFile(true);
    request.setHTTPMethod(m_request->method());
    const Vector<OwnPtr<FetchHeaderList::Header> >& list = m_request->headerList()->list();
    for (size_t i = 0; i < list.size(); ++i) {
        request.addHTTPHeaderField(AtomicString(list[i]->first), AtomicString(list[i]->second));
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
    if (m_request->credentials() == FetchRequestData::IncludeCredentials
        || (m_request->credentials() == FetchRequestData::SameOriginCredentials && !m_corsFlag)) {
        resourceLoaderOptions.allowCredentials = AllowStoredCredentials;
    }

    ThreadableLoaderOptions threadableLoaderOptions;
    if (m_corsPreflightFlag)
        threadableLoaderOptions.preflightPolicy = ForcePreflight;
    if (m_corsFlag)
        threadableLoaderOptions.crossOriginRequestPolicy = UseAccessControl;
    else
        threadableLoaderOptions.crossOriginRequestPolicy = AllowCrossOriginRequests;


    m_loader = ThreadableLoader::create(*m_executionContext, this, request, threadableLoaderOptions, resourceLoaderOptions);
}

void FetchManager::Loader::failed()
{
    if (m_failed)
        return;
    m_failed = true;
    ScriptState* state = m_resolver->scriptState();
    ScriptState::Scope scope(state);
    m_resolver->reject(V8ThrowException::createTypeError("Failed to fetch", state->isolate()));
    notifyFinished();
}

void FetchManager::Loader::notifyFinished()
{
    if (m_fetchManager)
        m_fetchManager->onLoaderFinished(this);
}

FetchManager::FetchManager(ExecutionContext* executionContext)
    : m_executionContext(executionContext)
{
}

FetchManager::~FetchManager()
{
    for (HashSet<OwnPtr<Loader> >::iterator it = m_loaders.begin(); it != m_loaders.end(); ++it) {
        (*it)->cleanup();
    }
}

ScriptPromise FetchManager::fetch(ScriptState* scriptState, PassRefPtrWillBeRawPtr<FetchRequestData> request)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    OwnPtr<Loader> ownLoader(adoptPtr(new Loader(m_executionContext, this, resolver.release(), request)));
    Loader* loader = m_loaders.add(ownLoader.release()).storedValue->get();
    loader->start();
    return promise;
}

void FetchManager::onLoaderFinished(Loader* loader)
{
    m_loaders.remove(loader);
}

} // namespace blink

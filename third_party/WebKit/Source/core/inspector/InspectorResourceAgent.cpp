/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/inspector/InspectorResourceAgent.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/ScriptCallStack.h"
#include "core/dom/Document.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/fetch/FetchInitiatorInfo.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourceLoader.h"
#include "core/fetch/UniqueIdentifier.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/NetworkResourcesData.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/MixedContentChecker.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/page/Page.h"
#include "core/xmlhttprequest/XMLHttpRequest.h"
#include "platform/JSONValues.h"
#include "platform/blob/BlobData.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceLoadPriority.h"
#include "platform/network/ResourceLoadTiming.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/network/WebSocketHandshakeRequest.h"
#include "platform/network/WebSocketHandshakeResponse.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/CurrentTime.h"
#include "wtf/RefPtr.h"
#include "wtf/text/Base64.h"

typedef blink::InspectorBackendDispatcher::NetworkCommandHandler::GetResponseBodyCallback GetResponseBodyCallback;

namespace blink {

namespace ResourceAgentState {
static const char resourceAgentEnabled[] = "resourceAgentEnabled";
static const char extraRequestHeaders[] = "extraRequestHeaders";
static const char cacheDisabled[] = "cacheDisabled";
static const char userAgentOverride[] = "userAgentOverride";
static const char monitoringXHR[] = "monitoringXHR";
static const char blockedURLs[] = "blockedURLs";
}

namespace {

// Keep in sync with kDevToolsRequestInitiator defined in devtools_network_controller.cc
const char kDevToolsEmulateNetworkConditionsClientId[] = "X-DevTools-Emulate-Network-Conditions-Client-Id";

// Pattern may contain stars ('*') which match to any (possibly empty) string.
// Stars implicitly assumed at the begin/end of pattern.
bool matches(const String& url, const String& pattern)
{
    Vector<String> parts;
    pattern.split("*", parts);
    size_t pos = 0;
    for (const String& part : parts) {
        pos = url.find(part, pos);
        if (pos == kNotFound)
            return false;
        pos += part.length();
    }
    return true;
}

static PassRefPtr<JSONObject> buildObjectForHeaders(const HTTPHeaderMap& headers)
{
    RefPtr<JSONObject> headersObject = JSONObject::create();
    for (const auto& header : headers)
        headersObject->setString(header.key.string(), header.value);
    return headersObject;
}

class InspectorFileReaderLoaderClient final : public FileReaderLoaderClient {
    WTF_MAKE_NONCOPYABLE(InspectorFileReaderLoaderClient);
public:
    InspectorFileReaderLoaderClient(PassRefPtr<BlobDataHandle> blob, PassOwnPtr<TextResourceDecoder> decoder, PassRefPtr<GetResponseBodyCallback> callback)
        : m_blob(blob)
        , m_decoder(decoder)
        , m_callback(callback)
    {
        m_loader = FileReaderLoader::create(FileReaderLoader::ReadByClient, this);
    }

    ~InspectorFileReaderLoaderClient() override { }

    void start(ExecutionContext* executionContext)
    {
        m_rawData = adoptPtr(new ArrayBufferBuilder());
        if (!m_rawData || !m_rawData->isValid()) {
            m_callback->sendFailure("Couldn't allocate buffer");
            dispose();
        }
        m_loader->start(executionContext, m_blob);
    }

    virtual void didStartLoading() { }

    virtual void didReceiveDataForClient(const char* data, unsigned dataLength)
    {
        if (!dataLength)
            return;
        if (!m_rawData->append(data, dataLength)) {
            m_callback->sendFailure("Couldn't extend buffer");
            dispose();
        }
    }

    virtual void didFinishLoading()
    {
        const char* data = static_cast<const char*>(m_rawData->data());
        unsigned dataLength = m_rawData->byteLength();
        if (m_decoder) {
            String text = m_decoder->decode(data, dataLength);
            text = text + m_decoder->flush();
            m_callback->sendSuccess(text, false);
        } else {
            m_callback->sendSuccess(base64Encode(data, dataLength), true);
        }
        dispose();
    }

    virtual void didFail(FileError::ErrorCode)
    {
        m_callback->sendFailure("Couldn't read BLOB");
        dispose();
    }

private:
    void dispose()
    {
        m_rawData.clear();
        delete this;
    }

    RefPtr<BlobDataHandle> m_blob;
    OwnPtr<TextResourceDecoder> m_decoder;
    RefPtr<GetResponseBodyCallback> m_callback;
    OwnPtr<FileReaderLoader> m_loader;
    OwnPtr<ArrayBufferBuilder> m_rawData;
};

KURL urlWithoutFragment(const KURL& url)
{
    KURL result = url;
    result.removeFragmentIdentifier();
    return result;
}

TypeBuilder::Network::Request::MixedContentType::Enum mixedContentTypeForContextType(MixedContentChecker::ContextType contextType)
{
    switch (contextType) {
    case MixedContentChecker::ContextTypeNotMixedContent:
        return TypeBuilder::Network::Request::MixedContentType::None;
    case MixedContentChecker::ContextTypeBlockable:
        return TypeBuilder::Network::Request::MixedContentType::Blockable;
    case MixedContentChecker::ContextTypeOptionallyBlockable:
    case MixedContentChecker::ContextTypeShouldBeBlockable:
        return TypeBuilder::Network::Request::MixedContentType::Optionally_blockable;
    }

    return TypeBuilder::Network::Request::MixedContentType::None;
}

TypeBuilder::Network::ResourcePriority::Enum resourcePriorityJSON(ResourceLoadPriority priority)
{
    switch (priority) {
    case ResourceLoadPriorityVeryLow: return TypeBuilder::Network::ResourcePriority::VeryLow;
    case ResourceLoadPriorityLow: return TypeBuilder::Network::ResourcePriority::Low;
    case ResourceLoadPriorityMedium: return TypeBuilder::Network::ResourcePriority::Medium;
    case ResourceLoadPriorityHigh: return TypeBuilder::Network::ResourcePriority::High;
    case ResourceLoadPriorityVeryHigh: return TypeBuilder::Network::ResourcePriority::VeryHigh;
    case ResourceLoadPriorityUnresolved: break;
    }
    ASSERT_NOT_REACHED();
    return TypeBuilder::Network::ResourcePriority::Medium;
}

TypeBuilder::Network::BlockedReason::Enum buildBlockedReason(ResourceRequestBlockedReason reason)
{
    switch (reason) {
    case ResourceRequestBlockedReasonCSP:
        return TypeBuilder::Network::BlockedReason::Enum::Csp;
    case ResourceRequestBlockedReasonMixedContent:
        return TypeBuilder::Network::BlockedReason::Enum::Mixed_content;
    case ResourceRequestBlockedReasonOrigin:
        return TypeBuilder::Network::BlockedReason::Enum::Origin;
    case ResourceRequestBlockedReasonInspector:
        return TypeBuilder::Network::BlockedReason::Enum::Inspector;
    case ResourceRequestBlockedReasonOther:
        return TypeBuilder::Network::BlockedReason::Enum::Other;
    case ResourceRequestBlockedReasonNone:
    default:
        ASSERT_NOT_REACHED();
        return TypeBuilder::Network::BlockedReason::Enum::Other;
    }
}

} // namespace

void InspectorResourceAgent::restore()
{
    if (m_state->booleanProperty(ResourceAgentState::resourceAgentEnabled, false))
        enable();
}

static PassRefPtr<TypeBuilder::Network::ResourceTiming> buildObjectForTiming(const ResourceLoadTiming& timing)
{
    return TypeBuilder::Network::ResourceTiming::create()
        .setRequestTime(timing.requestTime())
        .setProxyStart(timing.calculateMillisecondDelta(timing.proxyStart()))
        .setProxyEnd(timing.calculateMillisecondDelta(timing.proxyEnd()))
        .setDnsStart(timing.calculateMillisecondDelta(timing.dnsStart()))
        .setDnsEnd(timing.calculateMillisecondDelta(timing.dnsEnd()))
        .setConnectStart(timing.calculateMillisecondDelta(timing.connectStart()))
        .setConnectEnd(timing.calculateMillisecondDelta(timing.connectEnd()))
        .setSslStart(timing.calculateMillisecondDelta(timing.sslStart()))
        .setSslEnd(timing.calculateMillisecondDelta(timing.sslEnd()))
        .setWorkerStart(timing.calculateMillisecondDelta(timing.workerStart()))
        .setWorkerReady(timing.calculateMillisecondDelta(timing.workerReady()))
        .setSendStart(timing.calculateMillisecondDelta(timing.sendStart()))
        .setSendEnd(timing.calculateMillisecondDelta(timing.sendEnd()))
        .setReceiveHeadersEnd(timing.calculateMillisecondDelta(timing.receiveHeadersEnd()))
        .release();
}

static PassRefPtr<TypeBuilder::Network::Request> buildObjectForResourceRequest(const ResourceRequest& request)
{
    RefPtr<TypeBuilder::Network::Request> requestObject = TypeBuilder::Network::Request::create()
        .setUrl(urlWithoutFragment(request.url()).string())
        .setMethod(request.httpMethod())
        .setHeaders(buildObjectForHeaders(request.httpHeaderFields()))
        .setInitialPriority(resourcePriorityJSON(request.priority()));
    if (request.httpBody() && !request.httpBody()->isEmpty()) {
        Vector<char> bytes;
        request.httpBody()->flatten(bytes);
        requestObject->setPostData(String::fromUTF8WithLatin1Fallback(bytes.data(), bytes.size()));
    }
    return requestObject;
}

static PassRefPtr<TypeBuilder::Network::Response> buildObjectForResourceResponse(const ResourceResponse& response)
{
    if (response.isNull())
        return nullptr;

    double status;
    String statusText;
    if (response.resourceLoadInfo() && response.resourceLoadInfo()->httpStatusCode) {
        status = response.resourceLoadInfo()->httpStatusCode;
        statusText = response.resourceLoadInfo()->httpStatusText;
    } else {
        status = response.httpStatusCode();
        statusText = response.httpStatusText();
    }
    RefPtr<JSONObject> headers;
    if (response.resourceLoadInfo() && response.resourceLoadInfo()->responseHeaders.size())
        headers = buildObjectForHeaders(response.resourceLoadInfo()->responseHeaders);
    else
        headers = buildObjectForHeaders(response.httpHeaderFields());

    int64_t encodedDataLength = response.resourceLoadInfo() ? response.resourceLoadInfo()->encodedDataLength : -1;

    TypeBuilder::Security::SecurityState::Enum securityState = TypeBuilder::Security::SecurityState::Unknown;
    switch (response.securityStyle()) {
    case ResourceResponse::SecurityStyleUnknown:
        securityState = TypeBuilder::Security::SecurityState::Unknown;
        break;
    case ResourceResponse::SecurityStyleUnauthenticated:
        securityState = TypeBuilder::Security::SecurityState::Neutral;
        break;
    case ResourceResponse::SecurityStyleAuthenticationBroken:
        securityState = TypeBuilder::Security::SecurityState::Insecure;
        break;
    case ResourceResponse::SecurityStyleWarning:
        securityState = TypeBuilder::Security::SecurityState::Warning;
        break;
    case ResourceResponse::SecurityStyleAuthenticated:
        securityState = TypeBuilder::Security::SecurityState::Secure;
        break;
    }

    RefPtr<TypeBuilder::Network::Response> responseObject = TypeBuilder::Network::Response::create()
        .setUrl(urlWithoutFragment(response.url()).string())
        .setStatus(status)
        .setStatusText(statusText)
        .setHeaders(headers)
        .setMimeType(response.mimeType())
        .setConnectionReused(response.connectionReused())
        .setConnectionId(response.connectionID())
        .setEncodedDataLength(encodedDataLength)
        .setSecurityState(securityState);

    responseObject->setFromDiskCache(response.wasCached());
    responseObject->setFromServiceWorker(response.wasFetchedViaServiceWorker());
    if (response.resourceLoadTiming())
        responseObject->setTiming(buildObjectForTiming(*response.resourceLoadTiming()));

    if (response.resourceLoadInfo()) {
        if (!response.resourceLoadInfo()->responseHeadersText.isEmpty())
            responseObject->setHeadersText(response.resourceLoadInfo()->responseHeadersText);
        if (response.resourceLoadInfo()->requestHeaders.size())
            responseObject->setRequestHeaders(buildObjectForHeaders(response.resourceLoadInfo()->requestHeaders));
        if (!response.resourceLoadInfo()->requestHeadersText.isEmpty())
            responseObject->setRequestHeadersText(response.resourceLoadInfo()->requestHeadersText);
    }

    AtomicString remoteIPAddress = response.remoteIPAddress();
    if (!remoteIPAddress.isEmpty()) {
        responseObject->setRemoteIPAddress(remoteIPAddress);
        responseObject->setRemotePort(response.remotePort());
    }

    String protocol;
    if (response.resourceLoadInfo())
        protocol = response.resourceLoadInfo()->npnNegotiatedProtocol;
    if (protocol.isEmpty() || protocol == "unknown") {
        if (response.wasFetchedViaSPDY()) {
            protocol = "spdy";
        } else if (response.isHTTP()) {
            protocol = "http";
            if (response.httpVersion() == ResourceResponse::HTTPVersion::HTTPVersion_0_9)
                protocol = "http/0.9";
            else if (response.httpVersion() == ResourceResponse::HTTPVersion::HTTPVersion_1_0)
                protocol = "http/1.0";
            else if (response.httpVersion() == ResourceResponse::HTTPVersion::HTTPVersion_1_1)
                protocol = "http/1.1";
        } else {
            protocol = response.url().protocol();
        }
    }
    responseObject->setProtocol(protocol);

    if (response.securityStyle() != ResourceResponse::SecurityStyleUnknown
        && response.securityStyle() != ResourceResponse::SecurityStyleUnauthenticated) {

        const ResourceResponse::SecurityDetails* responseSecurityDetails = response.securityDetails();

        int numUnknownSCTs = safeCast<int>(responseSecurityDetails->numUnknownSCTs);
        int numInvalidSCTs = safeCast<int>(responseSecurityDetails->numInvalidSCTs);
        int numValidSCTs = safeCast<int>(responseSecurityDetails->numValidSCTs);

        RefPtr<TypeBuilder::Network::CertificateValidationDetails> certificateValidationDetails = TypeBuilder::Network::CertificateValidationDetails::create()
            .setNumUnknownScts(numUnknownSCTs)
            .setNumInvalidScts(numInvalidSCTs)
            .setNumValidScts(numValidSCTs);

        RefPtr<TypeBuilder::Network::SecurityDetails> securityDetails = TypeBuilder::Network::SecurityDetails::create()
            .setProtocol(responseSecurityDetails->protocol)
            .setKeyExchange(responseSecurityDetails->keyExchange)
            .setCipher(responseSecurityDetails->cipher)
            .setCertificateId(responseSecurityDetails->certID);
        securityDetails->setCertificateValidationDetails(certificateValidationDetails);
        if (responseSecurityDetails->mac.length() > 0)
            securityDetails->setMac(responseSecurityDetails->mac);

        responseObject->setSecurityDetails(securityDetails);
    }

    return responseObject;
}

InspectorResourceAgent::~InspectorResourceAgent()
{
#if !ENABLE(OILPAN)
    if (m_state->booleanProperty(ResourceAgentState::resourceAgentEnabled, false)) {
        ErrorString error;
        disable(&error);
    }
    ASSERT(!m_instrumentingAgents->inspectorResourceAgent());
#endif
}

DEFINE_TRACE(InspectorResourceAgent)
{
    visitor->trace(m_inspectedFrames);
    visitor->trace(m_resourcesData);
    visitor->trace(m_replayXHRs);
    visitor->trace(m_replayXHRsToBeDeleted);
    visitor->trace(m_pendingXHRReplayData);
    InspectorBaseAgent::trace(visitor);
}

bool InspectorResourceAgent::shouldBlockRequest(const ResourceRequest& request)
{
    RefPtr<JSONObject> blockedURLs = m_state->getObject(ResourceAgentState::blockedURLs);
    if (!blockedURLs)
        return false;
    String url = request.url().string();
    for (const auto& entry : *blockedURLs) {
        if (matches(url, entry.key))
            return true;
    }
    return false;
}

void InspectorResourceAgent::didBlockRequest(LocalFrame* frame, const ResourceRequest& request, DocumentLoader* loader, const FetchInitiatorInfo& initiatorInfo, ResourceRequestBlockedReason reason)
{
    unsigned long identifier = createUniqueIdentifier();
    willSendRequestInternal(frame, identifier, loader, request, ResourceResponse(), initiatorInfo);

    String requestId = IdentifiersFactory::requestId(identifier);
    TypeBuilder::Network::BlockedReason::Enum protocolReason = buildBlockedReason(reason);
    frontend()->loadingFailed(requestId, monotonicallyIncreasingTime(), InspectorPageAgent::resourceTypeJson(m_resourcesData->resourceType(requestId)), String(), nullptr, &protocolReason);
}

void InspectorResourceAgent::willSendRequestInternal(LocalFrame* frame, unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request, const ResourceResponse& redirectResponse, const FetchInitiatorInfo& initiatorInfo)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    String loaderId = IdentifiersFactory::loaderId(loader);
    m_resourcesData->resourceCreated(requestId, loaderId);

    InspectorPageAgent::ResourceType type = InspectorPageAgent::OtherResource;
    if (initiatorInfo.name == FetchInitiatorTypeNames::xmlhttprequest) {
        type = InspectorPageAgent::XHRResource;
        m_resourcesData->setResourceType(requestId, type);
    } else if (initiatorInfo.name == FetchInitiatorTypeNames::document) {
        type = InspectorPageAgent::DocumentResource;
        m_resourcesData->setResourceType(requestId, type);
    }

    String frameId = loader->frame() ? IdentifiersFactory::frameId(loader->frame()) : "";
    RefPtr<TypeBuilder::Network::Initiator> initiatorObject = buildInitiatorObject(loader->frame() ? loader->frame()->document() : 0, initiatorInfo);
    if (initiatorInfo.name == FetchInitiatorTypeNames::document) {
        FrameNavigationInitiatorMap::iterator it = m_frameNavigationInitiatorMap.find(frameId);
        if (it != m_frameNavigationInitiatorMap.end())
            initiatorObject = it->value;
    }

    RefPtr<TypeBuilder::Network::Request> requestInfo(buildObjectForResourceRequest(request));

    requestInfo->setMixedContentType(mixedContentTypeForContextType(MixedContentChecker::contextTypeForInspector(frame, request)));

    TypeBuilder::Page::ResourceType::Enum resourceType = InspectorPageAgent::resourceTypeJson(type);
    frontend()->requestWillBeSent(requestId, frameId, loaderId, urlWithoutFragment(loader->url()).string(), requestInfo.release(), monotonicallyIncreasingTime(), currentTime(), initiatorObject, buildObjectForResourceResponse(redirectResponse), &resourceType);
    if (m_pendingXHRReplayData && !m_pendingXHRReplayData->async())
        frontend()->flush();
}

void InspectorResourceAgent::willSendRequest(LocalFrame* frame, unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse, const FetchInitiatorInfo& initiatorInfo)
{
    // Ignore the request initiated internally.
    if (initiatorInfo.name == FetchInitiatorTypeNames::internal)
        return;

    if (initiatorInfo.name == FetchInitiatorTypeNames::document && loader->substituteData().isValid())
        return;

    RefPtr<JSONObject> headers = m_state->getObject(ResourceAgentState::extraRequestHeaders);
    if (headers) {
        for (const auto& header : *headers) {
            String value;
            if (header.value->asString(&value))
                request.setHTTPHeaderField(AtomicString(header.key), AtomicString(value));
        }
    }

    request.setReportRawHeaders(true);

    if (m_state->booleanProperty(ResourceAgentState::cacheDisabled, false)) {
        request.setCachePolicy(ReloadBypassingCache);
        request.setShouldResetAppCache(true);
    }

    willSendRequestInternal(frame, identifier, loader, request, redirectResponse, initiatorInfo);

    if (!m_hostId.isEmpty())
        request.addHTTPHeaderField(kDevToolsEmulateNetworkConditionsClientId, AtomicString(m_hostId));
}

void InspectorResourceAgent::markResourceAsCached(unsigned long identifier)
{
    frontend()->requestServedFromCache(IdentifiersFactory::requestId(identifier));
}

bool isResponseEmpty(PassRefPtr<TypeBuilder::Network::Response> response)
{
    if (!response)
        return true;

    RefPtr<JSONValue> status = response->get("status");
    RefPtr<JSONValue> mimeType = response->get("mimeType");
    RefPtr<JSONObject> headers = response->getObject("headers");

    return !status && !mimeType && (!headers || !headers->size());
}

void InspectorResourceAgent::didReceiveResourceResponse(LocalFrame* frame, unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    RefPtr<TypeBuilder::Network::Response> resourceResponse = buildObjectForResourceResponse(response);

    bool isNotModified = response.httpStatusCode() == 304;

    Resource* cachedResource = 0;
    if (resourceLoader && !isNotModified)
        cachedResource = resourceLoader->cachedResource();
    if (!cachedResource)
        cachedResource = InspectorPageAgent::cachedResource(frame, response.url());

    if (cachedResource && resourceResponse && response.mimeType().isEmpty()) {
        // Use mime type from cached resource in case the one in response is empty.
        resourceResponse->setString(TypeBuilder::Network::Response::MimeType, cachedResource->response().mimeType());
    }

    InspectorPageAgent::ResourceType type = cachedResource ? InspectorPageAgent::cachedResourceType(*cachedResource) : InspectorPageAgent::OtherResource;
    // Override with already discovered resource type.
    InspectorPageAgent::ResourceType savedType = m_resourcesData->resourceType(requestId);
    if (savedType == InspectorPageAgent::ScriptResource || savedType == InspectorPageAgent::XHRResource || savedType == InspectorPageAgent::DocumentResource
        || savedType == InspectorPageAgent::FetchResource || savedType == InspectorPageAgent::EventSourceResource) {
        type = savedType;
    }
    if (type == InspectorPageAgent::DocumentResource && loader && loader->substituteData().isValid())
        return;

    // It's only safe to reference a Resource in m_resourcesData if it's in the
    // cache at this point. Resources are added to NetworkResourcesData here
    // and removed in removedResourceFromMemoryCache(), so if the Resource isn't
    // in the cache here, NetworkResourceData's strong reference to the
    // Resource will keep it alive indefinitely.
    if (cachedResource && memoryCache()->contains(cachedResource))
        m_resourcesData->addResource(requestId, cachedResource);
    String frameId = IdentifiersFactory::frameId(frame);
    String loaderId = loader ? IdentifiersFactory::loaderId(loader) : "";
    m_resourcesData->responseReceived(requestId, frameId, response);
    m_resourcesData->setResourceType(requestId, type);

    if (!isResponseEmpty(resourceResponse))
        frontend()->responseReceived(requestId, frameId, loaderId, monotonicallyIncreasingTime(), InspectorPageAgent::resourceTypeJson(type), resourceResponse);
    // If we revalidated the resource and got Not modified, send content length following didReceiveResponse
    // as there will be no calls to didReceiveData from the network stack.
    if (isNotModified && cachedResource && cachedResource->encodedSize())
        didReceiveData(frame, identifier, 0, cachedResource->encodedSize(), 0);
}

static bool isErrorStatusCode(int statusCode)
{
    return statusCode >= 400;
}

void InspectorResourceAgent::didReceiveData(LocalFrame*, unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    String requestId = IdentifiersFactory::requestId(identifier);

    if (data) {
        NetworkResourcesData::ResourceData const* resourceData = m_resourcesData->data(requestId);
        if (resourceData && (!resourceData->cachedResource() || resourceData->cachedResource()->dataBufferingPolicy() == DoNotBufferData || isErrorStatusCode(resourceData->httpStatusCode())))
            m_resourcesData->maybeAddResourceData(requestId, data, dataLength);
    }

    frontend()->dataReceived(requestId, monotonicallyIncreasingTime(), dataLength, encodedDataLength);
}

void InspectorResourceAgent::didFinishLoading(unsigned long identifier, double monotonicFinishTime, int64_t encodedDataLength)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    m_resourcesData->maybeDecodeDataToContent(requestId);
    if (!monotonicFinishTime)
        monotonicFinishTime = monotonicallyIncreasingTime();
    frontend()->loadingFinished(requestId, monotonicFinishTime, encodedDataLength);
}

void InspectorResourceAgent::didReceiveCORSRedirectResponse(LocalFrame* frame, unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    // Update the response and finish loading
    didReceiveResourceResponse(frame, identifier, loader, response, resourceLoader);
    didFinishLoading(identifier, 0, WebURLLoaderClient::kUnknownEncodedDataLength);
}

void InspectorResourceAgent::didFailLoading(unsigned long identifier, const ResourceError& error)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    bool canceled = error.isCancellation();
    frontend()->loadingFailed(requestId, monotonicallyIncreasingTime(), InspectorPageAgent::resourceTypeJson(m_resourcesData->resourceType(requestId)), error.localizedDescription(), canceled ? &canceled : 0, nullptr);
}

void InspectorResourceAgent::scriptImported(unsigned long identifier, const String& sourceString)
{
    m_resourcesData->setResourceContent(IdentifiersFactory::requestId(identifier), sourceString);
}

void InspectorResourceAgent::didReceiveScriptResponse(unsigned long identifier)
{
    m_resourcesData->setResourceType(IdentifiersFactory::requestId(identifier), InspectorPageAgent::ScriptResource);
}

void InspectorResourceAgent::documentThreadableLoaderStartedLoadingForClient(unsigned long identifier, ThreadableLoaderClient* client)
{
    if (!client)
        return;
    ASSERT(client == m_pendingRequest || !m_pendingRequest);
    if (client != m_pendingRequest)
        return;
    m_knownRequestIdMap.set(client, identifier);
    String requestId = IdentifiersFactory::requestId(identifier);
    m_resourcesData->setResourceType(requestId, m_pendingRequestType);
    if (m_pendingRequestType == InspectorPageAgent::XHRResource) {
        m_resourcesData->setXHRReplayData(requestId, m_pendingXHRReplayData.get());
        m_pendingXHRReplayData.clear();
    }
    m_pendingRequest = nullptr;
}

void InspectorResourceAgent::willLoadXHR(XMLHttpRequest* xhr, ThreadableLoaderClient* client, const AtomicString& method, const KURL& url, bool async, PassRefPtr<EncodedFormData> formData, const HTTPHeaderMap& headers, bool includeCredentials)
{
    ASSERT(xhr);
    ASSERT(!m_pendingRequest);
    m_pendingRequest = client;
    m_pendingRequestType = InspectorPageAgent::XHRResource;
    m_pendingXHRReplayData = XHRReplayData::create(xhr->executionContext(), method, urlWithoutFragment(url), async, formData.get(), includeCredentials);
    for (const auto& header : headers)
        m_pendingXHRReplayData->addHeader(header.key, header.value);
}

void InspectorResourceAgent::delayedRemoveReplayXHR(XMLHttpRequest* xhr)
{
    if (!m_replayXHRs.contains(xhr))
        return;

    m_replayXHRsToBeDeleted.add(xhr);
    m_replayXHRs.remove(xhr);
    m_removeFinishedReplayXHRTimer.startOneShot(0, BLINK_FROM_HERE);
}

void InspectorResourceAgent::didFailXHRLoading(ExecutionContext* context, XMLHttpRequest* xhr, ThreadableLoaderClient* client, const AtomicString& method, const String& url)
{
    didFinishXHRInternal(context, xhr, client, method, url, false);
}

void InspectorResourceAgent::didFinishXHRLoading(ExecutionContext* context, XMLHttpRequest* xhr, ThreadableLoaderClient* client, const AtomicString& method, const String& url)
{
    didFinishXHRInternal(context, xhr, client, method, url, true);
}

void InspectorResourceAgent::didFinishXHRInternal(ExecutionContext* context, XMLHttpRequest* xhr, ThreadableLoaderClient* client, const AtomicString& method, const String& url, bool success)
{
    m_pendingRequest = nullptr;
    m_pendingXHRReplayData.clear();

    // This method will be called from the XHR.
    // We delay deleting the replay XHR, as deleting here may delete the caller.
    delayedRemoveReplayXHR(xhr);

    ThreadableLoaderClientRequestIdMap::iterator it = m_knownRequestIdMap.find(client);
    if (it == m_knownRequestIdMap.end())
        return;

    if (m_state->booleanProperty(ResourceAgentState::monitoringXHR, false)) {
        String message = (success ? "XHR finished loading: " : "XHR failed loading: ") + method + " \"" + url + "\".";
        RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(NetworkMessageSource, DebugMessageLevel, message);
        consoleMessage->setRequestIdentifier(it->value);
        m_inspectedFrames->root()->host()->consoleMessageStorage().reportMessage(context, consoleMessage.release());
    }
    m_knownRequestIdMap.remove(client);
}

void InspectorResourceAgent::willStartFetch(ThreadableLoaderClient* client)
{
    ASSERT(!m_pendingRequest);
    m_pendingRequest = client;
    m_pendingRequestType = InspectorPageAgent::FetchResource;
}

void InspectorResourceAgent::didFailFetch(ThreadableLoaderClient* client)
{
    m_knownRequestIdMap.remove(client);
}

void InspectorResourceAgent::didFinishFetch(ExecutionContext* context, ThreadableLoaderClient* client, const AtomicString& method, const String& url)
{
    ThreadableLoaderClientRequestIdMap::iterator it = m_knownRequestIdMap.find(client);
    if (it == m_knownRequestIdMap.end())
        return;

    if (m_state->booleanProperty(ResourceAgentState::monitoringXHR, false)) {
        String message = "Fetch complete: " + method + " \"" + url + "\".";
        RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(NetworkMessageSource, DebugMessageLevel, message);
        consoleMessage->setRequestIdentifier(it->value);
        m_inspectedFrames->root()->host()->consoleMessageStorage().reportMessage(context, consoleMessage.release());
    }
    m_knownRequestIdMap.remove(client);
}

void InspectorResourceAgent::willSendEventSourceRequest(ThreadableLoaderClient* eventSource)
{
    ASSERT(!m_pendingRequest);
    m_pendingRequest = eventSource;
    m_pendingRequestType = InspectorPageAgent::EventSourceResource;
}

void InspectorResourceAgent::willDispachEventSourceEvent(ThreadableLoaderClient* eventSource, const AtomicString& eventName, const AtomicString& eventId, const Vector<UChar>& data)
{
    ThreadableLoaderClientRequestIdMap::iterator it = m_knownRequestIdMap.find(eventSource);
    if (it == m_knownRequestIdMap.end())
        return;
    frontend()->eventSourceMessageReceived(IdentifiersFactory::requestId(it->value), monotonicallyIncreasingTime(), eventName.string(), eventId.string(), String(data));
}

void InspectorResourceAgent::didFinishEventSourceRequest(ThreadableLoaderClient* eventSource)
{
    m_knownRequestIdMap.remove(eventSource);
    m_pendingRequest = nullptr;
}

void InspectorResourceAgent::removedResourceFromMemoryCache(Resource* cachedResource)
{
    String content;
    bool base64Encoded;
    bool hasContent = InspectorPageAgent::cachedResourceContent(cachedResource, &content, &base64Encoded);
    Vector<String> requestIds = m_resourcesData->removeResource(cachedResource);
    if (hasContent && !isErrorStatusCode(cachedResource->response().httpStatusCode())) {
        for (auto& request : requestIds)
            m_resourcesData->setResourceContent(request, content, base64Encoded);
    }
}

void InspectorResourceAgent::applyUserAgentOverride(String* userAgent)
{
    String userAgentOverride;
    m_state->getString(ResourceAgentState::userAgentOverride, &userAgentOverride);
    if (!userAgentOverride.isEmpty())
        *userAgent = userAgentOverride;
}

void InspectorResourceAgent::willRecalculateStyle(Document*)
{
    m_isRecalculatingStyle = true;
}

void InspectorResourceAgent::didRecalculateStyle(int)
{
    m_isRecalculatingStyle = false;
    m_styleRecalculationInitiator = nullptr;
}

void InspectorResourceAgent::didScheduleStyleRecalculation(Document* document)
{
    if (!m_styleRecalculationInitiator)
        m_styleRecalculationInitiator = buildInitiatorObject(document, FetchInitiatorInfo());
}

PassRefPtr<TypeBuilder::Network::Initiator> InspectorResourceAgent::buildInitiatorObject(Document* document, const FetchInitiatorInfo& initiatorInfo)
{
    RefPtr<ScriptCallStack> stackTrace = ScriptCallStack::capture();
    if (stackTrace) {
        RefPtr<TypeBuilder::Network::Initiator> initiatorObject = TypeBuilder::Network::Initiator::create()
            .setType(TypeBuilder::Network::Initiator::Type::Script);
        initiatorObject->setStack(stackTrace->buildInspectorObject());
        return initiatorObject;
    }

    while (document && !document->scriptableDocumentParser())
        document = document->ownerElement() ? document->ownerElement()->ownerDocument() : nullptr;
    if (document && document->scriptableDocumentParser()) {
        RefPtr<TypeBuilder::Network::Initiator> initiatorObject = TypeBuilder::Network::Initiator::create()
            .setType(TypeBuilder::Network::Initiator::Type::Parser);
        initiatorObject->setUrl(urlWithoutFragment(document->url()).string());
        if (TextPosition::belowRangePosition() != initiatorInfo.position)
            initiatorObject->setLineNumber(initiatorInfo.position.m_line.oneBasedInt());
        else
            initiatorObject->setLineNumber(document->scriptableDocumentParser()->lineNumber().oneBasedInt());
        return initiatorObject;
    }

    if (m_isRecalculatingStyle && m_styleRecalculationInitiator)
        return m_styleRecalculationInitiator;

    return TypeBuilder::Network::Initiator::create()
        .setType(TypeBuilder::Network::Initiator::Type::Other)
        .release();
}

void InspectorResourceAgent::didCreateWebSocket(Document*, unsigned long identifier, const KURL& requestURL, const String&)
{
    frontend()->webSocketCreated(IdentifiersFactory::requestId(identifier), urlWithoutFragment(requestURL).string());
}

void InspectorResourceAgent::willSendWebSocketHandshakeRequest(Document*, unsigned long identifier, const WebSocketHandshakeRequest* request)
{
    ASSERT(request);
    RefPtr<TypeBuilder::Network::WebSocketRequest> requestObject = TypeBuilder::Network::WebSocketRequest::create()
        .setHeaders(buildObjectForHeaders(request->headerFields()));
    frontend()->webSocketWillSendHandshakeRequest(IdentifiersFactory::requestId(identifier), monotonicallyIncreasingTime(), currentTime(), requestObject);
}

void InspectorResourceAgent::didReceiveWebSocketHandshakeResponse(Document*, unsigned long identifier, const WebSocketHandshakeRequest* request, const WebSocketHandshakeResponse* response)
{
    ASSERT(response);
    RefPtr<TypeBuilder::Network::WebSocketResponse> responseObject = TypeBuilder::Network::WebSocketResponse::create()
        .setStatus(response->statusCode())
        .setStatusText(response->statusText())
        .setHeaders(buildObjectForHeaders(response->headerFields()));

    if (!response->headersText().isEmpty())
        responseObject->setHeadersText(response->headersText());
    if (request) {
        responseObject->setRequestHeaders(buildObjectForHeaders(request->headerFields()));
        if (!request->headersText().isEmpty())
            responseObject->setRequestHeadersText(request->headersText());
    }
    frontend()->webSocketHandshakeResponseReceived(IdentifiersFactory::requestId(identifier), monotonicallyIncreasingTime(), responseObject);
}

void InspectorResourceAgent::didCloseWebSocket(Document*, unsigned long identifier)
{
    frontend()->webSocketClosed(IdentifiersFactory::requestId(identifier), monotonicallyIncreasingTime());
}

void InspectorResourceAgent::didReceiveWebSocketFrame(unsigned long identifier, int opCode, bool masked, const char* payload, size_t payloadLength)
{
    RefPtr<TypeBuilder::Network::WebSocketFrame> frameObject = TypeBuilder::Network::WebSocketFrame::create()
        .setOpcode(opCode)
        .setMask(masked)
        .setPayloadData(String::fromUTF8WithLatin1Fallback(payload, payloadLength));
    frontend()->webSocketFrameReceived(IdentifiersFactory::requestId(identifier), monotonicallyIncreasingTime(), frameObject);
}

void InspectorResourceAgent::didSendWebSocketFrame(unsigned long identifier, int opCode, bool masked, const char* payload, size_t payloadLength)
{
    RefPtr<TypeBuilder::Network::WebSocketFrame> frameObject = TypeBuilder::Network::WebSocketFrame::create()
        .setOpcode(opCode)
        .setMask(masked)
        .setPayloadData(String::fromUTF8WithLatin1Fallback(payload, payloadLength));
    frontend()->webSocketFrameSent(IdentifiersFactory::requestId(identifier), monotonicallyIncreasingTime(), frameObject);
}

void InspectorResourceAgent::didReceiveWebSocketFrameError(unsigned long identifier, const String& errorMessage)
{
    frontend()->webSocketFrameError(IdentifiersFactory::requestId(identifier), monotonicallyIncreasingTime(), errorMessage);
}

void InspectorResourceAgent::enable(ErrorString*)
{
    enable();
}

void InspectorResourceAgent::enable()
{
    if (!frontend())
        return;
    m_state->setBoolean(ResourceAgentState::resourceAgentEnabled, true);
    m_instrumentingAgents->setInspectorResourceAgent(this);
}

void InspectorResourceAgent::disable(ErrorString*)
{
    ASSERT(!m_pendingRequest);
    m_state->setBoolean(ResourceAgentState::resourceAgentEnabled, false);
    m_state->setString(ResourceAgentState::userAgentOverride, "");
    m_instrumentingAgents->setInspectorResourceAgent(0);
    m_resourcesData->clear();
    m_knownRequestIdMap.clear();
}

void InspectorResourceAgent::setUserAgentOverride(ErrorString*, const String& userAgent)
{
    m_state->setString(ResourceAgentState::userAgentOverride, userAgent);
}

void InspectorResourceAgent::setExtraHTTPHeaders(ErrorString*, const RefPtr<JSONObject>& headers)
{
    m_state->setObject(ResourceAgentState::extraRequestHeaders, headers);
}

bool InspectorResourceAgent::getResponseBodyBlob(const String& requestId, PassRefPtr<GetResponseBodyCallback> callback)
{
    NetworkResourcesData::ResourceData const* resourceData = m_resourcesData->data(requestId);
    if (!resourceData)
        return false;
    if (BlobDataHandle* blob = resourceData->downloadedFileBlob()) {
        if (LocalFrame* frame = IdentifiersFactory::frameById(m_inspectedFrames, resourceData->frameId())) {
            if (Document* document = frame->document()) {
                InspectorFileReaderLoaderClient* client = new InspectorFileReaderLoaderClient(blob, InspectorPageAgent::createResourceTextDecoder(resourceData->mimeType(), resourceData->textEncodingName()), callback);
                client->start(document);
                return true;
            }
        }
    }
    return false;
}


void InspectorResourceAgent::getResponseBody(ErrorString* errorString, const String& requestId, PassRefPtr<GetResponseBodyCallback> passCallback)
{
    RefPtr<GetResponseBodyCallback> callback = passCallback;
    NetworkResourcesData::ResourceData const* resourceData = m_resourcesData->data(requestId);
    if (!resourceData) {
        callback->sendFailure("No resource with given identifier found");
        return;
    }

    // XHR with ResponseTypeBlob should be returned as blob.
    if (resourceData->xhrReplayData() && getResponseBodyBlob(requestId, callback))
        return;

    if (resourceData->hasContent()) {
        callback->sendSuccess(resourceData->content(), resourceData->base64Encoded());
        return;
    }

    if (resourceData->isContentEvicted()) {
        callback->sendFailure("Request content was evicted from inspector cache");
        return;
    }

    if (resourceData->buffer() && !resourceData->textEncodingName().isNull()) {
        String content;
        if (InspectorPageAgent::sharedBufferContent(resourceData->buffer(), resourceData->textEncodingName(), false, &content)) {
            callback->sendSuccess(content, false);
            return;
        }
    }

    if (resourceData->cachedResource()) {
        String content;
        bool base64Encoded = false;
        if (InspectorPageAgent::cachedResourceContent(resourceData->cachedResource(), &content, &base64Encoded)) {
            callback->sendSuccess(content, base64Encoded);
            return;
        }
    }

    if (getResponseBodyBlob(requestId, callback))
        return;

    callback->sendFailure("No data found for resource with given identifier");
}

void InspectorResourceAgent::addBlockedURL(ErrorString*, const String& url)
{
    RefPtr<JSONObject> blockedURLs = m_state->getObject(ResourceAgentState::blockedURLs);
    if (!blockedURLs) {
        blockedURLs = JSONObject::create();
        m_state->setObject(ResourceAgentState::blockedURLs, blockedURLs);
    }
    blockedURLs->setBoolean(url, true);
}

void InspectorResourceAgent::removeBlockedURL(ErrorString*, const String& url)
{
    RefPtr<JSONObject> blockedURLs = m_state->getObject(ResourceAgentState::blockedURLs);
    if (blockedURLs)
        blockedURLs->remove(url);
}

void InspectorResourceAgent::replayXHR(ErrorString*, const String& requestId)
{
    String actualRequestId = requestId;

    XHRReplayData* xhrReplayData = m_resourcesData->xhrReplayData(requestId);
    if (!xhrReplayData)
        return;

    ExecutionContext* executionContext = xhrReplayData->executionContext();
    if (!executionContext) {
        m_resourcesData->setXHRReplayData(requestId, 0);
        return;
    }

    XMLHttpRequest* xhr = XMLHttpRequest::create(executionContext);

    executionContext->removeURLFromMemoryCache(xhrReplayData->url());

    xhr->open(xhrReplayData->method(), xhrReplayData->url(), xhrReplayData->async(), IGNORE_EXCEPTION);
    if (xhrReplayData->includeCredentials())
        xhr->setWithCredentials(true, IGNORE_EXCEPTION);
    for (const auto& header : xhrReplayData->headers())
        xhr->setRequestHeader(header.key, header.value, IGNORE_EXCEPTION);
    xhr->sendForInspectorXHRReplay(xhrReplayData->formData(), IGNORE_EXCEPTION);

    m_replayXHRs.add(xhr);
}

void InspectorResourceAgent::setMonitoringXHREnabled(ErrorString*, bool enabled)
{
    m_state->setBoolean(ResourceAgentState::monitoringXHR, enabled);
}

void InspectorResourceAgent::canClearBrowserCache(ErrorString*, bool* result)
{
    *result = true;
}

void InspectorResourceAgent::canClearBrowserCookies(ErrorString*, bool* result)
{
    *result = true;
}

void InspectorResourceAgent::setCacheDisabled(ErrorString*, bool cacheDisabled)
{
    m_state->setBoolean(ResourceAgentState::cacheDisabled, cacheDisabled);
    if (cacheDisabled)
        memoryCache()->evictResources();
}

void InspectorResourceAgent::emulateNetworkConditions(ErrorString*, bool, double, double, double)
{
}

void InspectorResourceAgent::setDataSizeLimitsForTest(ErrorString*, int maxTotal, int maxResource)
{
    m_resourcesData->setResourcesDataSizeLimits(maxTotal, maxResource);
}

void InspectorResourceAgent::didCommitLoad(LocalFrame* frame, DocumentLoader* loader)
{
    if (loader->frame() != m_inspectedFrames->root())
        return;

    if (m_state->booleanProperty(ResourceAgentState::cacheDisabled, false))
        memoryCache()->evictResources();

    m_resourcesData->clear(IdentifiersFactory::loaderId(loader));
}

void InspectorResourceAgent::frameScheduledNavigation(LocalFrame* frame, double)
{
    RefPtr<TypeBuilder::Network::Initiator> initiator = buildInitiatorObject(frame->document(), FetchInitiatorInfo());
    m_frameNavigationInitiatorMap.set(IdentifiersFactory::frameId(frame), initiator);
}

void InspectorResourceAgent::frameClearedScheduledNavigation(LocalFrame* frame)
{
    m_frameNavigationInitiatorMap.remove(IdentifiersFactory::frameId(frame));
}

void InspectorResourceAgent::setHostId(const String& hostId)
{
    m_hostId = hostId;
}

bool InspectorResourceAgent::fetchResourceContent(Document* document, const KURL& url, String* content, bool* base64Encoded)
{
    // First try to fetch content from the cached resource.
    Resource* cachedResource = document->fetcher()->cachedResource(url);
    if (!cachedResource)
        cachedResource = memoryCache()->resourceForURL(url, document->fetcher()->getCacheIdentifier());
    if (cachedResource && InspectorPageAgent::cachedResourceContent(cachedResource, content, base64Encoded))
        return true;

    // Then fall back to resource data.
    for (auto& resource : m_resourcesData->resources()) {
        if (resource->url() == url) {
            *content = resource->content();
            *base64Encoded = resource->base64Encoded();
            return true;
        }
    }
    return false;
}

void InspectorResourceAgent::removeFinishedReplayXHRFired(Timer<InspectorResourceAgent>*)
{
    m_replayXHRsToBeDeleted.clear();
}

InspectorResourceAgent::InspectorResourceAgent(InspectedFrames* inspectedFrames)
    : InspectorBaseAgent<InspectorResourceAgent, InspectorFrontend::Network>("Network")
    , m_inspectedFrames(inspectedFrames)
    , m_resourcesData(NetworkResourcesData::create())
    , m_pendingRequest(nullptr)
    , m_isRecalculatingStyle(false)
    , m_removeFinishedReplayXHRTimer(this, &InspectorResourceAgent::removeFinishedReplayXHRFired)
{
}

bool InspectorResourceAgent::shouldForceCORSPreflight()
{
    return m_state->booleanProperty(ResourceAgentState::cacheDisabled, false);
}

} // namespace blink

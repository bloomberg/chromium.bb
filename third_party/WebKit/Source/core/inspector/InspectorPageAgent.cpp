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

#include "core/inspector/InspectorPageAgent.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptRegexp.h"
#include "core/HTMLNames.h"
#include "core/dom/DOMImplementation.h"
#include "core/dom/Document.h"
#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/FontResource.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ScriptResource.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/VoidCallback.h"
#include "core/html/imports/HTMLImportLoader.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/inspector/DOMPatchSupport.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorDebuggerAgent.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorResourceContentLoader.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/PlatformResourceLoader.h"
#include "platform/UserGestureIndicator.h"
#include "platform/inspector_protocol/Values.h"
#include "platform/v8_inspector/public/V8ContentSearchUtil.h"
#include "platform/v8_inspector/public/V8DebuggerAgent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/CurrentTime.h"
#include "wtf/ListHashSet.h"
#include "wtf/Vector.h"
#include "wtf/text/Base64.h"
#include "wtf/text/TextEncoding.h"

namespace blink {

namespace PageAgentState {
static const char pageAgentEnabled[] = "pageAgentEnabled";
static const char pageAgentScriptsToEvaluateOnLoad[] = "pageAgentScriptsToEvaluateOnLoad";
static const char screencastEnabled[] = "screencastEnabled";
static const char autoAttachToCreatedPages[] = "autoAttachToCreatedPages";
}

namespace {

KURL urlWithoutFragment(const KURL& url)
{
    KURL result = url;
    result.removeFragmentIdentifier();
    return result;
}

String frameId(LocalFrame* frame)
{
    return frame ? IdentifiersFactory::frameId(frame) : "";
}

String dialogTypeToProtocol(ChromeClient::DialogType dialogType)
{
    switch (dialogType) {
    case ChromeClient::AlertDialog:
        return protocol::Page::DialogTypeEnum::Alert;
    case ChromeClient::ConfirmDialog:
        return protocol::Page::DialogTypeEnum::Confirm;
    case ChromeClient::PromptDialog:
        return protocol::Page::DialogTypeEnum::Prompt;
    case ChromeClient::HTMLDialog:
        return protocol::Page::DialogTypeEnum::Beforeunload;
    }
    return protocol::Page::DialogTypeEnum::Alert;
}

} // namespace

static bool decodeBuffer(const char* buffer, unsigned size, const String& textEncodingName, String* result)
{
    if (buffer) {
        WTF::TextEncoding encoding(textEncodingName);
        if (!encoding.isValid())
            encoding = WindowsLatin1Encoding();
        *result = encoding.decode(buffer, size);
        return true;
    }
    return false;
}

static bool prepareResourceBuffer(Resource* cachedResource, bool* hasZeroSize)
{
    *hasZeroSize = false;
    if (!cachedResource)
        return false;

    if (cachedResource->getDataBufferingPolicy() == DoNotBufferData)
        return false;

    // Zero-sized resources don't have data at all -- so fake the empty buffer, instead of indicating error by returning 0.
    if (!cachedResource->encodedSize()) {
        *hasZeroSize = true;
        return true;
    }

    if (cachedResource->isPurgeable()) {
        // If the resource is purgeable then make it unpurgeable to get
        // get its data. This might fail, in which case we return an
        // empty String.
        // FIXME: should we do something else in the case of a purged
        // resource that informs the user why there is no data in the
        // inspector?
        if (!cachedResource->lock())
            return false;
    }

    return true;
}

static bool hasTextContent(Resource* cachedResource)
{
    Resource::Type type = cachedResource->getType();
    return type == Resource::CSSStyleSheet || type == Resource::XSLStyleSheet || type == Resource::Script || type == Resource::Raw || type == Resource::ImportResource || type == Resource::MainResource;
}

PassOwnPtr<TextResourceDecoder> InspectorPageAgent::createResourceTextDecoder(const String& mimeType, const String& textEncodingName)
{
    if (!textEncodingName.isEmpty())
        return TextResourceDecoder::create("text/plain", textEncodingName);
    if (DOMImplementation::isXMLMIMEType(mimeType)) {
        OwnPtr<TextResourceDecoder> decoder = TextResourceDecoder::create("application/xml");
        decoder->useLenientXMLDecoding();
        return decoder.release();
    }
    if (equalIgnoringCase(mimeType, "text/html"))
        return TextResourceDecoder::create("text/html", "UTF-8");
    if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(mimeType) || DOMImplementation::isJSONMIMEType(mimeType))
        return TextResourceDecoder::create("text/plain", "UTF-8");
    if (DOMImplementation::isTextMIMEType(mimeType))
        return TextResourceDecoder::create("text/plain", "ISO-8859-1");
    return PassOwnPtr<TextResourceDecoder>();
}

static void resourceContent(ErrorString* errorString, LocalFrame* frame, const KURL& url, String* result, bool* base64Encoded)
{
    if (!InspectorPageAgent::cachedResourceContent(InspectorPageAgent::cachedResource(frame, url), result, base64Encoded))
        *errorString = "No resource with given URL found";
}

static bool encodeCachedResourceContent(Resource* cachedResource, bool hasZeroSize, String* result, bool* base64Encoded)
{
    *base64Encoded = true;
    RefPtr<SharedBuffer> buffer = hasZeroSize ? SharedBuffer::create() : cachedResource->resourceBuffer();

    if (!buffer)
        return false;

    *result = base64Encode(buffer->data(), buffer->size());
    return true;
}

bool InspectorPageAgent::cachedResourceContent(Resource* cachedResource, String* result, bool* base64Encoded)
{
    bool hasZeroSize;
    bool prepared = prepareResourceBuffer(cachedResource, &hasZeroSize);
    if (!prepared)
        return false;

    if (!hasTextContent(cachedResource))
        return encodeCachedResourceContent(cachedResource, hasZeroSize, result, base64Encoded);
    *base64Encoded = false;

    if (hasZeroSize) {
        *result = "";
        return true;
    }

    if (cachedResource) {
        switch (cachedResource->getType()) {
        case Resource::CSSStyleSheet:
            *result = toCSSStyleSheetResource(cachedResource)->sheetText();
            return true;
        case Resource::Script:
            *result = cachedResource->resourceBuffer() ? toScriptResource(cachedResource)->decodedText() : toScriptResource(cachedResource)->script().toString();
            return true;
        case Resource::ImportResource: // Fall through.
        case Resource::Raw: {
            SharedBuffer* buffer = cachedResource->resourceBuffer();
            if (!buffer)
                return false;
            OwnPtr<TextResourceDecoder> decoder = InspectorPageAgent::createResourceTextDecoder(cachedResource->response().mimeType(), cachedResource->response().textEncodingName());
            if (!decoder)
                return encodeCachedResourceContent(cachedResource, hasZeroSize, result, base64Encoded);
            String content = decoder->decode(buffer->data(), buffer->size());
            *result = content + decoder->flush();
            return true;
        }
        default:
            SharedBuffer* buffer = cachedResource->resourceBuffer();
            return decodeBuffer(buffer ? buffer->data() : nullptr, buffer ? buffer->size() : 0, cachedResource->response().textEncodingName(), result);
        }
    }
    return false;
}

// static
bool InspectorPageAgent::sharedBufferContent(PassRefPtr<SharedBuffer> buffer, const String& textEncodingName, bool withBase64Encode, String* result)
{
    return dataContent(buffer ? buffer->data() : nullptr, buffer ? buffer->size() : 0, textEncodingName, withBase64Encode, result);
}

bool InspectorPageAgent::dataContent(const char* data, unsigned size, const String& textEncodingName, bool withBase64Encode, String* result)
{
    if (withBase64Encode) {
        *result = base64Encode(data, size);
        return true;
    }

    return decodeBuffer(data, size, textEncodingName, result);
}

InspectorPageAgent* InspectorPageAgent::create(InspectedFrames* inspectedFrames, Client* client, InspectorResourceContentLoader* resourceContentLoader, InspectorDebuggerAgent* debuggerAgent)
{
    return new InspectorPageAgent(inspectedFrames, client, resourceContentLoader, debuggerAgent);
}

Resource* InspectorPageAgent::cachedResource(LocalFrame* frame, const KURL& url)
{
    Document* document = frame->document();
    if (!document)
        return nullptr;
    Resource* cachedResource = document->fetcher()->cachedResource(url);
    if (!cachedResource) {
        HeapVector<Member<Document>> allImports = InspectorPageAgent::importsForFrame(frame);
        for (Document* import : allImports) {
            cachedResource = import->fetcher()->cachedResource(url);
            if (cachedResource)
                break;
        }
    }
    if (!cachedResource)
        cachedResource = memoryCache()->resourceForURL(url, document->fetcher()->getCacheIdentifier());
    return cachedResource;
}

String InspectorPageAgent::resourceTypeJson(InspectorPageAgent::ResourceType resourceType)
{
    switch (resourceType) {
    case DocumentResource:
        return protocol::Page::ResourceTypeEnum::Document;
    case FontResource:
        return protocol::Page::ResourceTypeEnum::Font;
    case ImageResource:
        return protocol::Page::ResourceTypeEnum::Image;
    case MediaResource:
        return protocol::Page::ResourceTypeEnum::Media;
    case ScriptResource:
        return protocol::Page::ResourceTypeEnum::Script;
    case StylesheetResource:
        return protocol::Page::ResourceTypeEnum::Stylesheet;
    case TextTrackResource:
        return protocol::Page::ResourceTypeEnum::TextTrack;
    case XHRResource:
        return protocol::Page::ResourceTypeEnum::XHR;
    case FetchResource:
        return protocol::Page::ResourceTypeEnum::Fetch;
    case EventSourceResource:
        return protocol::Page::ResourceTypeEnum::EventSource;
    case WebSocketResource:
        return protocol::Page::ResourceTypeEnum::WebSocket;
    case ManifestResource:
        return protocol::Page::ResourceTypeEnum::Manifest;
    case OtherResource:
        return protocol::Page::ResourceTypeEnum::Other;
    }
    return protocol::Page::ResourceTypeEnum::Other;
}

InspectorPageAgent::ResourceType InspectorPageAgent::cachedResourceType(const Resource& cachedResource)
{
    switch (cachedResource.getType()) {
    case Resource::Image:
        return InspectorPageAgent::ImageResource;
    case Resource::Font:
        return InspectorPageAgent::FontResource;
    case Resource::Media:
        return InspectorPageAgent::MediaResource;
    case Resource::Manifest:
        return InspectorPageAgent::ManifestResource;
    case Resource::TextTrack:
        return InspectorPageAgent::TextTrackResource;
    case Resource::CSSStyleSheet:
        // Fall through.
    case Resource::XSLStyleSheet:
        return InspectorPageAgent::StylesheetResource;
    case Resource::Script:
        return InspectorPageAgent::ScriptResource;
    case Resource::ImportResource:
        // Fall through.
    case Resource::MainResource:
        return InspectorPageAgent::DocumentResource;
    default:
        break;
    }
    return InspectorPageAgent::OtherResource;
}

String InspectorPageAgent::cachedResourceTypeJson(const Resource& cachedResource)
{
    return resourceTypeJson(cachedResourceType(cachedResource));
}

InspectorPageAgent::InspectorPageAgent(InspectedFrames* inspectedFrames, Client* client, InspectorResourceContentLoader* resourceContentLoader, InspectorDebuggerAgent* debuggerAgent)
    : InspectorBaseAgent<InspectorPageAgent, protocol::Frontend::Page>("Page")
    , m_inspectedFrames(inspectedFrames)
    , m_debuggerAgent(debuggerAgent)
    , m_client(client)
    , m_lastScriptIdentifier(0)
    , m_enabled(false)
    , m_reloading(false)
    , m_inspectorResourceContentLoader(resourceContentLoader)
    , m_resourceContentLoaderClientId(resourceContentLoader->createClientId())
{
}

void InspectorPageAgent::restore()
{
    if (m_state->booleanProperty(PageAgentState::pageAgentEnabled, false)) {
        ErrorString error;
        enable(&error);
    }
}

void InspectorPageAgent::enable(ErrorString*)
{
    m_enabled = true;
    m_state->setBoolean(PageAgentState::pageAgentEnabled, true);
    m_instrumentingAgents->setInspectorPageAgent(this);
}

void InspectorPageAgent::disable(ErrorString*)
{
    m_enabled = false;
    m_state->setBoolean(PageAgentState::pageAgentEnabled, false);
    m_state->remove(PageAgentState::pageAgentScriptsToEvaluateOnLoad);
    m_scriptToEvaluateOnLoadOnce = String();
    m_pendingScriptToEvaluateOnLoadOnce = String();
    m_instrumentingAgents->setInspectorPageAgent(0);
    m_inspectorResourceContentLoader->cancel(m_resourceContentLoaderClientId);

    stopScreencast(0);

    finishReload();
}

void InspectorPageAgent::addScriptToEvaluateOnLoad(ErrorString*, const String& source, String* identifier)
{
    protocol::DictionaryValue* scripts = m_state->getObject(PageAgentState::pageAgentScriptsToEvaluateOnLoad);
    if (!scripts) {
        OwnPtr<protocol::DictionaryValue> newScripts = protocol::DictionaryValue::create();
        scripts = newScripts.get();
        m_state->setObject(PageAgentState::pageAgentScriptsToEvaluateOnLoad, newScripts.release());
    }
    // Assure we don't override existing ids -- m_lastScriptIdentifier could get out of sync WRT actual
    // scripts once we restored the scripts from the cookie during navigation.
    do {
        *identifier = String::number(++m_lastScriptIdentifier);
    } while (scripts->get(*identifier));
    scripts->setString(*identifier, source);
}

void InspectorPageAgent::removeScriptToEvaluateOnLoad(ErrorString* error, const String& identifier)
{
    protocol::DictionaryValue* scripts = m_state->getObject(PageAgentState::pageAgentScriptsToEvaluateOnLoad);
    if (!scripts || !scripts->get(identifier)) {
        *error = "Script not found";
        return;
    }
    scripts->remove(identifier);
}

void InspectorPageAgent::setAutoAttachToCreatedPages(ErrorString*, bool autoAttach)
{
    m_state->setBoolean(PageAgentState::autoAttachToCreatedPages, autoAttach);
}

void InspectorPageAgent::reload(ErrorString*, const Maybe<bool>& optionalBypassCache, const Maybe<String>& optionalScriptToEvaluateOnLoad)
{
    m_pendingScriptToEvaluateOnLoadOnce = optionalScriptToEvaluateOnLoad.fromMaybe("");
    ErrorString unused;
    m_debuggerAgent->setSkipAllPauses(&unused, true);
    m_reloading = true;
    m_inspectedFrames->root()->reload(optionalBypassCache.fromMaybe(false) ? FrameLoadTypeReloadBypassingCache : FrameLoadTypeReload, ClientRedirectPolicy::NotClientRedirect);
}

void InspectorPageAgent::navigate(ErrorString*, const String& url, String* outFrameId)
{
    *outFrameId = frameId(m_inspectedFrames->root());
}

static void cachedResourcesForDocument(Document* document, HeapVector<Member<Resource>>& result, bool skipXHRs)
{
    const ResourceFetcher::DocumentResourceMap& allResources = document->fetcher()->allResources();
    for (const auto& resource : allResources) {
        Resource* cachedResource = resource.value.get();
        if (!cachedResource)
            continue;

        // Skip images that were not auto loaded (images disabled in the user agent),
        // fonts that were referenced in CSS but never used/downloaded, etc.
        if (cachedResource->stillNeedsLoad())
            continue;
        if (cachedResource->getType() == Resource::Raw && skipXHRs)
            continue;
        result.append(cachedResource);
    }
}

// static
HeapVector<Member<Document>> InspectorPageAgent::importsForFrame(LocalFrame* frame)
{
    HeapVector<Member<Document>> result;
    Document* rootDocument = frame->document();

    if (HTMLImportsController* controller = rootDocument->importsController()) {
        for (size_t i = 0; i < controller->loaderCount(); ++i) {
            if (Document* document = controller->loaderAt(i)->document())
                result.append(document);
        }
    }

    return result;
}

static HeapVector<Member<Resource>> cachedResourcesForFrame(LocalFrame* frame, bool skipXHRs)
{
    HeapVector<Member<Resource>> result;
    Document* rootDocument = frame->document();
    HeapVector<Member<Document>> loaders = InspectorPageAgent::importsForFrame(frame);

    cachedResourcesForDocument(rootDocument, result, skipXHRs);
    for (size_t i = 0; i < loaders.size(); ++i)
        cachedResourcesForDocument(loaders[i], result, skipXHRs);

    return result;
}

void InspectorPageAgent::getResourceTree(ErrorString*, OwnPtr<protocol::Page::FrameResourceTree>* object)
{
    *object = buildObjectForFrameTree(m_inspectedFrames->root());
}

void InspectorPageAgent::finishReload()
{
    if (!m_reloading)
        return;
    m_reloading = false;
    ErrorString unused;
    m_debuggerAgent->setSkipAllPauses(&unused, false);
}

void InspectorPageAgent::getResourceContentAfterResourcesContentLoaded(const String& frameId, const String& url, PassOwnPtr<GetResourceContentCallback> callback)
{
    LocalFrame* frame = IdentifiersFactory::frameById(m_inspectedFrames, frameId);
    if (!frame) {
        callback->sendFailure("No frame for given id found");
        return;
    }
    ErrorString errorString;
    String content;
    bool base64Encoded;
    resourceContent(&errorString, frame, KURL(ParsedURLString, url), &content, &base64Encoded);
    if (!errorString.isEmpty()) {
        callback->sendFailure(errorString);
        return;
    }
    callback->sendSuccess(content, base64Encoded);
}

void InspectorPageAgent::getResourceContent(ErrorString* errorString, const String& frameId, const String& url, PassOwnPtr<GetResourceContentCallback> callback)
{
    if (!m_enabled) {
        callback->sendFailure("Agent is not enabled.");
        return;
    }
    m_inspectorResourceContentLoader->ensureResourcesContentLoaded(m_resourceContentLoaderClientId, bind(&InspectorPageAgent::getResourceContentAfterResourcesContentLoaded, this, frameId, url, passed(std::move(callback))));
}

void InspectorPageAgent::searchContentAfterResourcesContentLoaded(const String& frameId, const String& url, const String& query, bool caseSensitive, bool isRegex, PassOwnPtr<SearchInResourceCallback> callback)
{
    LocalFrame* frame = IdentifiersFactory::frameById(m_inspectedFrames, frameId);
    if (!frame) {
        callback->sendFailure("No frame for given id found");
        return;
    }
    ErrorString errorString;
    String content;
    bool base64Encoded;
    resourceContent(&errorString, frame, KURL(ParsedURLString, url), &content, &base64Encoded);
    if (!errorString.isEmpty()) {
        callback->sendFailure(errorString);
        return;
    }

    OwnPtr<protocol::Array<protocol::Debugger::SearchMatch>> results;
    results = V8ContentSearchUtil::searchInTextByLines(&m_debuggerAgent->v8Agent()->debugger(), content, query, caseSensitive, isRegex);
    callback->sendSuccess(results.release());
}

void InspectorPageAgent::searchInResource(ErrorString*, const String& frameId, const String& url, const String& query, const Maybe<bool>& optionalCaseSensitive, const Maybe<bool>& optionalIsRegex, PassOwnPtr<SearchInResourceCallback> callback)
{
    if (!m_enabled) {
        callback->sendFailure("Agent is not enabled.");
        return;
    }
    m_inspectorResourceContentLoader->ensureResourcesContentLoaded(m_resourceContentLoaderClientId, bind(&InspectorPageAgent::searchContentAfterResourcesContentLoaded, this, frameId, url, query, optionalCaseSensitive.fromMaybe(false), optionalIsRegex.fromMaybe(false), passed(std::move(callback))));
}

void InspectorPageAgent::setDocumentContent(ErrorString* errorString, const String& frameId, const String& html)
{
    LocalFrame* frame = IdentifiersFactory::frameById(m_inspectedFrames, frameId);
    if (!frame) {
        *errorString = "No frame for given id found";
        return;
    }

    Document* document = frame->document();
    if (!document) {
        *errorString = "No Document instance to set HTML for";
        return;
    }
    DOMPatchSupport::patchDocument(*document, html);
}

void InspectorPageAgent::didClearDocumentOfWindowObject(LocalFrame* frame)
{
    if (!frontend())
        return;

    protocol::DictionaryValue* scripts = m_state->getObject(PageAgentState::pageAgentScriptsToEvaluateOnLoad);
    if (scripts) {
        for (size_t i = 0; i < scripts->size(); ++i) {
            auto script = scripts->at(i);
            String16 scriptText;
            if (script.second->asString(&scriptText))
                frame->script().executeScriptInMainWorld(scriptText);
        }
    }
    if (!m_scriptToEvaluateOnLoadOnce.isEmpty())
        frame->script().executeScriptInMainWorld(m_scriptToEvaluateOnLoadOnce);
}

void InspectorPageAgent::domContentLoadedEventFired(LocalFrame* frame)
{
    if (frame != m_inspectedFrames->root())
        return;
    frontend()->domContentEventFired(monotonicallyIncreasingTime());
}

void InspectorPageAgent::loadEventFired(LocalFrame* frame)
{
    if (frame != m_inspectedFrames->root())
        return;
    frontend()->loadEventFired(monotonicallyIncreasingTime());
}

void InspectorPageAgent::didCommitLoad(LocalFrame*, DocumentLoader* loader)
{
    if (loader->frame() == m_inspectedFrames->root()) {
        finishReload();
        m_scriptToEvaluateOnLoadOnce = m_pendingScriptToEvaluateOnLoadOnce;
        m_pendingScriptToEvaluateOnLoadOnce = String();
    }
    frontend()->frameNavigated(buildObjectForFrame(loader->frame()));
}

void InspectorPageAgent::frameAttachedToParent(LocalFrame* frame)
{
    Frame* parentFrame = frame->tree().parent();
    if (!parentFrame->isLocalFrame())
        parentFrame = 0;
    frontend()->frameAttached(frameId(frame), frameId(toLocalFrame(parentFrame)));
}

void InspectorPageAgent::frameDetachedFromParent(LocalFrame* frame)
{
    frontend()->frameDetached(frameId(frame));
}

bool InspectorPageAgent::screencastEnabled()
{
    return m_enabled && m_state->booleanProperty(PageAgentState::screencastEnabled, false);
}

void InspectorPageAgent::frameStartedLoading(LocalFrame* frame)
{
    frontend()->frameStartedLoading(frameId(frame));
}

void InspectorPageAgent::frameStoppedLoading(LocalFrame* frame)
{
    frontend()->frameStoppedLoading(frameId(frame));
}

void InspectorPageAgent::frameScheduledNavigation(LocalFrame* frame, double delay)
{
    frontend()->frameScheduledNavigation(frameId(frame), delay);
}

void InspectorPageAgent::frameClearedScheduledNavigation(LocalFrame* frame)
{
    frontend()->frameClearedScheduledNavigation(frameId(frame));
}

void InspectorPageAgent::willRunJavaScriptDialog(const String& message, ChromeClient::DialogType dialogType)
{
    frontend()->javascriptDialogOpening(message, dialogTypeToProtocol(dialogType));
}

void InspectorPageAgent::didRunJavaScriptDialog(bool result)
{
    frontend()->javascriptDialogClosed(result);
}

void InspectorPageAgent::didUpdateLayout()
{
    if (m_enabled && m_client)
        m_client->pageLayoutInvalidated(false);
}

void InspectorPageAgent::didResizeMainFrame()
{
    if (!m_inspectedFrames->root()->isMainFrame())
        return;
#if !OS(ANDROID)
    if (m_enabled && m_client)
        m_client->pageLayoutInvalidated(true);
#endif
    frontend()->frameResized();
}

void InspectorPageAgent::didRecalculateStyle()
{
    if (m_enabled && m_client)
        m_client->pageLayoutInvalidated(false);
}

void InspectorPageAgent::windowCreated(LocalFrame* created)
{
    if (m_enabled && m_state->booleanProperty(PageAgentState::autoAttachToCreatedPages, false))
        m_client->waitForCreateWindow(created);
}

PassOwnPtr<protocol::Page::Frame> InspectorPageAgent::buildObjectForFrame(LocalFrame* frame)
{
    OwnPtr<protocol::Page::Frame> frameObject = protocol::Page::Frame::create()
        .setId(frameId(frame))
        .setLoaderId(IdentifiersFactory::loaderId(frame->loader().documentLoader()))
        .setUrl(urlWithoutFragment(frame->document()->url()).getString())
        .setMimeType(frame->loader().documentLoader()->responseMIMEType())
        .setSecurityOrigin(frame->document()->getSecurityOrigin()->toRawString()).build();
    // FIXME: This doesn't work for OOPI.
    Frame* parentFrame = frame->tree().parent();
    if (parentFrame && parentFrame->isLocalFrame())
        frameObject->setParentId(frameId(toLocalFrame(parentFrame)));
    if (frame->deprecatedLocalOwner()) {
        AtomicString name = frame->deprecatedLocalOwner()->getNameAttribute();
        if (name.isEmpty())
            name = frame->deprecatedLocalOwner()->getAttribute(HTMLNames::idAttr);
        frameObject->setName(name);
    }

    return frameObject.release();
}

PassOwnPtr<protocol::Page::FrameResourceTree> InspectorPageAgent::buildObjectForFrameTree(LocalFrame* frame)
{
    OwnPtr<protocol::Page::Frame> frameObject = buildObjectForFrame(frame);
    OwnPtr<protocol::Array<protocol::Page::FrameResource>> subresources = protocol::Array<protocol::Page::FrameResource>::create();

    HeapVector<Member<Resource>> allResources = cachedResourcesForFrame(frame, true);
    for (Resource* cachedResource : allResources) {
        OwnPtr<protocol::Page::FrameResource> resourceObject = protocol::Page::FrameResource::create()
            .setUrl(urlWithoutFragment(cachedResource->url()).getString())
            .setType(cachedResourceTypeJson(*cachedResource))
            .setMimeType(cachedResource->response().mimeType()).build();
        if (cachedResource->wasCanceled())
            resourceObject->setCanceled(true);
        else if (cachedResource->getStatus() == Resource::LoadError)
            resourceObject->setFailed(true);
        subresources->addItem(resourceObject.release());
    }

    HeapVector<Member<Document>> allImports = InspectorPageAgent::importsForFrame(frame);
    for (Document* import : allImports) {
        OwnPtr<protocol::Page::FrameResource> resourceObject = protocol::Page::FrameResource::create()
            .setUrl(urlWithoutFragment(import->url()).getString())
            .setType(resourceTypeJson(InspectorPageAgent::DocumentResource))
            .setMimeType(import->suggestedMIMEType()).build();
        subresources->addItem(resourceObject.release());
    }

    OwnPtr<protocol::Page::FrameResourceTree> result = protocol::Page::FrameResourceTree::create()
        .setFrame(frameObject.release())
        .setResources(subresources.release()).build();

    OwnPtr<protocol::Array<protocol::Page::FrameResourceTree>> childrenArray;
    for (Frame* child = frame->tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (!child->isLocalFrame())
            continue;
        if (!childrenArray)
            childrenArray = protocol::Array<protocol::Page::FrameResourceTree>::create();
        childrenArray->addItem(buildObjectForFrameTree(toLocalFrame(child)));
    }
    result->setChildFrames(childrenArray.release());
    return result.release();
}

void InspectorPageAgent::startScreencast(ErrorString*, const Maybe<String>& format, const Maybe<int>& quality, const Maybe<int>& maxWidth, const Maybe<int>& maxHeight, const Maybe<int>& everyNthFrame)
{
    m_state->setBoolean(PageAgentState::screencastEnabled, true);
}

void InspectorPageAgent::stopScreencast(ErrorString*)
{
    m_state->setBoolean(PageAgentState::screencastEnabled, false);
}

void InspectorPageAgent::setOverlayMessage(ErrorString*, const Maybe<String>& message)
{
    if (m_client)
        m_client->setPausedInDebuggerMessage(message.fromMaybe(String()));
}

DEFINE_TRACE(InspectorPageAgent)
{
    visitor->trace(m_inspectedFrames);
    visitor->trace(m_debuggerAgent);
    visitor->trace(m_inspectorResourceContentLoader);
    InspectorBaseAgent::trace(visitor);
}

} // namespace blink

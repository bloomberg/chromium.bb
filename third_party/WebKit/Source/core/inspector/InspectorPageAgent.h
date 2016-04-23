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

#ifndef InspectorPageAgent_h
#define InspectorPageAgent_h


#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/page/ChromeClient.h"
#include "wtf/HashMap.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Resource;
class Document;
class DocumentLoader;
class InspectedFrames;
class InspectorCSSAgent;
class InspectorDebuggerAgent;
class InspectorResourceContentLoader;
class KURL;
class LocalFrame;
class SharedBuffer;
class TextResourceDecoder;

using blink::protocol::Maybe;

class CORE_EXPORT InspectorPageAgent final : public InspectorBaseAgent<InspectorPageAgent, protocol::Frontend::Page>, public protocol::Backend::Page {
    WTF_MAKE_NONCOPYABLE(InspectorPageAgent);
public:
    class Client {
    public:
        virtual ~Client() { }
        virtual void pageLayoutInvalidated(bool resized) { }
        virtual void setPausedInDebuggerMessage(const String&) { }
        virtual void waitForCreateWindow(LocalFrame*) { }
    };

    enum ResourceType {
        DocumentResource,
        StylesheetResource,
        ImageResource,
        FontResource,
        MediaResource,
        ScriptResource,
        TextTrackResource,
        XHRResource,
        FetchResource,
        EventSourceResource,
        WebSocketResource,
        ManifestResource,
        OtherResource
    };

    static InspectorPageAgent* create(InspectedFrames*, Client*, InspectorResourceContentLoader*, InspectorDebuggerAgent*);

    static HeapVector<Member<Document>> importsForFrame(LocalFrame*);
    static bool cachedResourceContent(Resource*, String* result, bool* base64Encoded);
    static bool sharedBufferContent(PassRefPtr<SharedBuffer>, const String& textEncodingName, bool withBase64Encode, String* result);

    static PassRefPtr<SharedBuffer> resourceData(LocalFrame*, const KURL&, String* textEncodingName);
    static Resource* cachedResource(LocalFrame*, const KURL&);
    static String resourceTypeJson(ResourceType);
    static ResourceType cachedResourceType(const Resource&);
    static String cachedResourceTypeJson(const Resource&);
    static PassOwnPtr<TextResourceDecoder> createResourceTextDecoder(const String& mimeType, const String& textEncodingName);

    // Page API for InspectorFrontend
    void enable(ErrorString*) override;
    void disable(ErrorString*) override;
    void addScriptToEvaluateOnLoad(ErrorString*, const String& scriptSource, String* identifier) override;
    void removeScriptToEvaluateOnLoad(ErrorString*, const String& identifier) override;
    void setAutoAttachToCreatedPages(ErrorString*, bool autoAttach) override;
    void reload(ErrorString*, const Maybe<bool>& bypassCache, const Maybe<String>& scriptToEvaluateOnLoad) override;
    void navigate(ErrorString*, const String& url, String* frameId) override;
    void getResourceTree(ErrorString*, OwnPtr<protocol::Page::FrameResourceTree>* frameTree) override;
    void getResourceContent(ErrorString*, const String& frameId, const String& url, PassOwnPtr<GetResourceContentCallback>) override;
    void searchInResource(ErrorString*, const String& frameId, const String& url, const String& query, const Maybe<bool>& caseSensitive, const Maybe<bool>& isRegex, PassOwnPtr<SearchInResourceCallback>) override;
    void setDocumentContent(ErrorString*, const String& frameId, const String& html) override;
    void startScreencast(ErrorString*, const Maybe<String>& format, const Maybe<int>& quality, const Maybe<int>& maxWidth, const Maybe<int>& maxHeight, const Maybe<int>& everyNthFrame) override;
    void stopScreencast(ErrorString*) override;
    void setOverlayMessage(ErrorString*, const Maybe<String>& message) override;

    // InspectorInstrumentation API
    void didClearDocumentOfWindowObject(LocalFrame*);
    void domContentLoadedEventFired(LocalFrame*);
    void loadEventFired(LocalFrame*);
    void didCommitLoad(LocalFrame*, DocumentLoader*);
    void frameAttachedToParent(LocalFrame*);
    void frameDetachedFromParent(LocalFrame*);
    void frameStartedLoading(LocalFrame*);
    void frameStoppedLoading(LocalFrame*);
    void frameScheduledNavigation(LocalFrame*, double delay);
    void frameClearedScheduledNavigation(LocalFrame*);
    void willRunJavaScriptDialog(const String& message, ChromeClient::DialogType);
    void didRunJavaScriptDialog(bool result);
    void didUpdateLayout();
    void didResizeMainFrame();
    void didRecalculateStyle(int);
    void windowCreated(LocalFrame*);

    // Inspector Controller API
    void restore() override;
    bool screencastEnabled();

    DECLARE_VIRTUAL_TRACE();

private:
    InspectorPageAgent(InspectedFrames*, Client*, InspectorResourceContentLoader*, InspectorDebuggerAgent*);

    void finishReload();
    void getResourceContentAfterResourcesContentLoaded(const String& frameId, const String& url, PassOwnPtr<GetResourceContentCallback>);
    void searchContentAfterResourcesContentLoaded(const String& frameId, const String& url, const String& query, bool caseSensitive, bool isRegex, PassOwnPtr<SearchInResourceCallback>);

    static bool dataContent(const char* data, unsigned size, const String& textEncodingName, bool withBase64Encode, String* result);

    PassOwnPtr<protocol::Page::Frame> buildObjectForFrame(LocalFrame*);
    PassOwnPtr<protocol::Page::FrameResourceTree> buildObjectForFrameTree(LocalFrame*);
    Member<InspectedFrames> m_inspectedFrames;
    Member<InspectorDebuggerAgent> m_debuggerAgent;
    Client* m_client;
    long m_lastScriptIdentifier;
    String m_pendingScriptToEvaluateOnLoadOnce;
    String m_scriptToEvaluateOnLoadOnce;
    bool m_enabled;
    bool m_reloading;
    Member<InspectorResourceContentLoader> m_inspectorResourceContentLoader;
    int m_resourceContentLoaderClientId;
};


} // namespace blink


#endif // !defined(InspectorPagerAgent_h)

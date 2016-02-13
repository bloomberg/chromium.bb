/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorDOMAgent_h
#define InspectorDOMAgent_h

#include "core/CoreExport.h"
#include "core/events/EventListenerMap.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/InspectorHighlight.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/JSONValues.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/v8_inspector/public/V8RuntimeAgent.h"

#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class CharacterData;
class DOMEditor;
class Document;
class DocumentLoader;
class Element;
class EventTarget;
class ExceptionState;
class FloatQuad;
class InsertionPoint;
class InspectedFrames;
class InspectorFrontend;
class InspectorHistory;
class Node;
class QualifiedName;
class PseudoElement;
class PlatformGestureEvent;
class PlatformMouseEvent;
class PlatformTouchEvent;
class InspectorRevalidateDOMTask;
class ShadowRoot;

typedef String ErrorString;

class CORE_EXPORT InspectorDOMAgent final : public InspectorBaseAgent<InspectorDOMAgent, protocol::Frontend::DOM>, public protocol::Dispatcher::DOMCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorDOMAgent);
public:
    struct CORE_EXPORT DOMListener : public WillBeGarbageCollectedMixin {
        virtual ~DOMListener()
        {
        }
        virtual void didRemoveDocument(Document*) = 0;
        virtual void didRemoveDOMNode(Node*) = 0;
        virtual void didModifyDOMAttr(Element*) = 0;
    };

    enum SearchMode { NotSearching, SearchingForNormal, SearchingForUAShadow, ShowLayoutEditor };

    class Client {
    public:
        virtual ~Client() { }
        virtual void hideHighlight() { }
        virtual void highlightNode(Node*, const InspectorHighlightConfig&, bool omitTooltip) { }
        virtual void highlightQuad(PassOwnPtr<FloatQuad>, const InspectorHighlightConfig&) { }
        virtual void setInspectMode(SearchMode searchMode, PassOwnPtr<InspectorHighlightConfig>) { }
        virtual void setInspectedNode(Node*) { }
    };

    static PassOwnPtrWillBeRawPtr<InspectorDOMAgent> create(v8::Isolate* isolate, InspectedFrames* inspectedFrames, V8RuntimeAgent* runtimeAgent, Client* client)
    {
        return adoptPtrWillBeNoop(new InspectorDOMAgent(isolate, inspectedFrames, runtimeAgent, client));
    }

    static String toErrorString(ExceptionState&);
    static bool getPseudoElementType(PseudoId, protocol::TypeBuilder::DOM::PseudoType::Enum*);
    static ShadowRoot* userAgentShadowRoot(Node*);

    ~InspectorDOMAgent() override;
    DECLARE_VIRTUAL_TRACE();

    void disable(ErrorString*) override;
    void restore() override;

    WillBeHeapVector<RawPtrWillBeMember<Document>> documents();
    void reset();

    // Methods called from the frontend for DOM nodes inspection.
    void enable(ErrorString*) override;
    void querySelector(ErrorString*, int nodeId, const String& selectors, int* elementId) override;
    void querySelectorAll(ErrorString*, int nodeId, const String& selectors, RefPtr<protocol::TypeBuilder::Array<int>>& result) override;
    void getDocument(ErrorString*, RefPtr<protocol::TypeBuilder::DOM::Node>& root) override;
    void requestChildNodes(ErrorString*, int nodeId, const int* depth) override;
    void setAttributeValue(ErrorString*, int elementId, const String& name, const String& value) override;
    void setAttributesAsText(ErrorString*, int elementId, const String& text, const String* name) override;
    void removeAttribute(ErrorString*, int elementId, const String& name) override;
    void removeNode(ErrorString*, int nodeId) override;
    void setNodeName(ErrorString*, int nodeId, const String& name, int* newId) override;
    void getOuterHTML(ErrorString*, int nodeId, WTF::String* outerHTML) override;
    void setOuterHTML(ErrorString*, int nodeId, const String& outerHTML) override;
    void setNodeValue(ErrorString*, int nodeId, const String& value) override;
    void performSearch(ErrorString*, const String& whitespaceTrimmedQuery, const bool* includeUserAgentShadowDOM, String* searchId, int* resultCount) override;
    void getSearchResults(ErrorString*, const String& searchId, int fromIndex, int toIndex, RefPtr<protocol::TypeBuilder::Array<int>>&) override;
    void discardSearchResults(ErrorString*, const String& searchId) override;
    void resolveNode(ErrorString*, int nodeId, const String* objectGroup, RefPtr<protocol::TypeBuilder::Runtime::RemoteObject>& result) override;
    void getAttributes(ErrorString*, int nodeId, RefPtr<protocol::TypeBuilder::Array<String>>& result) override;
    void setInspectMode(ErrorString*, const String&, const RefPtr<JSONObject>* highlightConfig) override;
    void requestNode(ErrorString*, const String& objectId, int* nodeId) override;
    void pushNodeByPathToFrontend(ErrorString*, const String& path, int* nodeId) override;
    void pushNodesByBackendIdsToFrontend(ErrorString*, const RefPtr<JSONArray>& nodeIds, RefPtr<protocol::TypeBuilder::Array<int>>&) override;
    void setInspectedNode(ErrorString*, int nodeId) override;
    void hideHighlight(ErrorString*) override;
    void highlightRect(ErrorString*, int x, int y, int width, int height, const RefPtr<JSONObject>* color, const RefPtr<JSONObject>* outlineColor) override;
    void highlightQuad(ErrorString*, const RefPtr<JSONArray>& quad, const RefPtr<JSONObject>* color, const RefPtr<JSONObject>* outlineColor) override;
    void highlightNode(ErrorString*, const RefPtr<JSONObject>& highlightConfig, const int* nodeId, const int* backendNodeId, const String* objectId) override;
    void highlightFrame(ErrorString*, const String& frameId, const RefPtr<JSONObject>* color, const RefPtr<JSONObject>* outlineColor) override;

    void copyTo(ErrorString*, int nodeId, int targetElementId, const int* anchorNodeId, int* newNodeId) override;
    void moveTo(ErrorString*, int nodeId, int targetNodeId, const int* anchorNodeId, int* newNodeId) override;
    void undo(ErrorString*) override;
    void redo(ErrorString*) override;
    void markUndoableState(ErrorString*) override;
    void focus(ErrorString*, int nodeId) override;
    void setFileInputFiles(ErrorString*, int nodeId, const RefPtr<JSONArray>& files) override;
    void getBoxModel(ErrorString*, int nodeId, RefPtr<protocol::TypeBuilder::DOM::BoxModel>&) override;
    void getNodeForLocation(ErrorString*, int x, int y, int* nodeId) override;
    void getRelayoutBoundary(ErrorString*, int nodeId, int* relayoutBoundaryNodeId) override;
    void getHighlightObjectForTest(ErrorString*, int nodeId, RefPtr<JSONObject>&) override;

    bool enabled() const;
    void releaseDanglingNodes();

    // Methods called from the InspectorInstrumentation.
    void domContentLoadedEventFired(LocalFrame*);
    void didCommitLoad(LocalFrame*, DocumentLoader*);
    void didInsertDOMNode(Node*);
    void willRemoveDOMNode(Node*);
    void willModifyDOMAttr(Element*, const AtomicString& oldValue, const AtomicString& newValue);
    void didModifyDOMAttr(Element*, const QualifiedName&, const AtomicString& value);
    void didRemoveDOMAttr(Element*, const QualifiedName&);
    void styleAttributeInvalidated(const WillBeHeapVector<RawPtrWillBeMember<Element>>& elements);
    void characterDataModified(CharacterData*);
    void didInvalidateStyleAttr(Node*);
    void didPushShadowRoot(Element* host, ShadowRoot*);
    void willPopShadowRoot(Element* host, ShadowRoot*);
    void didPerformElementShadowDistribution(Element*);
    void frameDocumentUpdated(LocalFrame*);
    void pseudoElementCreated(PseudoElement*);
    void pseudoElementDestroyed(PseudoElement*);

    Node* nodeForId(int nodeId);
    int boundNodeId(Node*);
    void setDOMListener(DOMListener*);
    void inspect(Node*);
    void nodeHighlightedInOverlay(Node*);

    static String documentURLString(Document*);

    PassRefPtr<protocol::TypeBuilder::Runtime::RemoteObject> resolveNode(Node*, const String& objectGroup);

    InspectorHistory* history() { return m_history.get(); }

    // We represent embedded doms as a part of the same hierarchy. Hence we treat children of frame owners differently.
    // We also skip whitespace text nodes conditionally. Following methods encapsulate these specifics.
    static Node* innerFirstChild(Node*);
    static Node* innerNextSibling(Node*);
    static Node* innerPreviousSibling(Node*);
    static unsigned innerChildNodeCount(Node*);
    static Node* innerParentNode(Node*);
    static bool isWhitespace(Node*);

    Node* assertNode(ErrorString*, int nodeId);
    Element* assertElement(ErrorString*, int nodeId);
    Document* assertDocument(ErrorString*, int nodeId);

private:
    InspectorDOMAgent(v8::Isolate*, InspectedFrames*, V8RuntimeAgent*, Client*);

    void setDocument(Document*);
    void innerEnable();

    void setSearchingForNode(ErrorString*, SearchMode, JSONObject* highlightConfig);
    PassOwnPtr<InspectorHighlightConfig> highlightConfigFromInspectorObject(ErrorString*, JSONObject* highlightInspectorObject);

    // Node-related methods.
    typedef WillBeHeapHashMap<RefPtrWillBeMember<Node>, int> NodeToIdMap;
    int bind(Node*, NodeToIdMap*);
    void unbind(Node*, NodeToIdMap*);

    Node* assertEditableNode(ErrorString*, int nodeId);
    Node* assertEditableChildNode(ErrorString*, Element* parentElement, int nodeId);
    Element* assertEditableElement(ErrorString*, int nodeId);

    int pushNodePathToFrontend(Node*, NodeToIdMap* nodeMap);
    int pushNodePathToFrontend(Node*);
    void pushChildNodesToFrontend(int nodeId, int depth = 1);

    void invalidateFrameOwnerElement(LocalFrame*);

    PassRefPtr<protocol::TypeBuilder::DOM::Node> buildObjectForNode(Node*, int depth, NodeToIdMap*);
    PassRefPtr<protocol::TypeBuilder::Array<String>> buildArrayForElementAttributes(Element*);
    PassRefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::DOM::Node>> buildArrayForContainerChildren(Node* container, int depth, NodeToIdMap* nodesMap);
    PassRefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::DOM::Node>> buildArrayForPseudoElements(Element*, NodeToIdMap* nodesMap);
    PassRefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::DOM::BackendNode>> buildArrayForDistributedNodes(InsertionPoint*);

    Node* nodeForPath(const String& path);
    Node* nodeForRemoteId(ErrorString*, const String& id);

    void discardFrontendBindings();

    void innerHighlightQuad(PassOwnPtr<FloatQuad>, const RefPtr<JSONObject>* color, const RefPtr<JSONObject>* outlineColor);

    bool pushDocumentUponHandlelessOperation(ErrorString*);

    RawPtrWillBeMember<InspectorRevalidateDOMTask> revalidateTask();

    v8::Isolate* m_isolate;
    RawPtrWillBeMember<InspectedFrames> m_inspectedFrames;
    V8RuntimeAgent* m_runtimeAgent;
    Client* m_client;
    RawPtrWillBeMember<DOMListener> m_domListener;
    OwnPtrWillBeMember<NodeToIdMap> m_documentNodeToIdMap;
    // Owns node mappings for dangling nodes.
    WillBeHeapVector<OwnPtrWillBeMember<NodeToIdMap>> m_danglingNodeToIdMaps;
    WillBeHeapHashMap<int, RawPtrWillBeMember<Node>> m_idToNode;
    WillBeHeapHashMap<int, RawPtrWillBeMember<NodeToIdMap>> m_idToNodesMap;
    HashSet<int> m_childrenRequested;
    HashSet<int> m_distributedNodesRequested;
    HashMap<int, int> m_cachedChildCount;
    int m_lastNodeId;
    RefPtrWillBeMember<Document> m_document;
    typedef WillBeHeapHashMap<String, WillBeHeapVector<RefPtrWillBeMember<Node>>> SearchResults;
    SearchResults m_searchResults;
    OwnPtrWillBeMember<InspectorRevalidateDOMTask> m_revalidateTask;
    OwnPtrWillBeMember<InspectorHistory> m_history;
    OwnPtrWillBeMember<DOMEditor> m_domEditor;
    bool m_suppressAttributeModifiedEvent;
    int m_backendNodeIdToInspect;
};


} // namespace blink

#endif // !defined(InspectorDOMAgent_h)

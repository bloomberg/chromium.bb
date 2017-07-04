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

#include <memory>
#include "core/CoreExport.h"
#include "core/events/EventListenerMap.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/DOM.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/AtomicString.h"
#include "v8/include/v8-inspector.h"

namespace blink {

class CharacterData;
class Color;
class DOMEditor;
class Document;
class DocumentLoader;
class Element;
class ExceptionState;
class FloatQuad;
class HTMLSlotElement;
class V0InsertionPoint;
class InspectedFrames;
class InspectorHistory;
class Node;
class QualifiedName;
class PseudoElement;
class InspectorRevalidateDOMTask;
class ShadowRoot;

class CORE_EXPORT InspectorDOMAgent final
    : public InspectorBaseAgent<protocol::DOM::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorDOMAgent);

 public:
  struct CORE_EXPORT DOMListener : public GarbageCollectedMixin {
    virtual ~DOMListener() {}
    virtual void DidAddDocument(Document*) = 0;
    virtual void DidRemoveDocument(Document*) = 0;
    virtual void DidRemoveDOMNode(Node*) = 0;
    virtual void DidModifyDOMAttr(Element*) = 0;
  };

  static protocol::Response ToResponse(ExceptionState&);
  static bool GetPseudoElementType(PseudoId, String*);
  static ShadowRoot* UserAgentShadowRoot(Node*);
  static Color ParseColor(protocol::DOM::RGBA*);

  InspectorDOMAgent(v8::Isolate*,
                    InspectedFrames*,
                    v8_inspector::V8InspectorSession*);
  ~InspectorDOMAgent() override;
  DECLARE_VIRTUAL_TRACE();

  void Restore() override;

  HeapVector<Member<Document>> Documents();
  void Reset();

  // Methods called from the frontend for DOM nodes inspection.
  protocol::Response enable() override;
  protocol::Response disable() override;
  protocol::Response getDocument(
      protocol::Maybe<int> depth,
      protocol::Maybe<bool> traverse_frames,
      std::unique_ptr<protocol::DOM::Node>* root) override;
  protocol::Response getFlattenedDocument(
      protocol::Maybe<int> depth,
      protocol::Maybe<bool> pierce,
      std::unique_ptr<protocol::Array<protocol::DOM::Node>>* nodes) override;
  protocol::Response collectClassNamesFromSubtree(
      int node_id,
      std::unique_ptr<protocol::Array<String>>* class_names) override;
  protocol::Response requestChildNodes(
      int node_id,
      protocol::Maybe<int> depth,
      protocol::Maybe<bool> traverse_frames) override;
  protocol::Response querySelector(int node_id,
                                   const String& selector,
                                   int* out_node_id) override;
  protocol::Response querySelectorAll(
      int node_id,
      const String& selector,
      std::unique_ptr<protocol::Array<int>>* node_ids) override;
  protocol::Response setNodeName(int node_id,
                                 const String& name,
                                 int* out_node_id) override;
  protocol::Response setNodeValue(int node_id, const String& value) override;
  protocol::Response removeNode(int node_id) override;
  protocol::Response setAttributeValue(int node_id,
                                       const String& name,
                                       const String& value) override;
  protocol::Response setAttributesAsText(int node_id,
                                         const String& text,
                                         protocol::Maybe<String> name) override;
  protocol::Response removeAttribute(int node_id, const String& name) override;
  protocol::Response getOuterHTML(int node_id, String* outer_html) override;
  protocol::Response setOuterHTML(int node_id,
                                  const String& outer_html) override;
  protocol::Response performSearch(
      const String& query,
      protocol::Maybe<bool> include_user_agent_shadow_dom,
      String* search_id,
      int* result_count) override;
  protocol::Response getSearchResults(
      const String& search_id,
      int from_index,
      int to_index,
      std::unique_ptr<protocol::Array<int>>* node_ids) override;
  protocol::Response discardSearchResults(const String& search_id) override;
  protocol::Response requestNode(const String& object_id,
                                 int* out_node_id) override;
  protocol::Response pushNodeByPathToFrontend(const String& path,
                                              int* out_node_id) override;
  protocol::Response pushNodesByBackendIdsToFrontend(
      std::unique_ptr<protocol::Array<int>> backend_node_ids,
      std::unique_ptr<protocol::Array<int>>* node_ids) override;
  protocol::Response setInspectedNode(int node_id) override;
  protocol::Response resolveNode(
      int node_id,
      protocol::Maybe<String> object_group,
      std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>*)
      override;
  protocol::Response getAttributes(
      int node_id,
      std::unique_ptr<protocol::Array<String>>* attributes) override;
  protocol::Response copyTo(int node_id,
                            int target_node_id,
                            protocol::Maybe<int> insert_before_node_id,
                            int* out_node_id) override;
  protocol::Response moveTo(int node_id,
                            int target_node_id,
                            protocol::Maybe<int> insert_before_node_id,
                            int* out_node_id) override;
  protocol::Response undo() override;
  protocol::Response redo() override;
  protocol::Response markUndoableState() override;
  protocol::Response focus(int node_id) override;
  protocol::Response setFileInputFiles(
      int node_id,
      std::unique_ptr<protocol::Array<String>> files) override;
  protocol::Response getBoxModel(
      int node_id,
      std::unique_ptr<protocol::DOM::BoxModel>*) override;
  protocol::Response getNodeForLocation(
      int x,
      int y,
      protocol::Maybe<bool> include_user_agent_shadow_dom,
      int* out_node_id) override;
  protocol::Response getRelayoutBoundary(int node_id,
                                         int* out_node_id) override;

  bool Enabled() const;
  void ReleaseDanglingNodes();

  // Methods called from the InspectorInstrumentation.
  void DomContentLoadedEventFired(LocalFrame*);
  void DidCommitLoad(LocalFrame*, DocumentLoader*);
  void DidInsertDOMNode(Node*);
  void WillRemoveDOMNode(Node*);
  void WillModifyDOMAttr(Element*,
                         const AtomicString& old_value,
                         const AtomicString& new_value);
  void DidModifyDOMAttr(Element*,
                        const QualifiedName&,
                        const AtomicString& value);
  void DidRemoveDOMAttr(Element*, const QualifiedName&);
  void StyleAttributeInvalidated(const HeapVector<Member<Element>>& elements);
  void CharacterDataModified(CharacterData*);
  void DidInvalidateStyleAttr(Node*);
  void DidPushShadowRoot(Element* host, ShadowRoot*);
  void WillPopShadowRoot(Element* host, ShadowRoot*);
  void DidPerformElementShadowDistribution(Element*);
  void DidPerformSlotDistribution(HTMLSlotElement*);
  void FrameDocumentUpdated(LocalFrame*);
  void PseudoElementCreated(PseudoElement*);
  void PseudoElementDestroyed(PseudoElement*);

  Node* NodeForId(int node_id);
  int BoundNodeId(Node*);
  void SetDOMListener(DOMListener*);
  int PushNodePathToFrontend(Node*);
  protocol::Response PushDocumentUponHandlelessOperation();
  protocol::Response NodeForRemoteObjectId(const String& remote_object_id,
                                           Node*&);

  static String DocumentURLString(Document*);
  static String DocumentBaseURLString(Document*);

  std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>
  ResolveNode(Node*, const String& object_group);

  InspectorHistory* History() { return history_.Get(); }

  // We represent embedded doms as a part of the same hierarchy. Hence we treat
  // children of frame owners differently.  We also skip whitespace text nodes
  // conditionally. Following methods encapsulate these specifics.
  static Node* InnerFirstChild(Node*);
  static Node* InnerNextSibling(Node*);
  static Node* InnerPreviousSibling(Node*);
  static unsigned InnerChildNodeCount(Node*);
  static Node* InnerParentNode(Node*);
  static bool IsWhitespace(Node*);
  static v8::Local<v8::Value> NodeV8Value(v8::Local<v8::Context>, Node*);
  static void CollectNodes(Node* root,
                           int depth,
                           bool pierce,
                           Function<bool(Node*)>*,
                           HeapVector<Member<Node>>* result);

  protocol::Response AssertNode(int node_id, Node*&);
  protocol::Response AssertElement(int node_id, Element*&);
  Document* GetDocument() const { return document_.Get(); }

 private:
  void SetDocument(Document*);
  void InnerEnable();

  // Node-related methods.
  typedef HeapHashMap<Member<Node>, int> NodeToIdMap;
  int Bind(Node*, NodeToIdMap*);
  void Unbind(Node*, NodeToIdMap*);

  protocol::Response AssertEditableNode(int node_id, Node*&);
  protocol::Response AssertEditableChildNode(Element* parent_element,
                                             int node_id,
                                             Node*&);
  protocol::Response AssertEditableElement(int node_id, Element*&);

  int PushNodePathToFrontend(Node*, NodeToIdMap* node_map);
  void PushChildNodesToFrontend(int node_id,
                                int depth = 1,
                                bool traverse_frames = false);

  void InvalidateFrameOwnerElement(LocalFrame*);

  std::unique_ptr<protocol::DOM::Node> BuildObjectForNode(
      Node*,
      int depth,
      bool traverse_frames,
      NodeToIdMap*,
      protocol::Array<protocol::DOM::Node>* flatten_result = nullptr);
  std::unique_ptr<protocol::Array<String>> BuildArrayForElementAttributes(
      Element*);
  std::unique_ptr<protocol::Array<protocol::DOM::Node>>
  BuildArrayForContainerChildren(
      Node* container,
      int depth,
      bool traverse_frames,
      NodeToIdMap* nodes_map,
      protocol::Array<protocol::DOM::Node>* flatten_result);
  std::unique_ptr<protocol::Array<protocol::DOM::Node>>
  BuildArrayForPseudoElements(Element*, NodeToIdMap* nodes_map);
  std::unique_ptr<protocol::Array<protocol::DOM::BackendNode>>
  BuildArrayForDistributedNodes(V0InsertionPoint*);
  std::unique_ptr<protocol::Array<protocol::DOM::BackendNode>>
  BuildDistributedNodesForSlot(HTMLSlotElement*);

  Node* NodeForPath(const String& path);

  void DiscardFrontendBindings();

  InspectorRevalidateDOMTask* RevalidateTask();

  v8::Isolate* isolate_;
  Member<InspectedFrames> inspected_frames_;
  v8_inspector::V8InspectorSession* v8_session_;
  Member<DOMListener> dom_listener_;
  Member<NodeToIdMap> document_node_to_id_map_;
  // Owns node mappings for dangling nodes.
  HeapVector<Member<NodeToIdMap>> dangling_node_to_id_maps_;
  HeapHashMap<int, Member<Node>> id_to_node_;
  HeapHashMap<int, Member<NodeToIdMap>> id_to_nodes_map_;
  HashSet<int> children_requested_;
  HashSet<int> distributed_nodes_requested_;
  HashMap<int, int> cached_child_count_;
  int last_node_id_;
  Member<Document> document_;
  typedef HeapHashMap<String, HeapVector<Member<Node>>> SearchResults;
  SearchResults search_results_;
  Member<InspectorRevalidateDOMTask> revalidate_task_;
  Member<InspectorHistory> history_;
  Member<DOMEditor> dom_editor_;
  bool suppress_attribute_modified_event_;
};

}  // namespace blink

#endif  // !defined(InspectorDOMAgent_h)

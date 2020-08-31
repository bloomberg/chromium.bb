/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2009 Joseph Pecoraro
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

#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"

#include <memory>
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_file.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/character_data.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/shadow_root_v0.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/v0_insertion_point.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/imports/html_import_child.h"
#include "third_party/blink/renderer/core/html/imports/html_import_loader.h"
#include "third_party/blink/renderer/core/html/portal/document_portals.h"
#include "third_party/blink/renderer/core/html/portal/html_portal_element.h"
#include "third_party/blink/renderer/core/html/portal/portal_contents.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/inspector/dom_editor.h"
#include "third_party/blink/renderer/core/inspector/dom_patch_support.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_highlight.h"
#include "third_party/blink/renderer/core/inspector/inspector_history.h"
#include "third_party/blink/renderer/core/inspector/resolve_node.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/xml/document_xpath_evaluator.h"
#include "third_party/blink/renderer/core/xml/xpath_result.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using protocol::Maybe;
using protocol::Response;

namespace {

const size_t kMaxTextSize = 10000;
const UChar kEllipsisUChar[] = {0x2026, 0};

}  // namespace

class InspectorRevalidateDOMTask final
    : public GarbageCollected<InspectorRevalidateDOMTask> {
 public:
  explicit InspectorRevalidateDOMTask(InspectorDOMAgent*);
  void ScheduleStyleAttrRevalidationFor(Element*);
  void Reset() { timer_.Stop(); }
  void OnTimer(TimerBase*);
  void Trace(Visitor*);

 private:
  Member<InspectorDOMAgent> dom_agent_;
  TaskRunnerTimer<InspectorRevalidateDOMTask> timer_;
  HeapHashSet<Member<Element>> style_attr_invalidated_elements_;
};

InspectorRevalidateDOMTask::InspectorRevalidateDOMTask(
    InspectorDOMAgent* dom_agent)
    : dom_agent_(dom_agent),
      timer_(
          dom_agent->GetDocument()->GetTaskRunner(TaskType::kDOMManipulation),
          this,
          &InspectorRevalidateDOMTask::OnTimer) {}

void InspectorRevalidateDOMTask::ScheduleStyleAttrRevalidationFor(
    Element* element) {
  style_attr_invalidated_elements_.insert(element);
  if (!timer_.IsActive())
    timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void InspectorRevalidateDOMTask::OnTimer(TimerBase*) {
  // The timer is stopped on m_domAgent destruction, so this method will never
  // be called after m_domAgent has been destroyed.
  HeapVector<Member<Element>> elements;
  for (auto& attribute : style_attr_invalidated_elements_)
    elements.push_back(attribute.Get());
  dom_agent_->StyleAttributeInvalidated(elements);
  style_attr_invalidated_elements_.clear();
}

void InspectorRevalidateDOMTask::Trace(Visitor* visitor) {
  visitor->Trace(dom_agent_);
  visitor->Trace(style_attr_invalidated_elements_);
}

Response InspectorDOMAgent::ToResponse(ExceptionState& exception_state) {
  if (exception_state.HadException()) {
    String name_prefix = IsDOMExceptionCode(exception_state.Code())
                             ? DOMException::GetErrorName(
                                   exception_state.CodeAs<DOMExceptionCode>()) +
                                   " "
                             : g_empty_string;
    String msg = name_prefix + exception_state.Message();
    return Response::ServerError(msg.Utf8());
  }
  return Response::Success();
}

protocol::DOM::PseudoType InspectorDOMAgent::ProtocolPseudoElementType(
    PseudoId pseudo_id) {
  switch (pseudo_id) {
    case kPseudoIdFirstLine:
      return protocol::DOM::PseudoTypeEnum::FirstLine;
    case kPseudoIdFirstLetter:
      return protocol::DOM::PseudoTypeEnum::FirstLetter;
    case kPseudoIdBefore:
      return protocol::DOM::PseudoTypeEnum::Before;
    case kPseudoIdAfter:
      return protocol::DOM::PseudoTypeEnum::After;
    case kPseudoIdMarker:
      return protocol::DOM::PseudoTypeEnum::Marker;
    case kPseudoIdBackdrop:
      return protocol::DOM::PseudoTypeEnum::Backdrop;
    case kPseudoIdSelection:
      return protocol::DOM::PseudoTypeEnum::Selection;
    case kPseudoIdFirstLineInherited:
      return protocol::DOM::PseudoTypeEnum::FirstLineInherited;
    case kPseudoIdScrollbar:
      return protocol::DOM::PseudoTypeEnum::Scrollbar;
    case kPseudoIdScrollbarThumb:
      return protocol::DOM::PseudoTypeEnum::ScrollbarThumb;
    case kPseudoIdScrollbarButton:
      return protocol::DOM::PseudoTypeEnum::ScrollbarButton;
    case kPseudoIdScrollbarTrack:
      return protocol::DOM::PseudoTypeEnum::ScrollbarTrack;
    case kPseudoIdScrollbarTrackPiece:
      return protocol::DOM::PseudoTypeEnum::ScrollbarTrackPiece;
    case kPseudoIdScrollbarCorner:
      return protocol::DOM::PseudoTypeEnum::ScrollbarCorner;
    case kPseudoIdResizer:
      return protocol::DOM::PseudoTypeEnum::Resizer;
    case kPseudoIdInputListButton:
      return protocol::DOM::PseudoTypeEnum::InputListButton;
    case kAfterLastInternalPseudoId:
    case kPseudoIdNone:
      CHECK(false);
      return "";
  }
}

// static
Color InspectorDOMAgent::ParseColor(protocol::DOM::RGBA* rgba) {
  if (!rgba)
    return Color::kTransparent;

  int r = rgba->getR();
  int g = rgba->getG();
  int b = rgba->getB();
  if (!rgba->hasA())
    return Color(r, g, b);

  double a = rgba->getA(1);
  // Clamp alpha to the [0..1] range.
  if (a < 0)
    a = 0;
  else if (a > 1)
    a = 1;

  return Color(r, g, b, static_cast<int>(a * 255));
}

InspectorDOMAgent::InspectorDOMAgent(
    v8::Isolate* isolate,
    InspectedFrames* inspected_frames,
    v8_inspector::V8InspectorSession* v8_session)
    : isolate_(isolate),
      inspected_frames_(inspected_frames),
      v8_session_(v8_session),
      dom_listener_(nullptr),
      document_node_to_id_map_(MakeGarbageCollected<NodeToIdMap>()),
      last_node_id_(1),
      suppress_attribute_modified_event_(false),
      enabled_(&agent_state_, /*default_value=*/false),
      capture_node_stack_traces_(&agent_state_, /*default_value=*/false) {}

InspectorDOMAgent::~InspectorDOMAgent() = default;

void InspectorDOMAgent::Restore() {
  if (enabled_.Get())
    EnableAndReset();
}

HeapVector<Member<Document>> InspectorDOMAgent::Documents() {
  HeapVector<Member<Document>> result;
  if (document_) {
    for (LocalFrame* frame : *inspected_frames_) {
      if (Document* document = frame->GetDocument())
        result.push_back(document);
    }
  }
  return result;
}

void InspectorDOMAgent::SetDOMListener(DOMListener* listener) {
  dom_listener_ = listener;
}

void InspectorDOMAgent::SetDocument(Document* doc) {
  if (doc == document_.Get())
    return;

  DiscardFrontendBindings();
  document_ = doc;

  if (!enabled_.Get())
    return;

  // Immediately communicate 0 document or document that has finished loading.
  if (!doc || !doc->Parsing())
    GetFrontend()->documentUpdated();
}

bool InspectorDOMAgent::Enabled() const {
  return enabled_.Get();
}

void InspectorDOMAgent::ReleaseDanglingNodes() {
  dangling_node_to_id_maps_.clear();
}

int InspectorDOMAgent::Bind(Node* node, NodeToIdMap* nodes_map) {
  if (!nodes_map)
    return 0;
  int id = nodes_map->at(node);
  if (id)
    return id;
  id = last_node_id_++;
  nodes_map->Set(node, id);
  id_to_node_.Set(id, node);
  id_to_nodes_map_.Set(id, nodes_map);
  return id;
}

void InspectorDOMAgent::Unbind(Node* node, NodeToIdMap* nodes_map) {
  int id = nodes_map->at(node);
  if (!id)
    return;

  id_to_node_.erase(id);
  id_to_nodes_map_.erase(id);

  if (IsA<Document>(node) && dom_listener_)
    dom_listener_->DidRemoveDocument(To<Document>(node));

  if (auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(node)) {
    Document* content_document = frame_owner->contentDocument();
    if (content_document)
      Unbind(content_document, nodes_map);
  }

  if (ShadowRoot* root = node->GetShadowRoot())
    Unbind(root, nodes_map);

  auto* element = DynamicTo<Element>(node);
  if (element) {
    if (element->GetPseudoElement(kPseudoIdBefore))
      Unbind(element->GetPseudoElement(kPseudoIdBefore), nodes_map);
    if (element->GetPseudoElement(kPseudoIdAfter))
      Unbind(element->GetPseudoElement(kPseudoIdAfter), nodes_map);
    if (element->GetPseudoElement(kPseudoIdMarker))
      Unbind(element->GetPseudoElement(kPseudoIdMarker), nodes_map);

    if (auto* link_element = DynamicTo<HTMLLinkElement>(*element)) {
      if (link_element->IsImport() && link_element->import())
        Unbind(link_element->import(), nodes_map);
    }
  }

  nodes_map->erase(node);
  if (dom_listener_)
    dom_listener_->DidRemoveDOMNode(node);

  bool children_requested = children_requested_.Contains(id);
  if (children_requested) {
    // Unbind subtree known to client recursively.
    children_requested_.erase(id);
    Node* child = InnerFirstChild(node);
    while (child) {
      Unbind(child, nodes_map);
      child = InnerNextSibling(child);
    }
  }
  if (nodes_map == document_node_to_id_map_.Get())
    cached_child_count_.erase(id);
}

Response InspectorDOMAgent::AssertNode(int node_id, Node*& node) {
  node = NodeForId(node_id);
  if (!node)
    return Response::ServerError("Could not find node with given id");
  return Response::Success();
}

Response InspectorDOMAgent::AssertNode(
    const protocol::Maybe<int>& node_id,
    const protocol::Maybe<int>& backend_node_id,
    const protocol::Maybe<String>& object_id,
    Node*& node) {
  if (node_id.isJust())
    return AssertNode(node_id.fromJust(), node);

  if (backend_node_id.isJust()) {
    node = DOMNodeIds::NodeForId(backend_node_id.fromJust());
    return !node ? Response::ServerError("No node found for given backend id")
                 : Response::Success();
  }

  if (object_id.isJust())
    return NodeForRemoteObjectId(object_id.fromJust(), node);

  return Response::ServerError(
      "Either nodeId, backendNodeId or objectId must be specified");
}

Response InspectorDOMAgent::AssertElement(int node_id, Element*& element) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  element = DynamicTo<Element>(node);
  if (!element)
    return Response::ServerError("Node is not an Element");
  return Response::Success();
}

// static
ShadowRoot* InspectorDOMAgent::UserAgentShadowRoot(Node* node) {
  if (!node || !node->IsInShadowTree())
    return nullptr;

  Node* candidate = node;
  while (candidate && !IsA<ShadowRoot>(candidate))
    candidate = candidate->ParentOrShadowHostNode();
  DCHECK(candidate);
  ShadowRoot* shadow_root = To<ShadowRoot>(candidate);

  return shadow_root->IsUserAgent() ? shadow_root : nullptr;
}

Response InspectorDOMAgent::AssertEditableNode(int node_id, Node*& node) {
  Response response = AssertNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  if (node->IsInShadowTree()) {
    if (IsA<ShadowRoot>(node))
      return Response::ServerError("Cannot edit shadow roots");
    if (UserAgentShadowRoot(node)) {
      return Response::ServerError(
          "Cannot edit nodes from user-agent shadow trees");
    }
  }

  if (node->IsPseudoElement())
    return Response::ServerError("Cannot edit pseudo elements");
  return Response::Success();
}

Response InspectorDOMAgent::AssertEditableChildNode(Element* parent_element,
                                                    int node_id,
                                                    Node*& node) {
  Response response = AssertEditableNode(node_id, node);
  if (!response.IsSuccess())
    return response;
  if (node->parentNode() != parent_element) {
    return Response::ServerError(
        "Anchor node must be child of the target element");
  }
  return Response::Success();
}

Response InspectorDOMAgent::AssertEditableElement(int node_id,
                                                  Element*& element) {
  Response response = AssertElement(node_id, element);
  if (!response.IsSuccess())
    return response;
  if (element->IsInShadowTree() && UserAgentShadowRoot(element)) {
    return Response::ServerError(
        "Cannot edit elements from user-agent shadow trees");
  }
  if (element->IsPseudoElement())
    return Response::ServerError("Cannot edit pseudo elements");

  return Response::Success();
}

void InspectorDOMAgent::EnableAndReset() {
  enabled_.Set(true);
  history_ = MakeGarbageCollected<InspectorHistory>();
  dom_editor_ = MakeGarbageCollected<DOMEditor>(history_.Get());
  document_ = inspected_frames_->Root()->GetDocument();
  instrumenting_agents_->AddInspectorDOMAgent(this);
}

Response InspectorDOMAgent::enable() {
  if (!enabled_.Get())
    EnableAndReset();
  return Response::Success();
}

Response InspectorDOMAgent::disable() {
  if (!enabled_.Get())
    return Response::ServerError("DOM agent hasn't been enabled");
  enabled_.Clear();
  instrumenting_agents_->RemoveInspectorDOMAgent(this);
  history_.Clear();
  dom_editor_.Clear();
  SetDocument(nullptr);
  return Response::Success();
}

Response InspectorDOMAgent::getDocument(
    Maybe<int> depth,
    Maybe<bool> pierce,
    std::unique_ptr<protocol::DOM::Node>* root) {
  // Backward compatibility. Mark agent as enabled when it requests document.
  enable();

  if (!document_)
    return Response::ServerError("Document is not available");

  DiscardFrontendBindings();

  int sanitized_depth = depth.fromMaybe(2);
  if (sanitized_depth == -1)
    sanitized_depth = INT_MAX;

  *root = BuildObjectForNode(document_.Get(), sanitized_depth,
                             pierce.fromMaybe(false),
                             document_node_to_id_map_.Get());
  return Response::Success();
}

Response InspectorDOMAgent::getFlattenedDocument(
    Maybe<int> depth,
    Maybe<bool> pierce,
    std::unique_ptr<protocol::Array<protocol::DOM::Node>>* nodes) {
  if (!enabled_.Get())
    return Response::ServerError("DOM agent hasn't been enabled");

  if (!document_)
    return Response::ServerError("Document is not available");

  DiscardFrontendBindings();

  int sanitized_depth = depth.fromMaybe(-1);
  if (sanitized_depth == -1)
    sanitized_depth = INT_MAX;

  nodes->reset(new protocol::Array<protocol::DOM::Node>());
  (*nodes)->emplace_back(BuildObjectForNode(
      document_.Get(), sanitized_depth, pierce.fromMaybe(false),
      document_node_to_id_map_.Get(), nodes->get()));
  return Response::Success();
}

void InspectorDOMAgent::PushChildNodesToFrontend(int node_id,
                                                 int depth,
                                                 bool pierce) {
  Node* node = NodeForId(node_id);
  if (!node || (!node->IsElementNode() && !node->IsDocumentNode() &&
                !node->IsDocumentFragment()))
    return;

  NodeToIdMap* node_map = id_to_nodes_map_.at(node_id);

  if (children_requested_.Contains(node_id)) {
    if (depth <= 1)
      return;

    depth--;

    for (node = InnerFirstChild(node); node; node = InnerNextSibling(node)) {
      int child_node_id = node_map->at(node);
      DCHECK(child_node_id);
      PushChildNodesToFrontend(child_node_id, depth, pierce);
    }

    return;
  }

  std::unique_ptr<protocol::Array<protocol::DOM::Node>> children =
      BuildArrayForContainerChildren(node, depth, pierce, node_map, nullptr);
  GetFrontend()->setChildNodes(node_id, std::move(children));
}

void InspectorDOMAgent::DiscardFrontendBindings() {
  if (history_)
    history_->Reset();
  search_results_.clear();
  document_node_to_id_map_->clear();
  id_to_node_.clear();
  id_to_nodes_map_.clear();
  ReleaseDanglingNodes();
  children_requested_.clear();
  cached_child_count_.clear();
  if (revalidate_task_)
    revalidate_task_->Reset();
}

Node* InspectorDOMAgent::NodeForId(int id) {
  if (!id)
    return nullptr;

  HeapHashMap<int, Member<Node>>::iterator it = id_to_node_.find(id);
  if (it != id_to_node_.end())
    return it->value;
  return nullptr;
}

Response InspectorDOMAgent::collectClassNamesFromSubtree(
    int node_id,
    std::unique_ptr<protocol::Array<String>>* class_names) {
  HashSet<String> unique_names;
  *class_names = std::make_unique<protocol::Array<String>>();
  Node* parent_node = NodeForId(node_id);
  auto* parent_element = DynamicTo<Element>(parent_node);
  if (!parent_element && !parent_node->IsDocumentNode() &&
      !parent_node->IsDocumentFragment())
    return Response::ServerError("No suitable node with given id found");

  for (Node* node = parent_node; node;
       node = FlatTreeTraversal::Next(*node, parent_node)) {
    if (const auto* element = DynamicTo<Element>(node)) {
      if (!element->HasClass())
        continue;
      const SpaceSplitString& class_name_list = element->ClassNames();
      for (unsigned i = 0; i < class_name_list.size(); ++i)
        unique_names.insert(class_name_list[i]);
    }
  }
  for (const String& class_name : unique_names)
    (*class_names)->emplace_back(class_name);
  return Response::Success();
}

Response InspectorDOMAgent::requestChildNodes(
    int node_id,
    Maybe<int> depth,
    Maybe<bool> maybe_taverse_frames) {
  int sanitized_depth = depth.fromMaybe(1);
  if (sanitized_depth == 0 || sanitized_depth < -1) {
    return Response::ServerError(
        "Please provide a positive integer as a depth or -1 for entire "
        "subtree");
  }
  if (sanitized_depth == -1)
    sanitized_depth = INT_MAX;

  PushChildNodesToFrontend(node_id, sanitized_depth,
                           maybe_taverse_frames.fromMaybe(false));
  return Response::Success();
}

Response InspectorDOMAgent::querySelector(int node_id,
                                          const String& selectors,
                                          int* element_id) {
  *element_id = 0;
  Node* node = nullptr;
  Response response = AssertNode(node_id, node);
  if (!response.IsSuccess())
    return response;
  auto* container_node = DynamicTo<ContainerNode>(node);
  if (!container_node)
    return Response::ServerError("Not a container node");

  DummyExceptionStateForTesting exception_state;
  Element* element =
      container_node->QuerySelector(AtomicString(selectors), exception_state);
  if (exception_state.HadException())
    return Response::ServerError("DOM Error while querying");

  if (element)
    *element_id = PushNodePathToFrontend(element);
  return Response::Success();
}

Response InspectorDOMAgent::querySelectorAll(
    int node_id,
    const String& selectors,
    std::unique_ptr<protocol::Array<int>>* result) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, node);
  if (!response.IsSuccess())
    return response;
  auto* container_node = DynamicTo<ContainerNode>(node);
  if (!container_node)
    return Response::ServerError("Not a container node");

  DummyExceptionStateForTesting exception_state;
  StaticElementList* elements = container_node->QuerySelectorAll(
      AtomicString(selectors), exception_state);
  if (exception_state.HadException())
    return Response::ServerError("DOM Error while querying");

  *result = std::make_unique<protocol::Array<int>>();

  for (unsigned i = 0; i < elements->length(); ++i)
    (*result)->emplace_back(PushNodePathToFrontend(elements->item(i)));
  return Response::Success();
}

int InspectorDOMAgent::PushNodePathToFrontend(Node* node_to_push,
                                              NodeToIdMap* node_map) {
  DCHECK(node_to_push);  // Invalid input
  // InspectorDOMAgent might have been resetted already. See crbug.com/450491
  if (!document_)
    return 0;
  if (!document_node_to_id_map_->Contains(document_))
    return 0;

  // Return id in case the node is known.
  int result = node_map->at(node_to_push);
  if (result)
    return result;

  Node* node = node_to_push;
  HeapVector<Member<Node>> path;

  while (true) {
    Node* parent = InnerParentNode(node);
    if (!parent)
      return 0;
    path.push_back(parent);
    if (node_map->at(parent))
      break;
    node = parent;
  }

  for (int i = path.size() - 1; i >= 0; --i) {
    int node_id = node_map->at(path.at(i).Get());
    DCHECK(node_id);
    PushChildNodesToFrontend(node_id);
  }
  return node_map->at(node_to_push);
}

int InspectorDOMAgent::PushNodePathToFrontend(Node* node_to_push) {
  if (!document_)
    return 0;

  int node_id =
      PushNodePathToFrontend(node_to_push, document_node_to_id_map_.Get());
  if (node_id)
    return node_id;

  Node* node = node_to_push;
  while (Node* parent = InnerParentNode(node))
    node = parent;

  // Node being pushed is detached -> push subtree root.
  NodeToIdMap* new_map = MakeGarbageCollected<NodeToIdMap>();
  NodeToIdMap* dangling_map = new_map;
  dangling_node_to_id_maps_.push_back(new_map);
  auto children = std::make_unique<protocol::Array<protocol::DOM::Node>>();
  children->emplace_back(BuildObjectForNode(node, 0, false, dangling_map));
  GetFrontend()->setChildNodes(0, std::move(children));

  return PushNodePathToFrontend(node_to_push, dangling_map);
}

int InspectorDOMAgent::BoundNodeId(Node* node) {
  return document_node_to_id_map_->at(node);
}

Response InspectorDOMAgent::setAttributeValue(int element_id,
                                              const String& name,
                                              const String& value) {
  Element* element = nullptr;
  Response response = AssertEditableElement(element_id, element);
  if (!response.IsSuccess())
    return response;
  return dom_editor_->SetAttribute(element, name, value);
}

Response InspectorDOMAgent::setAttributesAsText(int element_id,
                                                const String& text,
                                                Maybe<String> name) {
  Element* element = nullptr;
  Response response = AssertEditableElement(element_id, element);
  if (!response.IsSuccess())
    return response;

  String markup = "<span " + text + "></span>";
  DocumentFragment* fragment = element->GetDocument().createDocumentFragment();

  bool should_ignore_case =
      IsA<HTMLDocument>(element->GetDocument()) && element->IsHTMLElement();
  // Not all elements can represent the context (i.e. IFRAME), hence using
  // document.body.
  if (should_ignore_case && element->GetDocument().body()) {
    fragment->ParseHTML(markup, element->GetDocument().body(),
                        kAllowScriptingContent);
  } else {
    Element* contextElement = nullptr;
    if (auto* svg_element = DynamicTo<SVGElement>(element))
      contextElement = svg_element->ownerSVGElement();
    fragment->ParseXML(markup, contextElement, kAllowScriptingContent);
  }

  Element* parsed_element = DynamicTo<Element>(fragment->firstChild());
  if (!parsed_element)
    return Response::ServerError("Could not parse value as attributes");

  String case_adjusted_name = should_ignore_case
                                  ? name.fromMaybe("").DeprecatedLower()
                                  : name.fromMaybe("");

  AttributeCollection attributes = parsed_element->Attributes();
  if (attributes.IsEmpty() && name.isJust())
    return dom_editor_->RemoveAttribute(element, case_adjusted_name);

  bool found_original_attribute = false;
  for (auto& attribute : attributes) {
    // Add attribute pair
    String attribute_name = attribute.GetName().ToString();
    if (should_ignore_case)
      attribute_name = attribute_name.DeprecatedLower();
    found_original_attribute |=
        name.isJust() && attribute_name == case_adjusted_name;
    Response response =
        dom_editor_->SetAttribute(element, attribute_name, attribute.Value());
    if (!response.IsSuccess())
      return response;
  }

  if (!found_original_attribute && name.isJust() &&
      !name.fromJust().StripWhiteSpace().IsEmpty()) {
    return dom_editor_->RemoveAttribute(element, case_adjusted_name);
  }
  return Response::Success();
}

Response InspectorDOMAgent::removeAttribute(int element_id,
                                            const String& name) {
  Element* element = nullptr;
  Response response = AssertEditableElement(element_id, element);
  if (!response.IsSuccess())
    return response;

  return dom_editor_->RemoveAttribute(element, name);
}

Response InspectorDOMAgent::removeNode(int node_id) {
  Node* node = nullptr;
  Response response = AssertEditableNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  ContainerNode* parent_node = node->parentNode();
  if (!parent_node)
    return Response::ServerError("Cannot remove detached node");

  return dom_editor_->RemoveChild(parent_node, node);
}

Response InspectorDOMAgent::setNodeName(int node_id,
                                        const String& tag_name,
                                        int* new_id) {
  *new_id = 0;

  Element* old_element = nullptr;
  Response response = AssertElement(node_id, old_element);
  if (!response.IsSuccess())
    return response;

  DummyExceptionStateForTesting exception_state;
  Element* new_elem = old_element->GetDocument().CreateElementForBinding(
      AtomicString(tag_name), exception_state);
  if (exception_state.HadException())
    return ToResponse(exception_state);

  // Copy over the original node's attributes.
  new_elem->CloneAttributesFrom(*old_element);

  // Copy over the original node's children.
  for (Node* child = old_element->firstChild(); child;
       child = old_element->firstChild()) {
    response = dom_editor_->InsertBefore(new_elem, child, nullptr);
    if (!response.IsSuccess())
      return response;
  }

  // Replace the old node with the new node
  ContainerNode* parent = old_element->parentNode();
  response =
      dom_editor_->InsertBefore(parent, new_elem, old_element->nextSibling());
  if (!response.IsSuccess())
    return response;
  response = dom_editor_->RemoveChild(parent, old_element);
  if (!response.IsSuccess())
    return response;

  *new_id = PushNodePathToFrontend(new_elem);
  if (children_requested_.Contains(node_id))
    PushChildNodesToFrontend(*new_id);
  return Response::Success();
}

Response InspectorDOMAgent::getOuterHTML(Maybe<int> node_id,
                                         Maybe<int> backend_node_id,
                                         Maybe<String> object_id,
                                         WTF::String* outer_html) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, backend_node_id, object_id, node);
  if (!response.IsSuccess())
    return response;

  *outer_html = CreateMarkup(node);
  return Response::Success();
}

Response InspectorDOMAgent::setOuterHTML(int node_id,
                                         const String& outer_html) {
  if (!node_id) {
    DCHECK(document_);
    DOMPatchSupport dom_patch_support(dom_editor_.Get(), *document_.Get());
    dom_patch_support.PatchDocument(outer_html);
    return Response::Success();
  }

  Node* node = nullptr;
  Response response = AssertEditableNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  Document* document =
      IsA<Document>(node) ? To<Document>(node) : node->ownerDocument();
  if (!document ||
      (!IsA<HTMLDocument>(document) && !IsA<XMLDocument>(document)))
    return Response::ServerError("Not an HTML/XML document");

  Node* new_node = nullptr;
  response = dom_editor_->SetOuterHTML(node, outer_html, &new_node);
  if (!response.IsSuccess())
    return response;

  if (!new_node) {
    // The only child node has been deleted.
    return Response::Success();
  }

  int new_id = PushNodePathToFrontend(new_node);

  bool children_requested = children_requested_.Contains(node_id);
  if (children_requested)
    PushChildNodesToFrontend(new_id);
  return Response::Success();
}

Response InspectorDOMAgent::setNodeValue(int node_id, const String& value) {
  Node* node = nullptr;
  Response response = AssertEditableNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  if (node->getNodeType() != Node::kTextNode)
    return Response::ServerError("Can only set value of text nodes");

  return dom_editor_->ReplaceWholeText(To<Text>(node), value);
}

static Node* NextNodeWithShadowDOMInMind(const Node& current,
                                         const Node* stay_within,
                                         bool include_user_agent_shadow_dom) {
  // At first traverse the subtree.

  if (ShadowRoot* shadow_root = current.GetShadowRoot()) {
    if (!shadow_root->IsUserAgent() || include_user_agent_shadow_dom)
      return shadow_root;
  }
  if (current.hasChildren())
    return current.firstChild();

  // Then traverse siblings of the node itself and its ancestors.
  const Node* node = &current;
  do {
    if (node == stay_within)
      return nullptr;
    auto* shadow_root = DynamicTo<ShadowRoot>(node);
    if (shadow_root) {
      Element& host = shadow_root->host();
      if (host.HasChildren())
        return host.firstChild();
    }
    if (node->nextSibling())
      return node->nextSibling();
    node = shadow_root ? &shadow_root->host() : node->parentNode();
  } while (node);

  return nullptr;
}

Response InspectorDOMAgent::performSearch(
    const String& whitespace_trimmed_query,
    Maybe<bool> optional_include_user_agent_shadow_dom,
    String* search_id,
    int* result_count) {
  if (!enabled_.Get())
    return Response::ServerError("DOM agent is not enabled");

  // FIXME: Few things are missing here:
  // 1) Search works with node granularity - number of matches within node is
  //    not calculated.
  // 2) There is no need to push all search results to the front-end at a time,
  //    pushing next / previous result is sufficient.

  bool include_user_agent_shadow_dom =
      optional_include_user_agent_shadow_dom.fromMaybe(false);

  unsigned query_length = whitespace_trimmed_query.length();
  bool start_tag_found = !whitespace_trimmed_query.find('<');
  bool start_closing_tag_found = !whitespace_trimmed_query.Find("</");
  bool end_tag_found =
      whitespace_trimmed_query.ReverseFind('>') + 1 == query_length;
  bool start_quote_found = !whitespace_trimmed_query.find('"');
  bool end_quote_found =
      whitespace_trimmed_query.ReverseFind('"') + 1 == query_length;
  bool exact_attribute_match = start_quote_found && end_quote_found;

  String tag_name_query = whitespace_trimmed_query;
  String attribute_query = whitespace_trimmed_query;
  if (start_closing_tag_found)
    tag_name_query = tag_name_query.Right(tag_name_query.length() - 2);
  else if (start_tag_found)
    tag_name_query = tag_name_query.Right(tag_name_query.length() - 1);
  if (end_tag_found)
    tag_name_query = tag_name_query.Left(tag_name_query.length() - 1);
  if (start_quote_found)
    attribute_query = attribute_query.Right(attribute_query.length() - 1);
  if (end_quote_found)
    attribute_query = attribute_query.Left(attribute_query.length() - 1);

  HeapVector<Member<Document>> docs = Documents();
  HeapLinkedHashSet<Member<Node>> result_collector;

  for (Document* document : docs) {
    Node* document_element = document->documentElement();
    Node* node = document_element;
    if (!node)
      continue;

    // Manual plain text search.
    for (; node; node = NextNodeWithShadowDOMInMind(
                     *node, document_element, include_user_agent_shadow_dom)) {
      switch (node->getNodeType()) {
        case Node::kTextNode:
        case Node::kCommentNode:
        case Node::kCdataSectionNode: {
          String text = node->nodeValue();
          if (text.FindIgnoringCase(whitespace_trimmed_query) != kNotFound)
            result_collector.insert(node);
          break;
        }
        case Node::kElementNode: {
          if ((!start_tag_found && !end_tag_found &&
               (node->nodeName().FindIgnoringCase(tag_name_query) !=
                kNotFound)) ||
              (start_tag_found && end_tag_found &&
               DeprecatedEqualIgnoringCase(node->nodeName(), tag_name_query)) ||
              (start_tag_found && !end_tag_found &&
               node->nodeName().StartsWithIgnoringCase(tag_name_query)) ||
              (!start_tag_found && end_tag_found &&
               node->nodeName().EndsWithIgnoringCase(tag_name_query))) {
            result_collector.insert(node);
            break;
          }
          // Go through all attributes and serialize them.
          const auto* element = To<Element>(node);
          AttributeCollection attributes = element->Attributes();
          for (auto& attribute : attributes) {
            // Add attribute pair
            if (attribute.LocalName().FindIgnoringCase(whitespace_trimmed_query,
                                                       0) != kNotFound) {
              result_collector.insert(node);
              break;
            }
            size_t found_position =
                attribute.Value().FindIgnoringCase(attribute_query, 0);
            if (found_position != kNotFound) {
              if (!exact_attribute_match ||
                  (!found_position &&
                   attribute.Value().length() == attribute_query.length())) {
                result_collector.insert(node);
                break;
              }
            }
          }
          break;
        }
        default:
          break;
      }
    }

    // XPath evaluation
    for (Document* document : docs) {
      DCHECK(document);
      DummyExceptionStateForTesting exception_state;
      XPathResult* result = DocumentXPathEvaluator::evaluate(
          *document, whitespace_trimmed_query, document, nullptr,
          XPathResult::kOrderedNodeSnapshotType, ScriptValue(),
          exception_state);
      if (exception_state.HadException() || !result)
        continue;

      wtf_size_t size = result->snapshotLength(exception_state);
      for (wtf_size_t i = 0; !exception_state.HadException() && i < size; ++i) {
        Node* node = result->snapshotItem(i, exception_state);
        if (exception_state.HadException())
          break;

        if (node->getNodeType() == Node::kAttributeNode)
          node = To<Attr>(node)->ownerElement();
        result_collector.insert(node);
      }
    }

    // Selector evaluation
    for (Document* document : docs) {
      DummyExceptionStateForTesting exception_state;
      StaticElementList* element_list = document->QuerySelectorAll(
          AtomicString(whitespace_trimmed_query), exception_state);
      if (exception_state.HadException() || !element_list)
        continue;

      unsigned size = element_list->length();
      for (unsigned i = 0; i < size; ++i)
        result_collector.insert(element_list->item(i));
    }
  }

  *search_id = IdentifiersFactory::CreateIdentifier();
  HeapVector<Member<Node>>* results_it =
      &search_results_.insert(*search_id, HeapVector<Member<Node>>())
           .stored_value->value;

  for (auto& result : result_collector)
    results_it->push_back(result);

  *result_count = results_it->size();
  return Response::Success();
}

Response InspectorDOMAgent::getSearchResults(
    const String& search_id,
    int from_index,
    int to_index,
    std::unique_ptr<protocol::Array<int>>* node_ids) {
  SearchResults::iterator it = search_results_.find(search_id);
  if (it == search_results_.end())
    return Response::ServerError("No search session with given id found");

  int size = it->value.size();
  if (from_index < 0 || to_index > size || from_index >= to_index)
    return Response::ServerError("Invalid search result range");

  *node_ids = std::make_unique<protocol::Array<int>>();
  for (int i = from_index; i < to_index; ++i)
    (*node_ids)->emplace_back(PushNodePathToFrontend((it->value)[i].Get()));
  return Response::Success();
}

Response InspectorDOMAgent::discardSearchResults(const String& search_id) {
  search_results_.erase(search_id);
  return Response::Success();
}

Response InspectorDOMAgent::NodeForRemoteObjectId(const String& object_id,
                                                  Node*& node) {
  v8::HandleScope handles(isolate_);
  v8::Local<v8::Value> value;
  v8::Local<v8::Context> context;
  std::unique_ptr<v8_inspector::StringBuffer> error;
  if (!v8_session_->unwrapObject(&error, ToV8InspectorStringView(object_id),
                                 &value, &context, nullptr))
    return Response::ServerError(ToCoreString(std::move(error)).Utf8());
  if (!V8Node::HasInstance(value, isolate_))
    return Response::ServerError("Object id doesn't reference a Node");
  node = V8Node::ToImpl(v8::Local<v8::Object>::Cast(value));
  if (!node) {
    return Response::ServerError(
        "Couldn't convert object with given objectId to Node");
  }
  return Response::Success();
}

Response InspectorDOMAgent::copyTo(int node_id,
                                   int target_element_id,
                                   Maybe<int> anchor_node_id,
                                   int* new_node_id) {
  Node* node = nullptr;
  Response response = AssertEditableNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  Element* target_element = nullptr;
  response = AssertEditableElement(target_element_id, target_element);
  if (!response.IsSuccess())
    return response;

  Node* anchor_node = nullptr;
  if (anchor_node_id.isJust() && anchor_node_id.fromJust()) {
    response = AssertEditableChildNode(target_element,
                                       anchor_node_id.fromJust(), anchor_node);
    if (!response.IsSuccess())
      return response;
  }

  // The clone is deep by default.
  Node* cloned_node = node->cloneNode(true);
  if (!cloned_node)
    return Response::ServerError("Failed to clone node");
  response =
      dom_editor_->InsertBefore(target_element, cloned_node, anchor_node);
  if (!response.IsSuccess())
    return response;

  *new_node_id = PushNodePathToFrontend(cloned_node);
  return Response::Success();
}

Response InspectorDOMAgent::moveTo(int node_id,
                                   int target_element_id,
                                   Maybe<int> anchor_node_id,
                                   int* new_node_id) {
  Node* node = nullptr;
  Response response = AssertEditableNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  Element* target_element = nullptr;
  response = AssertEditableElement(target_element_id, target_element);
  if (!response.IsSuccess())
    return response;

  Node* current = target_element;
  while (current) {
    if (current == node) {
      return Response::ServerError(
          "Unable to move node into self or descendant");
    }
    current = current->parentNode();
  }

  Node* anchor_node = nullptr;
  if (anchor_node_id.isJust() && anchor_node_id.fromJust()) {
    response = AssertEditableChildNode(target_element,
                                       anchor_node_id.fromJust(), anchor_node);
    if (!response.IsSuccess())
      return response;
  }

  response = dom_editor_->InsertBefore(target_element, node, anchor_node);
  if (!response.IsSuccess())
    return response;

  *new_node_id = PushNodePathToFrontend(node);
  return Response::Success();
}

Response InspectorDOMAgent::undo() {
  if (!enabled_.Get())
    return Response::ServerError("DOM agent is not enabled");
  DummyExceptionStateForTesting exception_state;
  history_->Undo(exception_state);
  return InspectorDOMAgent::ToResponse(exception_state);
}

Response InspectorDOMAgent::redo() {
  if (!enabled_.Get())
    return Response::ServerError("DOM agent is not enabled");
  DummyExceptionStateForTesting exception_state;
  history_->Redo(exception_state);
  return InspectorDOMAgent::ToResponse(exception_state);
}

Response InspectorDOMAgent::markUndoableState() {
  history_->MarkUndoableState();
  return Response::Success();
}

Response InspectorDOMAgent::focus(Maybe<int> node_id,
                                  Maybe<int> backend_node_id,
                                  Maybe<String> object_id) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, backend_node_id, object_id, node);
  if (!response.IsSuccess())
    return response;
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return Response::ServerError("Node is not an Element");
  element->GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kInspector);
  if (!element->IsFocusable())
    return Response::ServerError("Element is not focusable");
  element->focus();
  return Response::Success();
}

Response InspectorDOMAgent::setFileInputFiles(
    std::unique_ptr<protocol::Array<String>> files,
    Maybe<int> node_id,
    Maybe<int> backend_node_id,
    Maybe<String> object_id) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, backend_node_id, object_id, node);
  if (!response.IsSuccess())
    return response;

  auto* html_input_element = DynamicTo<HTMLInputElement>(node);
  if (!html_input_element ||
      html_input_element->type() != input_type_names::kFile)
    return Response::ServerError("Node is not a file input element");

  Vector<String> paths;
  for (const String& file : *files)
    paths.push_back(file);
  To<HTMLInputElement>(node)->SetFilesFromPaths(paths);
  return Response::Success();
}

Response InspectorDOMAgent::setNodeStackTracesEnabled(bool enable) {
  capture_node_stack_traces_.Set(enable);
  return Response::Success();
}

Response InspectorDOMAgent::getNodeStackTraces(
    int node_id,
    protocol::Maybe<v8_inspector::protocol::Runtime::API::StackTrace>*
        creation) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, node);
  if (!response.IsSuccess())
    return response;

  InspectorSourceLocation* creation_inspector_source_location =
      node_to_creation_source_location_map_.at(node);
  if (creation_inspector_source_location) {
    SourceLocation& source_location =
        creation_inspector_source_location->GetSourceLocation();
    *creation = source_location.BuildInspectorObject();
  }
  return Response::Success();
}

Response InspectorDOMAgent::getBoxModel(
    Maybe<int> node_id,
    Maybe<int> backend_node_id,
    Maybe<String> object_id,
    std::unique_ptr<protocol::DOM::BoxModel>* model) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, backend_node_id, object_id, node);
  if (!response.IsSuccess())
    return response;

  bool result = InspectorHighlight::GetBoxModel(node, model, true);
  if (!result)
    return Response::ServerError("Could not compute box model.");
  return Response::Success();
}

Response InspectorDOMAgent::getContentQuads(
    Maybe<int> node_id,
    Maybe<int> backend_node_id,
    Maybe<String> object_id,
    std::unique_ptr<protocol::Array<protocol::Array<double>>>* quads) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, backend_node_id, object_id, node);
  if (!response.IsSuccess())
    return response;
  bool result = InspectorHighlight::GetContentQuads(node, quads);
  if (!result)
    return Response::ServerError("Could not compute content quads.");
  return Response::Success();
}

Response InspectorDOMAgent::getNodeForLocation(
    int x,
    int y,
    Maybe<bool> optional_include_user_agent_shadow_dom,
    Maybe<bool> optional_ignore_pointer_events_none,
    int* backend_node_id,
    String* frame_id,
    Maybe<int>* node_id) {
  bool include_user_agent_shadow_dom =
      optional_include_user_agent_shadow_dom.fromMaybe(false);
  Document* document = inspected_frames_->Root()->GetDocument();
  PhysicalOffset document_point(
      LayoutUnit(x * inspected_frames_->Root()->PageZoomFactor()),
      LayoutUnit(y * inspected_frames_->Root()->PageZoomFactor()));
  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kMove | HitTestRequest::kReadOnly |
      HitTestRequest::kAllowChildFrameContent;
  if (optional_ignore_pointer_events_none.fromMaybe(false))
    hit_type |= HitTestRequest::kIgnorePointerEventsNone;
  HitTestRequest request(hit_type);
  HitTestLocation location(document->View()->DocumentToFrame(document_point));
  HitTestResult result(request, location);
  document->GetFrame()->ContentLayoutObject()->HitTest(location, result);
  if (!include_user_agent_shadow_dom)
    result.SetToShadowHostIfInRestrictedShadowRoot();
  Node* node = result.InnerPossiblyPseudoNode();
  while (node && node->getNodeType() == Node::kTextNode)
    node = node->parentNode();
  if (!node)
    return Response::ServerError("No node found at given location");
  *backend_node_id = IdentifiersFactory::IntIdForNode(node);
  LocalFrame* frame = node->GetDocument().GetFrame();
  *frame_id = IdentifiersFactory::FrameId(frame);
  if (enabled_.Get() && document_ &&
      document_node_to_id_map_->Contains(document_)) {
    *node_id = PushNodePathToFrontend(node);
  }
  return Response::Success();
}

Response InspectorDOMAgent::resolveNode(
    protocol::Maybe<int> node_id,
    protocol::Maybe<int> backend_node_id,
    protocol::Maybe<String> object_group,
    protocol::Maybe<int> execution_context_id,
    std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>*
        result) {
  String object_group_name = object_group.fromMaybe("");
  Node* node = nullptr;

  if (node_id.isJust() == backend_node_id.isJust()) {
    return Response::ServerError(
        "Either nodeId or backendNodeId must be specified.");
  }

  if (node_id.isJust())
    node = NodeForId(node_id.fromJust());
  else
    node = DOMNodeIds::NodeForId(backend_node_id.fromJust());

  if (!node)
    return Response::ServerError("No node with given id found");
  *result = ResolveNode(v8_session_, node, object_group_name,
                        std::move(execution_context_id));
  if (!*result) {
    return Response::ServerError(
        "Node with given id does not belong to the document");
  }
  return Response::Success();
}

Response InspectorDOMAgent::getAttributes(
    int node_id,
    std::unique_ptr<protocol::Array<String>>* result) {
  Element* element = nullptr;
  Response response = AssertElement(node_id, element);
  if (!response.IsSuccess())
    return response;

  *result = BuildArrayForElementAttributes(element);
  return Response::Success();
}

Response InspectorDOMAgent::requestNode(const String& object_id, int* node_id) {
  Node* node = nullptr;
  Response response = NodeForRemoteObjectId(object_id, node);
  if (!response.IsSuccess())
    return response;
  *node_id = PushNodePathToFrontend(node);
  return Response::Success();
}

// static
String InspectorDOMAgent::DocumentURLString(Document* document) {
  if (!document || document->Url().IsNull())
    return "";
  return document->Url().GetString();
}

// static
String InspectorDOMAgent::DocumentBaseURLString(Document* document) {
  return document->BaseURL().GetString();
}

// static
protocol::DOM::ShadowRootType InspectorDOMAgent::GetShadowRootType(
    ShadowRoot* shadow_root) {
  switch (shadow_root->GetType()) {
    case ShadowRootType::kUserAgent:
      return protocol::DOM::ShadowRootTypeEnum::UserAgent;
    case ShadowRootType::V0:
    case ShadowRootType::kOpen:
      return protocol::DOM::ShadowRootTypeEnum::Open;
    case ShadowRootType::kClosed:
      return protocol::DOM::ShadowRootTypeEnum::Closed;
  }
  NOTREACHED();
  return protocol::DOM::ShadowRootTypeEnum::UserAgent;
}

std::unique_ptr<protocol::DOM::Node> InspectorDOMAgent::BuildObjectForNode(
    Node* node,
    int depth,
    bool pierce,
    NodeToIdMap* nodes_map,
    protocol::Array<protocol::DOM::Node>* flatten_result) {
  int id = Bind(node, nodes_map);
  String local_name;
  String node_value;

  switch (node->getNodeType()) {
    case Node::kTextNode:
    case Node::kCommentNode:
    case Node::kCdataSectionNode:
      node_value = node->nodeValue();
      if (node_value.length() > kMaxTextSize)
        node_value = node_value.Left(kMaxTextSize) + kEllipsisUChar;
      break;
    case Node::kAttributeNode:
      local_name = To<Attr>(node)->localName();
      break;
    case Node::kElementNode:
      local_name = To<Element>(node)->localName();
      break;
    default:
      break;
  }

  std::unique_ptr<protocol::DOM::Node> value =
      protocol::DOM::Node::create()
          .setNodeId(id)
          .setBackendNodeId(IdentifiersFactory::IntIdForNode(node))
          .setNodeType(static_cast<int>(node->getNodeType()))
          .setNodeName(node->nodeName())
          .setLocalName(local_name)
          .setNodeValue(node_value)
          .build();

  if (node->IsSVGElement())
    value->setIsSVG(true);

  bool force_push_children = false;
  if (auto* element = DynamicTo<Element>(node)) {
    value->setAttributes(BuildArrayForElementAttributes(element));

    if (auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(node)) {
      if (frame_owner->ContentFrame()) {
        value->setFrameId(
            IdentifiersFactory::FrameId(frame_owner->ContentFrame()));
      }
      if (Document* doc = frame_owner->contentDocument()) {
        value->setContentDocument(BuildObjectForNode(
            doc, pierce ? depth : 0, pierce, nodes_map, flatten_result));
      }
    }

    if (node->parentNode() && node->parentNode()->IsDocumentNode()) {
      LocalFrame* frame = node->GetDocument().GetFrame();
      if (frame)
        value->setFrameId(IdentifiersFactory::FrameId(frame));
    }

    if (ShadowRoot* root = element->GetShadowRoot()) {
      auto shadow_roots =
          std::make_unique<protocol::Array<protocol::DOM::Node>>();
      shadow_roots->emplace_back(BuildObjectForNode(
          root, pierce ? depth : 0, pierce, nodes_map, flatten_result));
      value->setShadowRoots(std::move(shadow_roots));
      force_push_children = true;
    }

    if (auto* link_element = DynamicTo<HTMLLinkElement>(*element)) {
      if (link_element->IsImport() && link_element->import() &&
          InnerParentNode(link_element->import()) == link_element) {
        value->setImportedDocument(BuildObjectForNode(
            link_element->import(), 0, pierce, nodes_map, flatten_result));
      }
      force_push_children = true;
    }

    if (auto* template_element = DynamicTo<HTMLTemplateElement>(*element)) {
      // The inspector should not try to access the .content() property of
      // declarative Shadow DOM <template> elements, because it will be null.
      if (!template_element->IsDeclarativeShadowRoot()) {
        value->setTemplateContent(BuildObjectForNode(
            template_element->content(), 0, pierce, nodes_map, flatten_result));
        force_push_children = true;
      }
    }

    if (element->GetPseudoId()) {
      value->setPseudoType(ProtocolPseudoElementType(element->GetPseudoId()));
    } else {
      if (!element->ownerDocument()->xmlVersion().IsEmpty())
        value->setXmlVersion(element->ownerDocument()->xmlVersion());
    }
    std::unique_ptr<protocol::Array<protocol::DOM::Node>> pseudo_elements =
        BuildArrayForPseudoElements(element, nodes_map);
    if (pseudo_elements) {
      value->setPseudoElements(std::move(pseudo_elements));
      force_push_children = true;
    }

    if (auto* insertion_point = DynamicTo<V0InsertionPoint>(element)) {
      value->setDistributedNodes(
          BuildArrayForDistributedNodes(insertion_point));
      force_push_children = true;
    }
    if (auto* slot = DynamicTo<HTMLSlotElement>(*element)) {
      if (node->IsInShadowTree()) {
        value->setDistributedNodes(BuildDistributedNodesForSlot(slot));
        force_push_children = true;
      }
    }
  } else if (auto* document = DynamicTo<Document>(node)) {
    value->setDocumentURL(DocumentURLString(document));
    value->setBaseURL(DocumentBaseURLString(document));
    value->setXmlVersion(document->xmlVersion());
  } else if (auto* doc_type = DynamicTo<DocumentType>(node)) {
    value->setPublicId(doc_type->publicId());
    value->setSystemId(doc_type->systemId());
  } else if (node->IsAttributeNode()) {
    auto* attribute = To<Attr>(node);
    value->setName(attribute->name());
    value->setValue(attribute->value());
  } else if (auto* shadow_root = DynamicTo<ShadowRoot>(node)) {
    value->setShadowRootType(GetShadowRootType(shadow_root));
  }

  if (node->IsContainerNode()) {
    int node_count = InnerChildNodeCount(node);
    value->setChildNodeCount(node_count);
    if (nodes_map == document_node_to_id_map_)
      cached_child_count_.Set(id, node_count);
    if (nodes_map && force_push_children && !depth)
      depth = 1;
    std::unique_ptr<protocol::Array<protocol::DOM::Node>> children =
        BuildArrayForContainerChildren(node, depth, pierce, nodes_map,
                                       flatten_result);
    if (!children->empty() ||
        depth)  // Push children along with shadow in any case.
      value->setChildren(std::move(children));
  }

  return value;
}

std::unique_ptr<protocol::Array<String>>
InspectorDOMAgent::BuildArrayForElementAttributes(Element* element) {
  auto attributes_value = std::make_unique<protocol::Array<String>>();
  // Go through all attributes and serialize them.
  for (const blink::Attribute& attribute : element->Attributes()) {
    // Add attribute pair
    attributes_value->emplace_back(attribute.GetName().ToString());
    attributes_value->emplace_back(attribute.Value());
  }
  return attributes_value;
}

std::unique_ptr<protocol::Array<protocol::DOM::Node>>
InspectorDOMAgent::BuildArrayForContainerChildren(
    Node* container,
    int depth,
    bool pierce,
    NodeToIdMap* nodes_map,
    protocol::Array<protocol::DOM::Node>* flatten_result) {
  auto children = std::make_unique<protocol::Array<protocol::DOM::Node>>();
  if (depth == 0) {
    if (!nodes_map)
      return children;
    // Special-case the only text child - pretend that container's children have
    // been requested.
    Node* first_child = container->firstChild();
    if (first_child && first_child->getNodeType() == Node::kTextNode &&
        !first_child->nextSibling()) {
      std::unique_ptr<protocol::DOM::Node> child_node =
          BuildObjectForNode(first_child, 0, pierce, nodes_map, flatten_result);
      child_node->setParentId(Bind(container, nodes_map));
      if (flatten_result) {
        flatten_result->emplace_back(std::move(child_node));
      } else {
        children->emplace_back(std::move(child_node));
      }
      children_requested_.insert(Bind(container, nodes_map));
    }
    return children;
  }

  Node* child = InnerFirstChild(container);
  depth--;
  if (nodes_map)
    children_requested_.insert(Bind(container, nodes_map));

  while (child) {
    std::unique_ptr<protocol::DOM::Node> child_node =
        BuildObjectForNode(child, depth, pierce, nodes_map, flatten_result);
    child_node->setParentId(Bind(container, nodes_map));
    if (flatten_result) {
      flatten_result->emplace_back(std::move(child_node));
    } else {
      children->emplace_back(std::move(child_node));
    }
    if (nodes_map)
      children_requested_.insert(Bind(container, nodes_map));
    child = InnerNextSibling(child);
  }
  return children;
}

std::unique_ptr<protocol::Array<protocol::DOM::Node>>
InspectorDOMAgent::BuildArrayForPseudoElements(Element* element,
                                               NodeToIdMap* nodes_map) {
  protocol::Array<protocol::DOM::Node> pseudo_elements;
  if (element->GetPseudoElement(kPseudoIdBefore)) {
    pseudo_elements.emplace_back(BuildObjectForNode(
        element->GetPseudoElement(kPseudoIdBefore), 0, false, nodes_map));
  }
  if (element->GetPseudoElement(kPseudoIdAfter)) {
    pseudo_elements.emplace_back(BuildObjectForNode(
        element->GetPseudoElement(kPseudoIdAfter), 0, false, nodes_map));
  }
  if (element->GetPseudoElement(kPseudoIdMarker) &&
      RuntimeEnabledFeatures::CSSMarkerPseudoElementEnabled()) {
    pseudo_elements.emplace_back(BuildObjectForNode(
        element->GetPseudoElement(kPseudoIdMarker), 0, false, nodes_map));
  }
  if (pseudo_elements.empty())
    return nullptr;
  return std::make_unique<protocol::Array<protocol::DOM::Node>>(
      std::move(pseudo_elements));
}

std::unique_ptr<protocol::Array<protocol::DOM::BackendNode>>
InspectorDOMAgent::BuildArrayForDistributedNodes(
    V0InsertionPoint* insertion_point) {
  auto distributed_nodes =
      std::make_unique<protocol::Array<protocol::DOM::BackendNode>>();
  for (wtf_size_t i = 0; i < insertion_point->DistributedNodesSize(); ++i) {
    Node* distributed_node = insertion_point->DistributedNodeAt(i);
    if (IsWhitespace(distributed_node))
      continue;

    std::unique_ptr<protocol::DOM::BackendNode> backend_node =
        protocol::DOM::BackendNode::create()
            .setNodeType(distributed_node->getNodeType())
            .setNodeName(distributed_node->nodeName())
            .setBackendNodeId(
                IdentifiersFactory::IntIdForNode(distributed_node))
            .build();
    distributed_nodes->emplace_back(std::move(backend_node));
  }
  return distributed_nodes;
}

std::unique_ptr<protocol::Array<protocol::DOM::BackendNode>>
InspectorDOMAgent::BuildDistributedNodesForSlot(HTMLSlotElement* slot_element) {
  // TODO(hayato): In Shadow DOM v1, the concept of distributed nodes should
  // not be used anymore. DistributedNodes should be replaced with
  // AssignedNodes() when IncrementalShadowDOM becomes stable and Shadow DOM v0
  // is removed.
  auto distributed_nodes =
      std::make_unique<protocol::Array<protocol::DOM::BackendNode>>();
  for (auto& node : slot_element->AssignedNodes()) {
    if (IsWhitespace(node))
      continue;

    std::unique_ptr<protocol::DOM::BackendNode> backend_node =
        protocol::DOM::BackendNode::create()
            .setNodeType(node->getNodeType())
            .setNodeName(node->nodeName())
            .setBackendNodeId(IdentifiersFactory::IntIdForNode(node))
            .build();
    distributed_nodes->emplace_back(std::move(backend_node));
  }
  return distributed_nodes;
}

// static
Node* InspectorDOMAgent::InnerFirstChild(Node* node) {
  node = node->firstChild();
  while (IsWhitespace(node))
    node = node->nextSibling();
  return node;
}

// static
Node* InspectorDOMAgent::InnerNextSibling(Node* node) {
  do {
    node = node->nextSibling();
  } while (IsWhitespace(node));
  return node;
}

// static
Node* InspectorDOMAgent::InnerPreviousSibling(Node* node) {
  do {
    node = node->previousSibling();
  } while (IsWhitespace(node));
  return node;
}

// static
unsigned InspectorDOMAgent::InnerChildNodeCount(Node* node) {
  unsigned count = 0;
  Node* child = InnerFirstChild(node);
  while (child) {
    count++;
    child = InnerNextSibling(child);
  }
  return count;
}

// static
Node* InspectorDOMAgent::InnerParentNode(Node* node) {
  if (auto* document = DynamicTo<Document>(node)) {
    if (HTMLImportLoader* loader = document->ImportLoader())
      return loader->FirstImport()->Link();
    return document->LocalOwner();
  }
  return node->ParentOrShadowHostNode();
}

// static
bool InspectorDOMAgent::IsWhitespace(Node* node) {
  // TODO: pull ignoreWhitespace setting from the frontend and use here.
  return node && node->getNodeType() == Node::kTextNode &&
         node->nodeValue().StripWhiteSpace().length() == 0;
}

// static
void InspectorDOMAgent::CollectNodes(
    Node* node,
    int depth,
    bool pierce,
    base::RepeatingCallback<bool(Node*)> filter,
    HeapVector<Member<Node>>* result) {
  if (filter && filter.Run(node))
    result->push_back(node);
  if (--depth <= 0)
    return;

  auto* element = DynamicTo<Element>(node);
  if (pierce && element) {
    if (auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(node)) {
      if (frame_owner->ContentFrame() &&
          frame_owner->ContentFrame()->IsLocalFrame()) {
        if (Document* doc = frame_owner->contentDocument())
          CollectNodes(doc, depth, pierce, filter, result);
      }
    }

    ShadowRoot* root = element->GetShadowRoot();
    if (pierce && root)
      CollectNodes(root, depth, pierce, filter, result);

    if (auto* link_element = DynamicTo<HTMLLinkElement>(*element)) {
      if (link_element->IsImport() && link_element->import() &&
          InnerParentNode(link_element->import()) == link_element) {
        CollectNodes(link_element->import(), depth, pierce, filter, result);
      }
    }
  }

  for (Node* child = InnerFirstChild(node); child;
       child = InnerNextSibling(child)) {
    CollectNodes(child, depth, pierce, filter, result);
  }
}

void InspectorDOMAgent::DomContentLoadedEventFired(LocalFrame* frame) {
  if (frame != inspected_frames_->Root())
    return;

  // Re-push document once it is loaded.
  DiscardFrontendBindings();
  if (enabled_.Get())
    GetFrontend()->documentUpdated();
}

void InspectorDOMAgent::InvalidateFrameOwnerElement(
    HTMLFrameOwnerElement* frame_owner) {
  if (!frame_owner)
    return;

  int frame_owner_id = document_node_to_id_map_->at(frame_owner);
  if (!frame_owner_id)
    return;

  // Re-add frame owner element together with its new children.
  int parent_id = document_node_to_id_map_->at(InnerParentNode(frame_owner));
  GetFrontend()->childNodeRemoved(parent_id, frame_owner_id);
  Unbind(frame_owner, document_node_to_id_map_.Get());

  std::unique_ptr<protocol::DOM::Node> value =
      BuildObjectForNode(frame_owner, 0, false, document_node_to_id_map_.Get());
  Node* previous_sibling = InnerPreviousSibling(frame_owner);
  int prev_id =
      previous_sibling ? document_node_to_id_map_->at(previous_sibling) : 0;
  GetFrontend()->childNodeInserted(parent_id, prev_id, std::move(value));
}

void InspectorDOMAgent::DidCommitLoad(LocalFrame*, DocumentLoader* loader) {
  Document* document = loader->GetFrame()->GetDocument();
  if (dom_listener_)
    dom_listener_->DidAddDocument(document);

  LocalFrame* inspected_frame = inspected_frames_->Root();
  if (loader->GetFrame() != inspected_frame) {
    InvalidateFrameOwnerElement(
        loader->GetFrame()->GetDocument()->LocalOwner());
    return;
  }

  SetDocument(inspected_frame->GetDocument());
}

void InspectorDOMAgent::DidInsertDOMNode(Node* node) {
  if (IsWhitespace(node))
    return;

  // We could be attaching existing subtree. Forget the bindings.
  Unbind(node, document_node_to_id_map_.Get());

  ContainerNode* parent = node->parentNode();
  if (!parent)
    return;
  int parent_id = document_node_to_id_map_->at(parent);
  // Return if parent is not mapped yet.
  if (!parent_id)
    return;

  if (!children_requested_.Contains(parent_id)) {
    // No children are mapped yet -> only notify on changes of child count.
    int count = cached_child_count_.at(parent_id) + 1;
    cached_child_count_.Set(parent_id, count);
    GetFrontend()->childNodeCountUpdated(parent_id, count);
  } else {
    // Children have been requested -> return value of a new child.
    Node* prev_sibling = InnerPreviousSibling(node);
    int prev_id = prev_sibling ? document_node_to_id_map_->at(prev_sibling) : 0;
    std::unique_ptr<protocol::DOM::Node> value =
        BuildObjectForNode(node, 0, false, document_node_to_id_map_.Get());
    GetFrontend()->childNodeInserted(parent_id, prev_id, std::move(value));
  }
}

void InspectorDOMAgent::WillRemoveDOMNode(Node* node) {
  if (IsWhitespace(node))
    return;
  DOMNodeRemoved(node);
}

void InspectorDOMAgent::DOMNodeRemoved(Node* node) {
  ContainerNode* parent = node->parentNode();

  // If parent is not mapped yet -> ignore the event.
  if (!document_node_to_id_map_->Contains(parent))
    return;

  int parent_id = document_node_to_id_map_->at(parent);

  if (!children_requested_.Contains(parent_id)) {
    // No children are mapped yet -> only notify on changes of child count.
    int count = cached_child_count_.at(parent_id) - 1;
    cached_child_count_.Set(parent_id, count);
    GetFrontend()->childNodeCountUpdated(parent_id, count);
  } else {
    GetFrontend()->childNodeRemoved(parent_id,
                                    document_node_to_id_map_->at(node));
  }
  Unbind(node, document_node_to_id_map_.Get());
}

void InspectorDOMAgent::WillModifyDOMAttr(Element*,
                                          const AtomicString& old_value,
                                          const AtomicString& new_value) {
  suppress_attribute_modified_event_ = (old_value == new_value);
}

void InspectorDOMAgent::DidModifyDOMAttr(Element* element,
                                         const QualifiedName& name,
                                         const AtomicString& value) {
  bool should_suppress_event = suppress_attribute_modified_event_;
  suppress_attribute_modified_event_ = false;
  if (should_suppress_event)
    return;

  int id = BoundNodeId(element);
  // If node is not mapped yet -> ignore the event.
  if (!id)
    return;

  if (dom_listener_)
    dom_listener_->DidModifyDOMAttr(element);

  GetFrontend()->attributeModified(id, name.ToString(), value);
}

void InspectorDOMAgent::DidRemoveDOMAttr(Element* element,
                                         const QualifiedName& name) {
  int id = BoundNodeId(element);
  // If node is not mapped yet -> ignore the event.
  if (!id)
    return;

  if (dom_listener_)
    dom_listener_->DidModifyDOMAttr(element);

  GetFrontend()->attributeRemoved(id, name.ToString());
}

void InspectorDOMAgent::StyleAttributeInvalidated(
    const HeapVector<Member<Element>>& elements) {
  auto node_ids = std::make_unique<protocol::Array<int>>();
  for (unsigned i = 0, size = elements.size(); i < size; ++i) {
    Element* element = elements.at(i);
    int id = BoundNodeId(element);
    // If node is not mapped yet -> ignore the event.
    if (!id)
      continue;

    if (dom_listener_)
      dom_listener_->DidModifyDOMAttr(element);
    node_ids->emplace_back(id);
  }
  GetFrontend()->inlineStyleInvalidated(std::move(node_ids));
}

void InspectorDOMAgent::CharacterDataModified(CharacterData* character_data) {
  int id = document_node_to_id_map_->at(character_data);
  if (IsWhitespace(character_data) && id) {
    DOMNodeRemoved(character_data);
    return;
  }
  if (!id) {
    // Push text node if it is being created.
    DidInsertDOMNode(character_data);
    return;
  }
  GetFrontend()->characterDataModified(id, character_data->data());
}

InspectorRevalidateDOMTask* InspectorDOMAgent::RevalidateTask() {
  if (!revalidate_task_)
    revalidate_task_ = MakeGarbageCollected<InspectorRevalidateDOMTask>(this);
  return revalidate_task_.Get();
}

void InspectorDOMAgent::DidInvalidateStyleAttr(Node* node) {
  int id = document_node_to_id_map_->at(node);
  // If node is not mapped yet -> ignore the event.
  if (!id)
    return;

  RevalidateTask()->ScheduleStyleAttrRevalidationFor(To<Element>(node));
}

void InspectorDOMAgent::DidPushShadowRoot(Element* host, ShadowRoot* root) {
  if (!host->ownerDocument())
    return;

  int host_id = document_node_to_id_map_->at(host);
  if (!host_id)
    return;

  PushChildNodesToFrontend(host_id, 1);
  GetFrontend()->shadowRootPushed(
      host_id,
      BuildObjectForNode(root, 0, false, document_node_to_id_map_.Get()));
}

void InspectorDOMAgent::WillPopShadowRoot(Element* host, ShadowRoot* root) {
  if (!host->ownerDocument())
    return;

  int host_id = document_node_to_id_map_->at(host);
  int root_id = document_node_to_id_map_->at(root);
  if (host_id && root_id)
    GetFrontend()->shadowRootPopped(host_id, root_id);
}

void InspectorDOMAgent::DidPerformElementShadowDistribution(
    Element* shadow_host) {
  int shadow_host_id = document_node_to_id_map_->at(shadow_host);
  if (!shadow_host_id)
    return;

  if (ShadowRoot* root = shadow_host->GetShadowRoot()) {
    const HeapVector<Member<V0InsertionPoint>>& insertion_points =
        root->V0().DescendantInsertionPoints();
    for (const auto& it : insertion_points) {
      V0InsertionPoint* insertion_point = it.Get();
      int insertion_point_id = document_node_to_id_map_->at(insertion_point);
      if (insertion_point_id)
        GetFrontend()->distributedNodesUpdated(
            insertion_point_id, BuildArrayForDistributedNodes(insertion_point));
    }
  }
}

void InspectorDOMAgent::DidPerformSlotDistribution(
    HTMLSlotElement* slot_element) {
  int insertion_point_id = document_node_to_id_map_->at(slot_element);
  if (insertion_point_id)
    GetFrontend()->distributedNodesUpdated(
        insertion_point_id, BuildDistributedNodesForSlot(slot_element));
}

void InspectorDOMAgent::FrameDocumentUpdated(LocalFrame* frame) {
  Document* document = frame->GetDocument();
  if (!document)
    return;

  if (frame != inspected_frames_->Root())
    return;

  // Only update the main frame document, nested frame document updates are not
  // required (will be handled by invalidateFrameOwnerElement()).
  SetDocument(document);
}

void InspectorDOMAgent::FrameOwnerContentUpdated(
    LocalFrame* frame,
    HTMLFrameOwnerElement* frame_owner) {
  if (!frame_owner->contentDocument()) {
    // frame_owner does not point to frame at this point, so Unbind it
    // explicitly.
    Unbind(frame->GetDocument(), document_node_to_id_map_.Get());
  }

  // Revalidating owner can serialize empty frame owner - that's what we are
  // looking for when disconnecting.
  InvalidateFrameOwnerElement(frame_owner);
}

void InspectorDOMAgent::PseudoElementCreated(PseudoElement* pseudo_element) {
  if (pseudo_element->IsMarkerPseudoElement() &&
      !RuntimeEnabledFeatures::CSSMarkerPseudoElementEnabled()) {
    return;
  }
  Element* parent = pseudo_element->ParentOrShadowHostElement();
  if (!parent)
    return;
  int parent_id = document_node_to_id_map_->at(parent);
  if (!parent_id)
    return;

  PushChildNodesToFrontend(parent_id, 1);
  GetFrontend()->pseudoElementAdded(
      parent_id, BuildObjectForNode(pseudo_element, 0, false,
                                    document_node_to_id_map_.Get()));
}

void InspectorDOMAgent::PseudoElementDestroyed(PseudoElement* pseudo_element) {
  int pseudo_element_id = document_node_to_id_map_->at(pseudo_element);
  if (!pseudo_element_id)
    return;

  // If a PseudoElement is bound, its parent element must be bound, too.
  Element* parent = pseudo_element->ParentOrShadowHostElement();
  DCHECK(parent);
  int parent_id = document_node_to_id_map_->at(parent);
  DCHECK(parent_id);

  Unbind(pseudo_element, document_node_to_id_map_.Get());
  GetFrontend()->pseudoElementRemoved(parent_id, pseudo_element_id);
}

void InspectorDOMAgent::NodeCreated(Node* node) {
  if (!capture_node_stack_traces_.Get())
    return;

  std::unique_ptr<SourceLocation> creation_source_location =
      SourceLocation::CaptureWithFullStackTrace();
  if (creation_source_location) {
    node_to_creation_source_location_map_.Set(
        node, MakeGarbageCollected<InspectorSourceLocation>(
                  std::move(creation_source_location)));
  }
}

void InspectorDOMAgent::PortalRemoteFrameCreated(
    HTMLPortalElement* portal_element) {
  InvalidateFrameOwnerElement(portal_element);
}

static ShadowRoot* ShadowRootForNode(Node* node, const String& type) {
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return nullptr;
  if (type == "a")
    return element->AuthorShadowRoot();
  if (type == "u")
    return element->UserAgentShadowRoot();
  return nullptr;
}

Node* InspectorDOMAgent::NodeForPath(const String& path) {
  // The path is of form "1,HTML,2,BODY,1,DIV" (<index> and <nodeName>
  // interleaved).  <index> may also be "a" (author shadow root) or "u"
  // (user-agent shadow root), in which case <nodeName> MUST be
  // "#document-fragment".
  if (!document_)
    return nullptr;

  Node* node = document_.Get();
  Vector<String> path_tokens;
  path.Split(',', path_tokens);
  if (!path_tokens.size())
    return nullptr;

  for (wtf_size_t i = 0; i < path_tokens.size() - 1; i += 2) {
    bool success = true;
    String& index_value = path_tokens[i];
    wtf_size_t child_number = index_value.ToUInt(&success);
    Node* child;
    if (!success) {
      child = ShadowRootForNode(node, index_value);
    } else {
      if (child_number >= InnerChildNodeCount(node))
        return nullptr;

      child = InnerFirstChild(node);
    }
    String child_name = path_tokens[i + 1];
    for (wtf_size_t j = 0; child && j < child_number; ++j)
      child = InnerNextSibling(child);

    if (!child || child->nodeName() != child_name)
      return nullptr;
    node = child;
  }
  return node;
}

Response InspectorDOMAgent::pushNodeByPathToFrontend(const String& path,
                                                     int* node_id) {
  if (!enabled_.Get())
    return Response::ServerError("DOM agent is not enabled");
  if (Node* node = NodeForPath(path))
    *node_id = PushNodePathToFrontend(node);
  else
    return Response::ServerError("No node with given path found");
  return Response::Success();
}

Response InspectorDOMAgent::pushNodesByBackendIdsToFrontend(
    std::unique_ptr<protocol::Array<int>> backend_node_ids,
    std::unique_ptr<protocol::Array<int>>* result) {
  if (!document_ || !document_node_to_id_map_->Contains(document_))
    return Response::ServerError("Document needs to be requested first");

  *result = std::make_unique<protocol::Array<int>>();
  for (int id : *backend_node_ids) {
    Node* node = DOMNodeIds::NodeForId(id);
    if (node && node->GetDocument().GetFrame() &&
        inspected_frames_->Contains(node->GetDocument().GetFrame()))
      (*result)->emplace_back(PushNodePathToFrontend(node));
    else
      (*result)->emplace_back(0);
  }
  return Response::Success();
}

class InspectableNode final
    : public v8_inspector::V8InspectorSession::Inspectable {
 public:
  explicit InspectableNode(Node* node)
      : node_id_(DOMNodeIds::IdForNode(node)) {}

  v8::Local<v8::Value> get(v8::Local<v8::Context> context) override {
    return NodeV8Value(context, DOMNodeIds::NodeForId(node_id_));
  }

 private:
  DOMNodeId node_id_;
};

Response InspectorDOMAgent::setInspectedNode(int node_id) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, node);
  if (!response.IsSuccess())
    return response;
  v8_session_->addInspectedObject(std::make_unique<InspectableNode>(node));
  return Response::Success();
}

Response InspectorDOMAgent::getRelayoutBoundary(
    int node_id,
    int* relayout_boundary_node_id) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, node);
  if (!response.IsSuccess())
    return response;
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object) {
    return Response::ServerError(
        "No layout object for node, perhaps orphan or hidden node");
  }
  while (layout_object && !layout_object->IsDocumentElement() &&
         !layout_object->IsRelayoutBoundary())
    layout_object = layout_object->Container();
  Node* result_node =
      layout_object ? layout_object->GeneratingNode() : node->ownerDocument();
  *relayout_boundary_node_id = PushNodePathToFrontend(result_node);
  return Response::Success();
}

protocol::Response InspectorDOMAgent::describeNode(
    protocol::Maybe<int> node_id,
    protocol::Maybe<int> backend_node_id,
    protocol::Maybe<String> object_id,
    protocol::Maybe<int> depth,
    protocol::Maybe<bool> pierce,
    std::unique_ptr<protocol::DOM::Node>* result) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, backend_node_id, object_id, node);
  if (!response.IsSuccess())
    return response;
  if (!node)
    return Response::ServerError("Node not found");
  *result = BuildObjectForNode(node, depth.fromMaybe(0),
                               pierce.fromMaybe(false), nullptr, nullptr);
  return Response::Success();
}

protocol::Response InspectorDOMAgent::scrollIntoViewIfNeeded(
    protocol::Maybe<int> node_id,
    protocol::Maybe<int> backend_node_id,
    protocol::Maybe<String> object_id,
    protocol::Maybe<protocol::DOM::Rect> rect) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, backend_node_id, object_id, node);
  if (!response.IsSuccess())
    return response;
  node->GetDocument().EnsurePaintLocationDataValidForNode(
      node, DocumentUpdateReason::kInspector);
  if (!node->isConnected())
    return Response::ServerError("Node is detached from document");
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return Response::ServerError("Node does not have a layout object");
  PhysicalRect rect_to_scroll = PhysicalRect::EnclosingRect(
      layout_object->AbsoluteBoundingBoxFloatRect());
  if (rect.isJust()) {
    rect_to_scroll.SetX(rect_to_scroll.X() +
                        LayoutUnit(rect.fromJust()->getX()));
    rect_to_scroll.SetY(rect_to_scroll.Y() +
                        LayoutUnit(rect.fromJust()->getY()));
    rect_to_scroll.SetWidth(LayoutUnit(rect.fromJust()->getWidth()));
    rect_to_scroll.SetHeight(LayoutUnit(rect.fromJust()->getHeight()));
  }
  layout_object->ScrollRectToVisible(
      rect_to_scroll,
      ScrollAlignment::CreateScrollIntoViewParams(
          ScrollAlignment::CenterIfNeeded(), ScrollAlignment::CenterIfNeeded(),
          mojom::blink::ScrollType::kProgrammatic,
          true /* make_visible_in_visual_viewport */,
          mojom::blink::ScrollBehavior::kInstant,
          true /* is_for_scroll_sequence */, false /* zoom_into_rect */));
  return Response::Success();
}

protocol::Response InspectorDOMAgent::getFrameOwner(
    const String& frame_id,
    int* backend_node_id,
    protocol::Maybe<int>* node_id) {
  Frame* frame = inspected_frames_->Root();
  for (; frame; frame = frame->Tree().TraverseNext(inspected_frames_->Root())) {
    if (IdentifiersFactory::FrameId(frame) == frame_id)
      break;
  }
  if (!frame) {
    for (PortalContents* portal :
         DocumentPortals::From(*inspected_frames_->Root()->GetDocument())
             .GetPortals()) {
      frame = portal->GetFrame();
      if (IdentifiersFactory::FrameId(frame) == frame_id)
        break;
    }
  }
  if (!frame)
    return Response::ServerError("Frame with the given id was not found.");
  auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(frame->Owner());
  if (!frame_owner) {
    return Response::ServerError(
        "Frame with the given id does not belong to the target.");
  }

  *backend_node_id = IdentifiersFactory::IntIdForNode(frame_owner);

  if (enabled_.Get() && document_ &&
      document_node_to_id_map_->Contains(document_)) {
    *node_id = PushNodePathToFrontend(frame_owner);
  }
  return Response::Success();
}

Response InspectorDOMAgent::getFileInfo(const String& object_id, String* path) {
  v8::HandleScope handles(isolate_);
  v8::Local<v8::Value> value;
  v8::Local<v8::Context> context;
  std::unique_ptr<v8_inspector::StringBuffer> error;
  if (!v8_session_->unwrapObject(&error, ToV8InspectorStringView(object_id),
                                 &value, &context, nullptr))
    return Response::ServerError(ToCoreString(std::move(error)).Utf8());

  if (!V8File::HasInstance(value, isolate_))
    return Response::ServerError("Object id doesn't reference a File");
  File* file = V8File::ToImpl(v8::Local<v8::Object>::Cast(value));
  if (!file) {
    return Response::ServerError(
        "Couldn't convert object with given objectId to File");
  }

  *path = file->GetPath();
  return Response::Success();
}

void InspectorDOMAgent::Trace(Visitor* visitor) {
  visitor->Trace(dom_listener_);
  visitor->Trace(inspected_frames_);
  visitor->Trace(document_node_to_id_map_);
  visitor->Trace(dangling_node_to_id_maps_);
  visitor->Trace(id_to_node_);
  visitor->Trace(id_to_nodes_map_);
  visitor->Trace(document_);
  visitor->Trace(revalidate_task_);
  visitor->Trace(search_results_);
  visitor->Trace(history_);
  visitor->Trace(dom_editor_);
  visitor->Trace(node_to_creation_source_location_map_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink

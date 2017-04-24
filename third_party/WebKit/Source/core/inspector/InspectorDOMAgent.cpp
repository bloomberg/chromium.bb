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

#include "core/inspector/InspectorDOMAgent.h"

#include <memory>
#include "bindings/core/v8/BindingSecurity.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8Node.h"
#include "core/InputTypeNames.h"
#include "core/dom/Attr.h"
#include "core/dom/CharacterData.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/DOMException.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/DocumentType.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/StaticNodeList.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/serializers/Serialization.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/HTMLSlotElement.h"
#include "core/html/HTMLTemplateElement.h"
#include "core/html/imports/HTMLImportChild.h"
#include "core/html/imports/HTMLImportLoader.h"
#include "core/inspector/DOMEditor.h"
#include "core/inspector/DOMPatchSupport.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorHighlight.h"
#include "core/inspector/InspectorHistory.h"
#include "core/inspector/V8InspectorString.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/xml/DocumentXPathEvaluator.h"
#include "core/xml/XPathResult.h"
#include "platform/wtf/ListHashSet.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

using namespace HTMLNames;
using protocol::Maybe;
using protocol::Response;

namespace DOMAgentState {
static const char kDomAgentEnabled[] = "domAgentEnabled";
};

namespace {

const size_t kMaxTextSize = 10000;
const UChar kEllipsisUChar[] = {0x2026, 0};

Color ParseColor(protocol::DOM::RGBA* rgba) {
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

bool ParseQuad(std::unique_ptr<protocol::Array<double>> quad_array,
               FloatQuad* quad) {
  const size_t kCoordinatesInQuad = 8;
  if (!quad_array || quad_array->length() != kCoordinatesInQuad)
    return false;
  quad->SetP1(FloatPoint(quad_array->get(0), quad_array->get(1)));
  quad->SetP2(FloatPoint(quad_array->get(2), quad_array->get(3)));
  quad->SetP3(FloatPoint(quad_array->get(4), quad_array->get(5)));
  quad->SetP4(FloatPoint(quad_array->get(6), quad_array->get(7)));
  return true;
}

}  // namespace

class InspectorRevalidateDOMTask final
    : public GarbageCollectedFinalized<InspectorRevalidateDOMTask> {
 public:
  explicit InspectorRevalidateDOMTask(InspectorDOMAgent*);
  void ScheduleStyleAttrRevalidationFor(Element*);
  void Reset() { timer_.Stop(); }
  void OnTimer(TimerBase*);
  DECLARE_TRACE();

 private:
  Member<InspectorDOMAgent> dom_agent_;
  Timer<InspectorRevalidateDOMTask> timer_;
  HeapHashSet<Member<Element>> style_attr_invalidated_elements_;
};

InspectorRevalidateDOMTask::InspectorRevalidateDOMTask(
    InspectorDOMAgent* dom_agent)
    : dom_agent_(dom_agent),
      timer_(this, &InspectorRevalidateDOMTask::OnTimer) {}

void InspectorRevalidateDOMTask::ScheduleStyleAttrRevalidationFor(
    Element* element) {
  style_attr_invalidated_elements_.insert(element);
  if (!timer_.IsActive())
    timer_.StartOneShot(0, BLINK_FROM_HERE);
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

DEFINE_TRACE(InspectorRevalidateDOMTask) {
  visitor->Trace(dom_agent_);
  visitor->Trace(style_attr_invalidated_elements_);
}

Response InspectorDOMAgent::ToResponse(ExceptionState& exception_state) {
  if (exception_state.HadException()) {
    return Response::Error(DOMException::GetErrorName(exception_state.Code()) +
                           " " + exception_state.Message());
  }
  return Response::OK();
}

bool InspectorDOMAgent::GetPseudoElementType(PseudoId pseudo_id,
                                             protocol::DOM::PseudoType* type) {
  switch (pseudo_id) {
    case kPseudoIdFirstLine:
      *type = protocol::DOM::PseudoTypeEnum::FirstLine;
      return true;
    case kPseudoIdFirstLetter:
      *type = protocol::DOM::PseudoTypeEnum::FirstLetter;
      return true;
    case kPseudoIdBefore:
      *type = protocol::DOM::PseudoTypeEnum::Before;
      return true;
    case kPseudoIdAfter:
      *type = protocol::DOM::PseudoTypeEnum::After;
      return true;
    case kPseudoIdBackdrop:
      *type = protocol::DOM::PseudoTypeEnum::Backdrop;
      return true;
    case kPseudoIdSelection:
      *type = protocol::DOM::PseudoTypeEnum::Selection;
      return true;
    case kPseudoIdFirstLineInherited:
      *type = protocol::DOM::PseudoTypeEnum::FirstLineInherited;
      return true;
    case kPseudoIdScrollbar:
      *type = protocol::DOM::PseudoTypeEnum::Scrollbar;
      return true;
    case kPseudoIdScrollbarThumb:
      *type = protocol::DOM::PseudoTypeEnum::ScrollbarThumb;
      return true;
    case kPseudoIdScrollbarButton:
      *type = protocol::DOM::PseudoTypeEnum::ScrollbarButton;
      return true;
    case kPseudoIdScrollbarTrack:
      *type = protocol::DOM::PseudoTypeEnum::ScrollbarTrack;
      return true;
    case kPseudoIdScrollbarTrackPiece:
      *type = protocol::DOM::PseudoTypeEnum::ScrollbarTrackPiece;
      return true;
    case kPseudoIdScrollbarCorner:
      *type = protocol::DOM::PseudoTypeEnum::ScrollbarCorner;
      return true;
    case kPseudoIdResizer:
      *type = protocol::DOM::PseudoTypeEnum::Resizer;
      return true;
    case kPseudoIdInputListButton:
      *type = protocol::DOM::PseudoTypeEnum::InputListButton;
      return true;
    default:
      return false;
  }
}

InspectorDOMAgent::InspectorDOMAgent(
    v8::Isolate* isolate,
    InspectedFrames* inspected_frames,
    v8_inspector::V8InspectorSession* v8_session,
    Client* client)
    : isolate_(isolate),
      inspected_frames_(inspected_frames),
      v8_session_(v8_session),
      client_(client),
      dom_listener_(nullptr),
      document_node_to_id_map_(new NodeToIdMap()),
      last_node_id_(1),
      suppress_attribute_modified_event_(false),
      backend_node_id_to_inspect_(0) {}

InspectorDOMAgent::~InspectorDOMAgent() {}

void InspectorDOMAgent::Restore() {
  if (!Enabled())
    return;
  InnerEnable();
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

  if (!Enabled())
    return;

  // Immediately communicate 0 document or document that has finished loading.
  if (!doc || !doc->Parsing())
    GetFrontend()->documentUpdated();
}

void InspectorDOMAgent::ReleaseDanglingNodes() {
  dangling_node_to_id_maps_.clear();
}

int InspectorDOMAgent::Bind(Node* node, NodeToIdMap* nodes_map) {
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

  if (node->IsFrameOwnerElement()) {
    Document* content_document =
        ToHTMLFrameOwnerElement(node)->contentDocument();
    if (dom_listener_)
      dom_listener_->DidRemoveDocument(content_document);
    if (content_document)
      Unbind(content_document, nodes_map);
  }

  for (ShadowRoot* root = node->YoungestShadowRoot(); root;
       root = root->OlderShadowRoot())
    Unbind(root, nodes_map);

  if (node->IsElementNode()) {
    Element* element = ToElement(node);
    if (element->GetPseudoElement(kPseudoIdBefore))
      Unbind(element->GetPseudoElement(kPseudoIdBefore), nodes_map);
    if (element->GetPseudoElement(kPseudoIdAfter))
      Unbind(element->GetPseudoElement(kPseudoIdAfter), nodes_map);

    if (isHTMLLinkElement(*element)) {
      HTMLLinkElement& link_element = toHTMLLinkElement(*element);
      if (link_element.IsImport() && link_element.import())
        Unbind(link_element.import(), nodes_map);
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
    return Response::Error("Could not find node with given id");
  return Response::OK();
}

Response InspectorDOMAgent::AssertElement(int node_id, Element*& element) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, node);
  if (!response.isSuccess())
    return response;

  if (!node->IsElementNode())
    return Response::Error("Node is not an Element");
  element = ToElement(node);
  return Response::OK();
}

// static
ShadowRoot* InspectorDOMAgent::UserAgentShadowRoot(Node* node) {
  if (!node || !node->IsInShadowTree())
    return nullptr;

  Node* candidate = node;
  while (candidate && !candidate->IsShadowRoot())
    candidate = candidate->ParentOrShadowHostNode();
  ASSERT(candidate);
  ShadowRoot* shadow_root = ToShadowRoot(candidate);

  return shadow_root->GetType() == ShadowRootType::kUserAgent ? shadow_root
                                                              : nullptr;
}

Response InspectorDOMAgent::AssertEditableNode(int node_id, Node*& node) {
  Response response = AssertNode(node_id, node);
  if (!response.isSuccess())
    return response;

  if (node->IsInShadowTree()) {
    if (node->IsShadowRoot())
      return Response::Error("Cannot edit shadow roots");
    if (UserAgentShadowRoot(node))
      return Response::Error("Cannot edit nodes from user-agent shadow trees");
  }

  if (node->IsPseudoElement())
    return Response::Error("Cannot edit pseudo elements");
  return Response::OK();
}

Response InspectorDOMAgent::AssertEditableChildNode(Element* parent_element,
                                                    int node_id,
                                                    Node*& node) {
  Response response = AssertEditableNode(node_id, node);
  if (!response.isSuccess())
    return response;
  if (node->parentNode() != parent_element)
    return Response::Error("Anchor node must be child of the target element");
  return Response::OK();
}

Response InspectorDOMAgent::AssertEditableElement(int node_id,
                                                  Element*& element) {
  Response response = AssertElement(node_id, element);
  if (!response.isSuccess())
    return response;

  if (element->IsInShadowTree() && UserAgentShadowRoot(element))
    return Response::Error("Cannot edit elements from user-agent shadow trees");

  if (element->IsPseudoElement())
    return Response::Error("Cannot edit pseudo elements");

  return Response::OK();
}

void InspectorDOMAgent::InnerEnable() {
  state_->setBoolean(DOMAgentState::kDomAgentEnabled, true);
  history_ = new InspectorHistory();
  dom_editor_ = new DOMEditor(history_.Get());
  document_ = inspected_frames_->Root()->GetDocument();
  instrumenting_agents_->addInspectorDOMAgent(this);
  if (backend_node_id_to_inspect_)
    GetFrontend()->inspectNodeRequested(backend_node_id_to_inspect_);
  backend_node_id_to_inspect_ = 0;
}

Response InspectorDOMAgent::enable() {
  if (!Enabled())
    InnerEnable();
  return Response::OK();
}

bool InspectorDOMAgent::Enabled() const {
  return state_->booleanProperty(DOMAgentState::kDomAgentEnabled, false);
}

Response InspectorDOMAgent::disable() {
  if (!Enabled())
    return Response::Error("DOM agent hasn't been enabled");
  state_->setBoolean(DOMAgentState::kDomAgentEnabled, false);
  SetSearchingForNode(kNotSearching, Maybe<protocol::DOM::HighlightConfig>());
  instrumenting_agents_->removeInspectorDOMAgent(this);
  history_.Clear();
  dom_editor_.Clear();
  SetDocument(nullptr);
  return Response::OK();
}

Response InspectorDOMAgent::getDocument(
    Maybe<int> depth,
    Maybe<bool> pierce,
    std::unique_ptr<protocol::DOM::Node>* root) {
  // Backward compatibility. Mark agent as enabled when it requests document.
  if (!Enabled())
    InnerEnable();

  if (!document_)
    return Response::Error("Document is not available");

  DiscardFrontendBindings();

  int sanitized_depth = depth.fromMaybe(2);
  if (sanitized_depth == -1)
    sanitized_depth = INT_MAX;

  *root = BuildObjectForNode(document_.Get(), sanitized_depth,
                             pierce.fromMaybe(false),
                             document_node_to_id_map_.Get());
  return Response::OK();
}

Response InspectorDOMAgent::getFlattenedDocument(
    Maybe<int> depth,
    Maybe<bool> pierce,
    std::unique_ptr<protocol::Array<protocol::DOM::Node>>* nodes) {
  if (!Enabled())
    return Response::Error("Document not enabled");

  if (!document_)
    return Response::Error("Document is not available");

  DiscardFrontendBindings();

  int sanitized_depth = depth.fromMaybe(-1);
  if (sanitized_depth == -1)
    sanitized_depth = INT_MAX;

  nodes->reset(new protocol::Array<protocol::DOM::Node>());
  (*nodes)->addItem(BuildObjectForNode(
      document_.Get(), sanitized_depth, pierce.fromMaybe(false),
      document_node_to_id_map_.Get(), nodes->get()));
  return Response::OK();
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
      ASSERT(child_node_id);
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

  HeapHashMap<int, Member<Node>>::iterator it = id_to_node_.Find(id);
  if (it != id_to_node_.end())
    return it->value;
  return nullptr;
}

Response InspectorDOMAgent::collectClassNamesFromSubtree(
    int node_id,
    std::unique_ptr<protocol::Array<String>>* class_names) {
  HashSet<String> unique_names;
  *class_names = protocol::Array<String>::create();
  Node* parent_node = NodeForId(node_id);
  if (!parent_node ||
      (!parent_node->IsElementNode() && !parent_node->IsDocumentNode() &&
       !parent_node->IsDocumentFragment()))
    return Response::Error("No suitable node with given id found");

  for (Node* node = parent_node; node;
       node = FlatTreeTraversal::Next(*node, parent_node)) {
    if (node->IsElementNode()) {
      const Element& element = ToElement(*node);
      if (!element.HasClass())
        continue;
      const SpaceSplitString& class_name_list = element.ClassNames();
      for (unsigned i = 0; i < class_name_list.size(); ++i)
        unique_names.insert(class_name_list[i]);
    }
  }
  for (const String& class_name : unique_names)
    (*class_names)->addItem(class_name);
  return Response::OK();
}

Response InspectorDOMAgent::requestChildNodes(
    int node_id,
    Maybe<int> depth,
    Maybe<bool> maybe_taverse_frames) {
  int sanitized_depth = depth.fromMaybe(1);
  if (sanitized_depth == 0 || sanitized_depth < -1) {
    return Response::Error(
        "Please provide a positive integer as a depth or -1 for entire "
        "subtree");
  }
  if (sanitized_depth == -1)
    sanitized_depth = INT_MAX;

  PushChildNodesToFrontend(node_id, sanitized_depth,
                           maybe_taverse_frames.fromMaybe(false));
  return Response::OK();
}

Response InspectorDOMAgent::querySelector(int node_id,
                                          const String& selectors,
                                          int* element_id) {
  *element_id = 0;
  Node* node = nullptr;
  Response response = AssertNode(node_id, node);
  if (!response.isSuccess())
    return response;
  if (!node || !node->IsContainerNode())
    return Response::Error("Not a container node");

  DummyExceptionStateForTesting exception_state;
  Element* element = ToContainerNode(node)->QuerySelector(
      AtomicString(selectors), exception_state);
  if (exception_state.HadException())
    return Response::Error("DOM Error while querying");

  if (element)
    *element_id = PushNodePathToFrontend(element);
  return Response::OK();
}

Response InspectorDOMAgent::querySelectorAll(
    int node_id,
    const String& selectors,
    std::unique_ptr<protocol::Array<int>>* result) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, node);
  if (!response.isSuccess())
    return response;
  if (!node || !node->IsContainerNode())
    return Response::Error("Not a container node");

  DummyExceptionStateForTesting exception_state;
  StaticElementList* elements = ToContainerNode(node)->QuerySelectorAll(
      AtomicString(selectors), exception_state);
  if (exception_state.HadException())
    return Response::Error("DOM Error while querying");

  *result = protocol::Array<int>::create();

  for (unsigned i = 0; i < elements->length(); ++i)
    (*result)->addItem(PushNodePathToFrontend(elements->item(i)));
  return Response::OK();
}

int InspectorDOMAgent::PushNodePathToFrontend(Node* node_to_push,
                                              NodeToIdMap* node_map) {
  ASSERT(node_to_push);  // Invalid input
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
    ASSERT(node_id);
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
  NodeToIdMap* new_map = new NodeToIdMap;
  NodeToIdMap* dangling_map = new_map;
  dangling_node_to_id_maps_.push_back(new_map);
  std::unique_ptr<protocol::Array<protocol::DOM::Node>> children =
      protocol::Array<protocol::DOM::Node>::create();
  children->addItem(BuildObjectForNode(node, 0, false, dangling_map));
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
  if (!response.isSuccess())
    return response;
  return dom_editor_->SetAttribute(element, name, value);
}

Response InspectorDOMAgent::setAttributesAsText(int element_id,
                                                const String& text,
                                                Maybe<String> name) {
  Element* element = nullptr;
  Response response = AssertEditableElement(element_id, element);
  if (!response.isSuccess())
    return response;

  String markup = "<span " + text + "></span>";
  DocumentFragment* fragment = element->GetDocument().createDocumentFragment();

  bool should_ignore_case =
      element->GetDocument().IsHTMLDocument() && element->IsHTMLElement();
  // Not all elements can represent the context (i.e. IFRAME), hence using
  // document.body.
  if (should_ignore_case && element->GetDocument().body())
    fragment->ParseHTML(markup, element->GetDocument().body(),
                        kAllowScriptingContent);
  else
    fragment->ParseXML(markup, 0, kAllowScriptingContent);

  Element* parsed_element =
      fragment->firstChild() && fragment->firstChild()->IsElementNode()
          ? ToElement(fragment->firstChild())
          : nullptr;
  if (!parsed_element)
    return Response::Error("Could not parse value as attributes");

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
    if (!response.isSuccess())
      return response;
  }

  if (!found_original_attribute && name.isJust() &&
      !name.fromJust().StripWhiteSpace().IsEmpty()) {
    return dom_editor_->RemoveAttribute(element, case_adjusted_name);
  }
  return Response::OK();
}

Response InspectorDOMAgent::removeAttribute(int element_id,
                                            const String& name) {
  Element* element = nullptr;
  Response response = AssertEditableElement(element_id, element);
  if (!response.isSuccess())
    return response;

  return dom_editor_->RemoveAttribute(element, name);
}

Response InspectorDOMAgent::removeNode(int node_id) {
  Node* node = nullptr;
  Response response = AssertEditableNode(node_id, node);
  if (!response.isSuccess())
    return response;

  ContainerNode* parent_node = node->parentNode();
  if (!parent_node)
    return Response::Error("Cannot remove detached node");

  return dom_editor_->RemoveChild(parent_node, node);
}

Response InspectorDOMAgent::setNodeName(int node_id,
                                        const String& tag_name,
                                        int* new_id) {
  *new_id = 0;

  Element* old_element = nullptr;
  Response response = AssertElement(node_id, old_element);
  if (!response.isSuccess())
    return response;

  DummyExceptionStateForTesting exception_state;
  Element* new_elem = old_element->GetDocument().createElement(
      AtomicString(tag_name), exception_state);
  if (exception_state.HadException())
    return ToResponse(exception_state);

  // Copy over the original node's attributes.
  new_elem->CloneAttributesFromElement(*old_element);

  // Copy over the original node's children.
  for (Node* child = old_element->firstChild(); child;
       child = old_element->firstChild()) {
    response = dom_editor_->InsertBefore(new_elem, child, 0);
    if (!response.isSuccess())
      return response;
  }

  // Replace the old node with the new node
  ContainerNode* parent = old_element->parentNode();
  response =
      dom_editor_->InsertBefore(parent, new_elem, old_element->nextSibling());
  if (!response.isSuccess())
    return response;
  response = dom_editor_->RemoveChild(parent, old_element);
  if (!response.isSuccess())
    return response;

  *new_id = PushNodePathToFrontend(new_elem);
  if (children_requested_.Contains(node_id))
    PushChildNodesToFrontend(*new_id);
  return Response::OK();
}

Response InspectorDOMAgent::getOuterHTML(int node_id, WTF::String* outer_html) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, node);
  if (!response.isSuccess())
    return response;

  *outer_html = CreateMarkup(node);
  return Response::OK();
}

Response InspectorDOMAgent::setOuterHTML(int node_id,
                                         const String& outer_html) {
  if (!node_id) {
    ASSERT(document_);
    DOMPatchSupport dom_patch_support(dom_editor_.Get(), *document_.Get());
    dom_patch_support.PatchDocument(outer_html);
    return Response::OK();
  }

  Node* node = nullptr;
  Response response = AssertEditableNode(node_id, node);
  if (!response.isSuccess())
    return response;

  Document* document =
      node->IsDocumentNode() ? ToDocument(node) : node->ownerDocument();
  if (!document || (!document->IsHTMLDocument() && !document->IsXMLDocument()))
    return Response::Error("Not an HTML/XML document");

  Node* new_node = nullptr;
  response = dom_editor_->SetOuterHTML(node, outer_html, &new_node);
  if (!response.isSuccess())
    return response;

  if (!new_node) {
    // The only child node has been deleted.
    return Response::OK();
  }

  int new_id = PushNodePathToFrontend(new_node);

  bool children_requested = children_requested_.Contains(node_id);
  if (children_requested)
    PushChildNodesToFrontend(new_id);
  return Response::OK();
}

Response InspectorDOMAgent::setNodeValue(int node_id, const String& value) {
  Node* node = nullptr;
  Response response = AssertEditableNode(node_id, node);
  if (!response.isSuccess())
    return response;

  if (node->getNodeType() != Node::kTextNode)
    return Response::Error("Can only set value of text nodes");

  return dom_editor_->ReplaceWholeText(ToText(node), value);
}

static Node* NextNodeWithShadowDOMInMind(const Node& current,
                                         const Node* stay_within,
                                         bool include_user_agent_shadow_dom) {
  // At first traverse the subtree.
  if (current.IsElementNode()) {
    const Element& element = ToElement(current);
    ElementShadow* element_shadow = element.Shadow();
    if (element_shadow) {
      ShadowRoot& shadow_root = element_shadow->YoungestShadowRoot();
      if (shadow_root.GetType() != ShadowRootType::kUserAgent ||
          include_user_agent_shadow_dom)
        return &shadow_root;
    }
  }
  if (current.hasChildren())
    return current.firstChild();

  // Then traverse siblings of the node itself and its ancestors.
  const Node* node = &current;
  do {
    if (node == stay_within)
      return nullptr;
    if (node->IsShadowRoot()) {
      const ShadowRoot* shadow_root = ToShadowRoot(node);
      if (shadow_root->OlderShadowRoot())
        return shadow_root->OlderShadowRoot();
      Element& host = shadow_root->host();
      if (host.HasChildren())
        return host.firstChild();
    }
    if (node->nextSibling())
      return node->nextSibling();
    node =
        node->IsShadowRoot() ? &ToShadowRoot(node)->host() : node->parentNode();
  } while (node);

  return nullptr;
}

Response InspectorDOMAgent::performSearch(
    const String& whitespace_trimmed_query,
    Maybe<bool> optional_include_user_agent_shadow_dom,
    String* search_id,
    int* result_count) {
  // FIXME: Few things are missing here:
  // 1) Search works with node granularity - number of matches within node is
  //    not calculated.
  // 2) There is no need to push all search results to the front-end at a time,
  //    pushing next / previous result is sufficient.

  bool include_user_agent_shadow_dom =
      optional_include_user_agent_shadow_dom.fromMaybe(false);

  unsigned query_length = whitespace_trimmed_query.length();
  bool start_tag_found = !whitespace_trimmed_query.Find('<');
  bool end_tag_found =
      whitespace_trimmed_query.ReverseFind('>') + 1 == query_length;
  bool start_quote_found = !whitespace_trimmed_query.Find('"');
  bool end_quote_found =
      whitespace_trimmed_query.ReverseFind('"') + 1 == query_length;
  bool exact_attribute_match = start_quote_found && end_quote_found;

  String tag_name_query = whitespace_trimmed_query;
  String attribute_query = whitespace_trimmed_query;
  if (start_tag_found)
    tag_name_query = tag_name_query.Right(tag_name_query.length() - 1);
  if (end_tag_found)
    tag_name_query = tag_name_query.Left(tag_name_query.length() - 1);
  if (start_quote_found)
    attribute_query = attribute_query.Right(attribute_query.length() - 1);
  if (end_quote_found)
    attribute_query = attribute_query.Left(attribute_query.length() - 1);

  HeapVector<Member<Document>> docs = Documents();
  HeapListHashSet<Member<Node>> result_collector;

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
               node->nodeName().StartsWith(tag_name_query,
                                           kTextCaseUnicodeInsensitive)) ||
              (!start_tag_found && end_tag_found &&
               node->nodeName().EndsWith(tag_name_query,
                                         kTextCaseUnicodeInsensitive))) {
            result_collector.insert(node);
            break;
          }
          // Go through all attributes and serialize them.
          const Element* element = ToElement(node);
          AttributeCollection attributes = element->Attributes();
          for (auto& attribute : attributes) {
            // Add attribute pair
            if (attribute.LocalName().Find(whitespace_trimmed_query, 0,
                                           kTextCaseUnicodeInsensitive) !=
                kNotFound) {
              result_collector.insert(node);
              break;
            }
            size_t found_position = attribute.Value().Find(
                attribute_query, 0, kTextCaseUnicodeInsensitive);
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
      ASSERT(document);
      DummyExceptionStateForTesting exception_state;
      XPathResult* result = DocumentXPathEvaluator::evaluate(
          *document, whitespace_trimmed_query, document, nullptr,
          XPathResult::kOrderedNodeSnapshotType, ScriptValue(),
          exception_state);
      if (exception_state.HadException() || !result)
        continue;

      unsigned long size = result->snapshotLength(exception_state);
      for (unsigned long i = 0; !exception_state.HadException() && i < size;
           ++i) {
        Node* node = result->snapshotItem(i, exception_state);
        if (exception_state.HadException())
          break;

        if (node->getNodeType() == Node::kAttributeNode)
          node = ToAttr(node)->ownerElement();
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
  return Response::OK();
}

Response InspectorDOMAgent::getSearchResults(
    const String& search_id,
    int from_index,
    int to_index,
    std::unique_ptr<protocol::Array<int>>* node_ids) {
  SearchResults::iterator it = search_results_.Find(search_id);
  if (it == search_results_.end())
    return Response::Error("No search session with given id found");

  int size = it->value.size();
  if (from_index < 0 || to_index > size || from_index >= to_index)
    return Response::Error("Invalid search result range");

  *node_ids = protocol::Array<int>::create();
  for (int i = from_index; i < to_index; ++i)
    (*node_ids)->addItem(PushNodePathToFrontend((it->value)[i].Get()));
  return Response::OK();
}

Response InspectorDOMAgent::discardSearchResults(const String& search_id) {
  search_results_.erase(search_id);
  return Response::OK();
}

void InspectorDOMAgent::Inspect(Node* inspected_node) {
  if (!inspected_node)
    return;

  Node* node = inspected_node;
  while (node && !node->IsElementNode() && !node->IsDocumentNode() &&
         !node->IsDocumentFragment())
    node = node->ParentOrShadowHostNode();
  if (!node)
    return;

  int backend_node_id = DOMNodeIds::IdForNode(node);
  if (!GetFrontend() || !Enabled()) {
    backend_node_id_to_inspect_ = backend_node_id;
    return;
  }

  GetFrontend()->inspectNodeRequested(backend_node_id);
}

void InspectorDOMAgent::NodeHighlightedInOverlay(Node* node) {
  if (!GetFrontend() || !Enabled())
    return;

  while (node && !node->IsElementNode() && !node->IsDocumentNode() &&
         !node->IsDocumentFragment())
    node = node->ParentOrShadowHostNode();

  if (!node)
    return;

  int node_id = PushNodePathToFrontend(node);
  GetFrontend()->nodeHighlightRequested(node_id);
}

Response InspectorDOMAgent::SetSearchingForNode(
    SearchMode search_mode,
    Maybe<protocol::DOM::HighlightConfig> highlight_inspector_object) {
  if (!client_)
    return Response::OK();
  if (search_mode == kNotSearching) {
    client_->SetInspectMode(kNotSearching, nullptr);
    return Response::OK();
  }
  std::unique_ptr<InspectorHighlightConfig> config;
  Response response = HighlightConfigFromInspectorObject(
      std::move(highlight_inspector_object), &config);
  if (!response.isSuccess())
    return response;
  client_->SetInspectMode(search_mode, std::move(config));
  return Response::OK();
}

Response InspectorDOMAgent::HighlightConfigFromInspectorObject(
    Maybe<protocol::DOM::HighlightConfig> highlight_inspector_object,
    std::unique_ptr<InspectorHighlightConfig>* out_config) {
  if (!highlight_inspector_object.isJust()) {
    return Response::Error(
        "Internal error: highlight configuration parameter is missing");
  }

  protocol::DOM::HighlightConfig* config =
      highlight_inspector_object.fromJust();
  std::unique_ptr<InspectorHighlightConfig> highlight_config =
      WTF::MakeUnique<InspectorHighlightConfig>();
  highlight_config->show_info = config->getShowInfo(false);
  highlight_config->show_rulers = config->getShowRulers(false);
  highlight_config->show_extension_lines = config->getShowExtensionLines(false);
  highlight_config->display_as_material = config->getDisplayAsMaterial(false);
  highlight_config->content = ParseColor(config->getContentColor(nullptr));
  highlight_config->padding = ParseColor(config->getPaddingColor(nullptr));
  highlight_config->border = ParseColor(config->getBorderColor(nullptr));
  highlight_config->margin = ParseColor(config->getMarginColor(nullptr));
  highlight_config->event_target =
      ParseColor(config->getEventTargetColor(nullptr));
  highlight_config->shape = ParseColor(config->getShapeColor(nullptr));
  highlight_config->shape_margin =
      ParseColor(config->getShapeMarginColor(nullptr));
  highlight_config->selector_list = config->getSelectorList("");

  *out_config = std::move(highlight_config);
  return Response::OK();
}

Response InspectorDOMAgent::setInspectMode(
    const String& mode,
    Maybe<protocol::DOM::HighlightConfig> highlight_config) {
  SearchMode search_mode;
  if (mode == protocol::DOM::InspectModeEnum::SearchForNode) {
    search_mode = kSearchingForNormal;
  } else if (mode == protocol::DOM::InspectModeEnum::SearchForUAShadowDOM) {
    search_mode = kSearchingForUAShadow;
  } else if (mode == protocol::DOM::InspectModeEnum::None) {
    search_mode = kNotSearching;
  } else {
    return Response::Error(
        String("Unknown mode \"" + mode + "\" was provided."));
  }

  if (search_mode != kNotSearching) {
    Response response = PushDocumentUponHandlelessOperation();
    if (!response.isSuccess())
      return response;
  }

  return SetSearchingForNode(search_mode, std::move(highlight_config));
}

Response InspectorDOMAgent::highlightRect(
    int x,
    int y,
    int width,
    int height,
    Maybe<protocol::DOM::RGBA> color,
    Maybe<protocol::DOM::RGBA> outline_color) {
  std::unique_ptr<FloatQuad> quad =
      WTF::WrapUnique(new FloatQuad(FloatRect(x, y, width, height)));
  InnerHighlightQuad(std::move(quad), std::move(color),
                     std::move(outline_color));
  return Response::OK();
}

Response InspectorDOMAgent::highlightQuad(
    std::unique_ptr<protocol::Array<double>> quad_array,
    Maybe<protocol::DOM::RGBA> color,
    Maybe<protocol::DOM::RGBA> outline_color) {
  std::unique_ptr<FloatQuad> quad = WTF::MakeUnique<FloatQuad>();
  if (!ParseQuad(std::move(quad_array), quad.get()))
    return Response::Error("Invalid Quad format");
  InnerHighlightQuad(std::move(quad), std::move(color),
                     std::move(outline_color));
  return Response::OK();
}

void InspectorDOMAgent::InnerHighlightQuad(
    std::unique_ptr<FloatQuad> quad,
    Maybe<protocol::DOM::RGBA> color,
    Maybe<protocol::DOM::RGBA> outline_color) {
  std::unique_ptr<InspectorHighlightConfig> highlight_config =
      WTF::MakeUnique<InspectorHighlightConfig>();
  highlight_config->content = ParseColor(color.fromMaybe(nullptr));
  highlight_config->content_outline =
      ParseColor(outline_color.fromMaybe(nullptr));
  if (client_)
    client_->HighlightQuad(std::move(quad), *highlight_config);
}

Response InspectorDOMAgent::NodeForRemoteId(const String& object_id,
                                            Node*& node) {
  v8::HandleScope handles(isolate_);
  v8::Local<v8::Value> value;
  v8::Local<v8::Context> context;
  std::unique_ptr<v8_inspector::StringBuffer> error;
  if (!v8_session_->unwrapObject(&error, ToV8InspectorStringView(object_id),
                                 &value, &context, nullptr))
    return Response::Error(ToCoreString(std::move(error)));
  if (!V8Node::hasInstance(value, isolate_))
    return Response::Error("Object id doesn't reference a Node");
  node = V8Node::toImpl(v8::Local<v8::Object>::Cast(value));
  if (!node) {
    return Response::Error(
        "Couldn't convert object with given objectId to Node");
  }
  return Response::OK();
}

Response InspectorDOMAgent::highlightNode(
    std::unique_ptr<protocol::DOM::HighlightConfig> highlight_inspector_object,
    Maybe<int> node_id,
    Maybe<int> backend_node_id,
    Maybe<String> object_id) {
  Node* node = nullptr;
  Response response;
  if (node_id.isJust()) {
    response = AssertNode(node_id.fromJust(), node);
  } else if (backend_node_id.isJust()) {
    node = DOMNodeIds::NodeForId(backend_node_id.fromJust());
    response = !node ? Response::Error("No node found for given backend id")
                     : Response::OK();
  } else if (object_id.isJust()) {
    response = NodeForRemoteId(object_id.fromJust(), node);
  } else {
    response = Response::Error("Either nodeId or objectId must be specified");
  }

  if (!response.isSuccess())
    return response;

  std::unique_ptr<InspectorHighlightConfig> highlight_config;
  response = HighlightConfigFromInspectorObject(
      std::move(highlight_inspector_object), &highlight_config);
  if (!response.isSuccess())
    return response;

  if (client_)
    client_->HighlightNode(node, *highlight_config, false);
  return Response::OK();
}

Response InspectorDOMAgent::highlightFrame(
    const String& frame_id,
    Maybe<protocol::DOM::RGBA> color,
    Maybe<protocol::DOM::RGBA> outline_color) {
  LocalFrame* frame =
      IdentifiersFactory::FrameById(inspected_frames_, frame_id);
  // FIXME: Inspector doesn't currently work cross process.
  if (frame && frame->DeprecatedLocalOwner()) {
    std::unique_ptr<InspectorHighlightConfig> highlight_config =
        WTF::MakeUnique<InspectorHighlightConfig>();
    highlight_config->show_info = true;  // Always show tooltips for frames.
    highlight_config->content = ParseColor(color.fromMaybe(nullptr));
    highlight_config->content_outline =
        ParseColor(outline_color.fromMaybe(nullptr));
    if (client_)
      client_->HighlightNode(frame->DeprecatedLocalOwner(), *highlight_config,
                             false);
  }
  return Response::OK();
}

Response InspectorDOMAgent::hideHighlight() {
  if (client_)
    client_->HideHighlight();
  return Response::OK();
}

Response InspectorDOMAgent::copyTo(int node_id,
                                   int target_element_id,
                                   Maybe<int> anchor_node_id,
                                   int* new_node_id) {
  Node* node = nullptr;
  Response response = AssertEditableNode(node_id, node);
  if (!response.isSuccess())
    return response;

  Element* target_element = nullptr;
  response = AssertEditableElement(target_element_id, target_element);
  if (!response.isSuccess())
    return response;

  Node* anchor_node = nullptr;
  if (anchor_node_id.isJust() && anchor_node_id.fromJust()) {
    response = AssertEditableChildNode(target_element,
                                       anchor_node_id.fromJust(), anchor_node);
    if (!response.isSuccess())
      return response;
  }

  // The clone is deep by default.
  Node* cloned_node = node->cloneNode(true);
  if (!cloned_node)
    return Response::Error("Failed to clone node");
  response =
      dom_editor_->InsertBefore(target_element, cloned_node, anchor_node);
  if (!response.isSuccess())
    return response;

  *new_node_id = PushNodePathToFrontend(cloned_node);
  return Response::OK();
}

Response InspectorDOMAgent::moveTo(int node_id,
                                   int target_element_id,
                                   Maybe<int> anchor_node_id,
                                   int* new_node_id) {
  Node* node = nullptr;
  Response response = AssertEditableNode(node_id, node);
  if (!response.isSuccess())
    return response;

  Element* target_element = nullptr;
  response = AssertEditableElement(target_element_id, target_element);
  if (!response.isSuccess())
    return response;

  Node* current = target_element;
  while (current) {
    if (current == node)
      return Response::Error("Unable to move node into self or descendant");
    current = current->parentNode();
  }

  Node* anchor_node = nullptr;
  if (anchor_node_id.isJust() && anchor_node_id.fromJust()) {
    response = AssertEditableChildNode(target_element,
                                       anchor_node_id.fromJust(), anchor_node);
    if (!response.isSuccess())
      return response;
  }

  response = dom_editor_->InsertBefore(target_element, node, anchor_node);
  if (!response.isSuccess())
    return response;

  *new_node_id = PushNodePathToFrontend(node);
  return Response::OK();
}

Response InspectorDOMAgent::undo() {
  DummyExceptionStateForTesting exception_state;
  history_->Undo(exception_state);
  return InspectorDOMAgent::ToResponse(exception_state);
}

Response InspectorDOMAgent::redo() {
  DummyExceptionStateForTesting exception_state;
  history_->Redo(exception_state);
  return InspectorDOMAgent::ToResponse(exception_state);
}

Response InspectorDOMAgent::markUndoableState() {
  history_->MarkUndoableState();
  return Response::OK();
}

Response InspectorDOMAgent::focus(int node_id) {
  Element* element = nullptr;
  Response response = AssertElement(node_id, element);
  if (!response.isSuccess())
    return response;

  element->GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  if (!element->IsFocusable())
    return Response::Error("Element is not focusable");
  element->focus();
  return Response::OK();
}

Response InspectorDOMAgent::setFileInputFiles(
    int node_id,
    std::unique_ptr<protocol::Array<String>> files) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, node);
  if (!response.isSuccess())
    return response;
  if (!isHTMLInputElement(*node) ||
      toHTMLInputElement(*node).type() != InputTypeNames::file)
    return Response::Error("Node is not a file input element");

  Vector<String> paths;
  for (size_t index = 0; index < files->length(); ++index)
    paths.push_back(files->get(index));
  toHTMLInputElement(node)->SetFilesFromPaths(paths);
  return Response::OK();
}

Response InspectorDOMAgent::getBoxModel(
    int node_id,
    std::unique_ptr<protocol::DOM::BoxModel>* model) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, node);
  if (!response.isSuccess())
    return response;

  bool result = InspectorHighlight::GetBoxModel(node, model);
  if (!result)
    return Response::Error("Could not compute box model.");
  return Response::OK();
}

Response InspectorDOMAgent::getNodeForLocation(
    int x,
    int y,
    Maybe<bool> optional_include_user_agent_shadow_dom,
    int* node_id) {
  bool include_user_agent_shadow_dom =
      optional_include_user_agent_shadow_dom.fromMaybe(false);
  Response response = PushDocumentUponHandlelessOperation();
  if (!response.isSuccess())
    return response;
  HitTestRequest request(HitTestRequest::kMove | HitTestRequest::kReadOnly |
                         HitTestRequest::kAllowChildFrameContent);
  HitTestResult result(request, IntPoint(x, y));
  document_->GetFrame()->ContentLayoutItem().HitTest(result);
  if (!include_user_agent_shadow_dom)
    result.SetToShadowHostIfInRestrictedShadowRoot();
  Node* node = result.InnerPossiblyPseudoNode();
  while (node && node->getNodeType() == Node::kTextNode)
    node = node->parentNode();
  if (!node)
    return Response::Error("No node found at given location");
  *node_id = PushNodePathToFrontend(node);
  return Response::OK();
}

Response InspectorDOMAgent::resolveNode(
    int node_id,
    Maybe<String> object_group,
    std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>*
        result) {
  String object_group_name = object_group.fromMaybe("");
  Node* node = NodeForId(node_id);
  if (!node)
    return Response::Error("No node with given id found");
  *result = ResolveNode(node, object_group_name);
  if (!*result) {
    return Response::Error(
        "Node with given id does not belong to the document");
  }
  return Response::OK();
}

Response InspectorDOMAgent::getAttributes(
    int node_id,
    std::unique_ptr<protocol::Array<String>>* result) {
  Element* element = nullptr;
  Response response = AssertElement(node_id, element);
  if (!response.isSuccess())
    return response;

  *result = BuildArrayForElementAttributes(element);
  return Response::OK();
}

Response InspectorDOMAgent::requestNode(const String& object_id, int* node_id) {
  Node* node = nullptr;
  Response response = NodeForRemoteId(object_id, node);
  if (!response.isSuccess())
    return response;
  *node_id = PushNodePathToFrontend(node);
  return Response::OK();
}

// static
String InspectorDOMAgent::DocumentURLString(Document* document) {
  if (!document || document->Url().IsNull())
    return "";
  return document->Url().GetString();
}

static String DocumentBaseURLString(Document* document) {
  return document->BaseURLForOverride(document->BaseURL()).GetString();
}

static protocol::DOM::ShadowRootType GetShadowRootType(
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
  ASSERT_NOT_REACHED();
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
      local_name = ToAttr(node)->localName();
      break;
    case Node::kElementNode:
      local_name = ToElement(node)->localName();
      break;
    default:
      break;
  }

  std::unique_ptr<protocol::DOM::Node> value =
      protocol::DOM::Node::create()
          .setNodeId(id)
          .setBackendNodeId(DOMNodeIds::IdForNode(node))
          .setNodeType(static_cast<int>(node->getNodeType()))
          .setNodeName(node->nodeName())
          .setLocalName(local_name)
          .setNodeValue(node_value)
          .build();

  if (node->IsSVGElement())
    value->setIsSVG(true);

  bool force_push_children = false;
  if (node->IsElementNode()) {
    Element* element = ToElement(node);
    value->setAttributes(BuildArrayForElementAttributes(element));

    if (node->IsFrameOwnerElement()) {
      HTMLFrameOwnerElement* frame_owner = ToHTMLFrameOwnerElement(node);
      if (LocalFrame* frame =
              frame_owner->ContentFrame() &&
                      frame_owner->ContentFrame()->IsLocalFrame()
                  ? ToLocalFrame(frame_owner->ContentFrame())
                  : nullptr)
        value->setFrameId(IdentifiersFactory::FrameId(frame));
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

    ElementShadow* shadow = element->Shadow();
    if (shadow) {
      std::unique_ptr<protocol::Array<protocol::DOM::Node>> shadow_roots =
          protocol::Array<protocol::DOM::Node>::create();
      for (ShadowRoot* root = &shadow->YoungestShadowRoot(); root;
           root = root->OlderShadowRoot()) {
        shadow_roots->addItem(BuildObjectForNode(
            root, pierce ? depth : 0, pierce, nodes_map, flatten_result));
      }
      value->setShadowRoots(std::move(shadow_roots));
      force_push_children = true;
    }

    if (isHTMLLinkElement(*element)) {
      HTMLLinkElement& link_element = toHTMLLinkElement(*element);
      if (link_element.IsImport() && link_element.import() &&
          InnerParentNode(link_element.import()) == link_element) {
        value->setImportedDocument(BuildObjectForNode(
            link_element.import(), 0, pierce, nodes_map, flatten_result));
      }
      force_push_children = true;
    }

    if (isHTMLTemplateElement(*element)) {
      value->setTemplateContent(
          BuildObjectForNode(toHTMLTemplateElement(*element).content(), 0,
                             pierce, nodes_map, flatten_result));
      force_push_children = true;
    }

    if (element->GetPseudoId()) {
      protocol::DOM::PseudoType pseudo_type;
      if (InspectorDOMAgent::GetPseudoElementType(element->GetPseudoId(),
                                                  &pseudo_type))
        value->setPseudoType(pseudo_type);
    } else {
      std::unique_ptr<protocol::Array<protocol::DOM::Node>> pseudo_elements =
          BuildArrayForPseudoElements(element, nodes_map);
      if (pseudo_elements) {
        value->setPseudoElements(std::move(pseudo_elements));
        force_push_children = true;
      }
      if (!element->ownerDocument()->xmlVersion().IsEmpty())
        value->setXmlVersion(element->ownerDocument()->xmlVersion());
    }

    if (element->IsInsertionPoint()) {
      value->setDistributedNodes(
          BuildArrayForDistributedNodes(ToInsertionPoint(element)));
      force_push_children = true;
    }
    if (isHTMLSlotElement(*element)) {
      value->setDistributedNodes(
          BuildDistributedNodesForSlot(toHTMLSlotElement(element)));
      force_push_children = true;
    }
  } else if (node->IsDocumentNode()) {
    Document* document = ToDocument(node);
    value->setDocumentURL(DocumentURLString(document));
    value->setBaseURL(DocumentBaseURLString(document));
    value->setXmlVersion(document->xmlVersion());
  } else if (node->IsDocumentTypeNode()) {
    DocumentType* doc_type = ToDocumentType(node);
    value->setPublicId(doc_type->publicId());
    value->setSystemId(doc_type->systemId());
  } else if (node->IsAttributeNode()) {
    Attr* attribute = ToAttr(node);
    value->setName(attribute->name());
    value->setValue(attribute->value());
  } else if (node->IsShadowRoot()) {
    value->setShadowRootType(GetShadowRootType(ToShadowRoot(node)));
  }

  if (node->IsContainerNode()) {
    int node_count = InnerChildNodeCount(node);
    value->setChildNodeCount(node_count);
    if (nodes_map == document_node_to_id_map_)
      cached_child_count_.Set(id, node_count);
    if (force_push_children && !depth)
      depth = 1;
    std::unique_ptr<protocol::Array<protocol::DOM::Node>> children =
        BuildArrayForContainerChildren(node, depth, pierce, nodes_map,
                                       flatten_result);
    if (children->length() > 0 ||
        depth)  // Push children along with shadow in any case.
      value->setChildren(std::move(children));
  }

  return value;
}

std::unique_ptr<protocol::Array<String>>
InspectorDOMAgent::BuildArrayForElementAttributes(Element* element) {
  std::unique_ptr<protocol::Array<String>> attributes_value =
      protocol::Array<String>::create();
  // Go through all attributes and serialize them.
  AttributeCollection attributes = element->Attributes();
  for (auto& attribute : attributes) {
    // Add attribute pair
    attributes_value->addItem(attribute.GetName().ToString());
    attributes_value->addItem(attribute.Value());
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
  std::unique_ptr<protocol::Array<protocol::DOM::Node>> children =
      protocol::Array<protocol::DOM::Node>::create();
  if (depth == 0) {
    // Special-case the only text child - pretend that container's children have
    // been requested.
    Node* first_child = container->firstChild();
    if (first_child && first_child->getNodeType() == Node::kTextNode &&
        !first_child->nextSibling()) {
      std::unique_ptr<protocol::DOM::Node> child_node =
          BuildObjectForNode(first_child, 0, pierce, nodes_map, flatten_result);
      child_node->setParentId(Bind(container, nodes_map));
      if (flatten_result) {
        flatten_result->addItem(std::move(child_node));
      } else {
        children->addItem(std::move(child_node));
      }
      children_requested_.insert(Bind(container, nodes_map));
    }
    return children;
  }

  Node* child = InnerFirstChild(container);
  depth--;
  children_requested_.insert(Bind(container, nodes_map));

  while (child) {
    std::unique_ptr<protocol::DOM::Node> child_node =
        BuildObjectForNode(child, depth, pierce, nodes_map, flatten_result);
    child_node->setParentId(Bind(container, nodes_map));
    if (flatten_result) {
      flatten_result->addItem(std::move(child_node));
    } else {
      children->addItem(std::move(child_node));
    }
    children_requested_.insert(Bind(container, nodes_map));
    child = InnerNextSibling(child);
  }
  return children;
}

std::unique_ptr<protocol::Array<protocol::DOM::Node>>
InspectorDOMAgent::BuildArrayForPseudoElements(Element* element,
                                               NodeToIdMap* nodes_map) {
  if (!element->GetPseudoElement(kPseudoIdBefore) &&
      !element->GetPseudoElement(kPseudoIdAfter))
    return nullptr;

  std::unique_ptr<protocol::Array<protocol::DOM::Node>> pseudo_elements =
      protocol::Array<protocol::DOM::Node>::create();
  if (element->GetPseudoElement(kPseudoIdBefore)) {
    pseudo_elements->addItem(BuildObjectForNode(
        element->GetPseudoElement(kPseudoIdBefore), 0, false, nodes_map));
  }
  if (element->GetPseudoElement(kPseudoIdAfter)) {
    pseudo_elements->addItem(BuildObjectForNode(
        element->GetPseudoElement(kPseudoIdAfter), 0, false, nodes_map));
  }
  return pseudo_elements;
}

std::unique_ptr<protocol::Array<protocol::DOM::BackendNode>>
InspectorDOMAgent::BuildArrayForDistributedNodes(
    InsertionPoint* insertion_point) {
  std::unique_ptr<protocol::Array<protocol::DOM::BackendNode>>
      distributed_nodes = protocol::Array<protocol::DOM::BackendNode>::create();
  for (size_t i = 0; i < insertion_point->DistributedNodesSize(); ++i) {
    Node* distributed_node = insertion_point->DistributedNodeAt(i);
    if (IsWhitespace(distributed_node))
      continue;

    std::unique_ptr<protocol::DOM::BackendNode> backend_node =
        protocol::DOM::BackendNode::create()
            .setNodeType(distributed_node->getNodeType())
            .setNodeName(distributed_node->nodeName())
            .setBackendNodeId(DOMNodeIds::IdForNode(distributed_node))
            .build();
    distributed_nodes->addItem(std::move(backend_node));
  }
  return distributed_nodes;
}

std::unique_ptr<protocol::Array<protocol::DOM::BackendNode>>
InspectorDOMAgent::BuildDistributedNodesForSlot(HTMLSlotElement* slot_element) {
  std::unique_ptr<protocol::Array<protocol::DOM::BackendNode>>
      distributed_nodes = protocol::Array<protocol::DOM::BackendNode>::create();
  for (Node* node = slot_element->FirstDistributedNode(); node;
       node = slot_element->DistributedNodeNextTo(*node)) {
    if (IsWhitespace(node))
      continue;

    std::unique_ptr<protocol::DOM::BackendNode> backend_node =
        protocol::DOM::BackendNode::create()
            .setNodeType(node->getNodeType())
            .setNodeName(node->nodeName())
            .setBackendNodeId(DOMNodeIds::IdForNode(node))
            .build();
    distributed_nodes->addItem(std::move(backend_node));
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
  if (node->IsDocumentNode()) {
    Document* document = ToDocument(node);
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
v8::Local<v8::Value> InspectorDOMAgent::NodeV8Value(
    v8::Local<v8::Context> context,
    Node* node) {
  v8::Isolate* isolate = context->GetIsolate();
  if (!node || !BindingSecurity::ShouldAllowAccessTo(
                   CurrentDOMWindow(isolate), node,
                   BindingSecurity::ErrorReportOption::kDoNotReport))
    return v8::Null(isolate);
  return ToV8(node, context->Global(), isolate);
}

// static
void InspectorDOMAgent::CollectNodes(Node* node,
                                     int depth,
                                     bool pierce,
                                     Function<bool(Node*)>* filter,
                                     HeapVector<Member<Node>>* result) {
  if (filter && filter->operator()(node))
    result->push_back(node);
  if (--depth <= 0)
    return;

  if (pierce && node->IsElementNode()) {
    Element* element = ToElement(node);
    if (node->IsFrameOwnerElement()) {
      HTMLFrameOwnerElement* frame_owner = ToHTMLFrameOwnerElement(node);
      if (frame_owner->ContentFrame() &&
          frame_owner->ContentFrame()->IsLocalFrame()) {
        if (Document* doc = frame_owner->contentDocument())
          CollectNodes(doc, depth, pierce, filter, result);
      }
    }

    ElementShadow* shadow = element->Shadow();
    if (pierce && shadow) {
      for (ShadowRoot* root = &shadow->YoungestShadowRoot(); root;
           root = root->OlderShadowRoot()) {
        CollectNodes(root, depth, pierce, filter, result);
      }
    }

    if (isHTMLLinkElement(*element)) {
      HTMLLinkElement& link_element = toHTMLLinkElement(*element);
      if (link_element.IsImport() && link_element.import() &&
          InnerParentNode(link_element.import()) == link_element) {
        CollectNodes(link_element.import(), depth, pierce, filter, result);
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
  if (Enabled())
    GetFrontend()->documentUpdated();
}

void InspectorDOMAgent::InvalidateFrameOwnerElement(LocalFrame* frame) {
  HTMLFrameOwnerElement* frame_owner = frame->GetDocument()->LocalOwner();
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
    InvalidateFrameOwnerElement(loader->GetFrame());
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
  std::unique_ptr<protocol::Array<int>> node_ids =
      protocol::Array<int>::create();
  for (unsigned i = 0, size = elements.size(); i < size; ++i) {
    Element* element = elements.at(i);
    int id = BoundNodeId(element);
    // If node is not mapped yet -> ignore the event.
    if (!id)
      continue;

    if (dom_listener_)
      dom_listener_->DidModifyDOMAttr(element);
    node_ids->addItem(id);
  }
  GetFrontend()->inlineStyleInvalidated(std::move(node_ids));
}

void InspectorDOMAgent::CharacterDataModified(CharacterData* character_data) {
  int id = document_node_to_id_map_->at(character_data);
  if (!id) {
    // Push text node if it is being created.
    DidInsertDOMNode(character_data);
    return;
  }
  GetFrontend()->characterDataModified(id, character_data->data());
}

InspectorRevalidateDOMTask* InspectorDOMAgent::RevalidateTask() {
  if (!revalidate_task_)
    revalidate_task_ = new InspectorRevalidateDOMTask(this);
  return revalidate_task_.Get();
}

void InspectorDOMAgent::DidInvalidateStyleAttr(Node* node) {
  int id = document_node_to_id_map_->at(node);
  // If node is not mapped yet -> ignore the event.
  if (!id)
    return;

  RevalidateTask()->ScheduleStyleAttrRevalidationFor(ToElement(node));
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

  for (ShadowRoot* root = shadow_host->YoungestShadowRoot(); root;
       root = root->OlderShadowRoot()) {
    const HeapVector<Member<InsertionPoint>>& insertion_points =
        root->DescendantInsertionPoints();
    for (const auto& it : insertion_points) {
      InsertionPoint* insertion_point = it.Get();
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

void InspectorDOMAgent::PseudoElementCreated(PseudoElement* pseudo_element) {
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
  ASSERT(parent);
  int parent_id = document_node_to_id_map_->at(parent);
  ASSERT(parent_id);

  Unbind(pseudo_element, document_node_to_id_map_.Get());
  GetFrontend()->pseudoElementRemoved(parent_id, pseudo_element_id);
}

static ShadowRoot* ShadowRootForNode(Node* node, const String& type) {
  if (!node->IsElementNode())
    return nullptr;
  if (type == "a")
    return ToElement(node)->AuthorShadowRoot();
  if (type == "u")
    return ToElement(node)->UserAgentShadowRoot();
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

  for (size_t i = 0; i < path_tokens.size() - 1; i += 2) {
    bool success = true;
    String& index_value = path_tokens[i];
    unsigned child_number = index_value.ToUInt(&success);
    Node* child;
    if (!success) {
      child = ShadowRootForNode(node, index_value);
    } else {
      if (child_number >= InnerChildNodeCount(node))
        return nullptr;

      child = InnerFirstChild(node);
    }
    String child_name = path_tokens[i + 1];
    for (size_t j = 0; child && j < child_number; ++j)
      child = InnerNextSibling(child);

    if (!child || child->nodeName() != child_name)
      return nullptr;
    node = child;
  }
  return node;
}

Response InspectorDOMAgent::pushNodeByPathToFrontend(const String& path,
                                                     int* node_id) {
  if (Node* node = NodeForPath(path))
    *node_id = PushNodePathToFrontend(node);
  else
    return Response::Error("No node with given path found");
  return Response::OK();
}

Response InspectorDOMAgent::pushNodesByBackendIdsToFrontend(
    std::unique_ptr<protocol::Array<int>> backend_node_ids,
    std::unique_ptr<protocol::Array<int>>* result) {
  *result = protocol::Array<int>::create();
  for (size_t index = 0; index < backend_node_ids->length(); ++index) {
    Node* node = DOMNodeIds::NodeForId(backend_node_ids->get(index));
    if (node && node->GetDocument().GetFrame() &&
        inspected_frames_->Contains(node->GetDocument().GetFrame()))
      (*result)->addItem(PushNodePathToFrontend(node));
    else
      (*result)->addItem(0);
  }
  return Response::OK();
}

class InspectableNode final
    : public v8_inspector::V8InspectorSession::Inspectable {
 public:
  explicit InspectableNode(Node* node)
      : node_id_(DOMNodeIds::IdForNode(node)) {}

  v8::Local<v8::Value> get(v8::Local<v8::Context> context) override {
    return InspectorDOMAgent::NodeV8Value(context,
                                          DOMNodeIds::NodeForId(node_id_));
  }

 private:
  int node_id_;
};

Response InspectorDOMAgent::setInspectedNode(int node_id) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, node);
  if (!response.isSuccess())
    return response;
  v8_session_->addInspectedObject(WTF::MakeUnique<InspectableNode>(node));
  return Response::OK();
}

Response InspectorDOMAgent::getRelayoutBoundary(
    int node_id,
    int* relayout_boundary_node_id) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, node);
  if (!response.isSuccess())
    return response;
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object) {
    return Response::Error(
        "No layout object for node, perhaps orphan or hidden node");
  }
  while (layout_object && !layout_object->IsDocumentElement() &&
         !layout_object->IsRelayoutBoundaryForInspector())
    layout_object = layout_object->Container();
  Node* result_node =
      layout_object ? layout_object->GeneratingNode() : node->ownerDocument();
  *relayout_boundary_node_id = PushNodePathToFrontend(result_node);
  return Response::OK();
}

Response InspectorDOMAgent::getHighlightObjectForTest(
    int node_id,
    std::unique_ptr<protocol::DictionaryValue>* result) {
  Node* node = nullptr;
  Response response = AssertNode(node_id, node);
  if (!response.isSuccess())
    return response;
  InspectorHighlight highlight(node, InspectorHighlight::DefaultConfig(), true);
  *result = highlight.AsProtocolValue();
  return Response::OK();
}

std::unique_ptr<v8_inspector::protocol::Runtime::API::RemoteObject>
InspectorDOMAgent::ResolveNode(Node* node, const String& object_group) {
  Document* document =
      node->IsDocumentNode() ? &node->GetDocument() : node->ownerDocument();
  LocalFrame* frame = document ? document->GetFrame() : nullptr;
  if (!frame)
    return nullptr;

  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  if (!script_state)
    return nullptr;

  ScriptState::Scope scope(script_state);
  return v8_session_->wrapObject(script_state->GetContext(),
                                 NodeV8Value(script_state->GetContext(), node),
                                 ToV8InspectorStringView(object_group));
}

Response InspectorDOMAgent::PushDocumentUponHandlelessOperation() {
  if (!document_node_to_id_map_->Contains(document_)) {
    std::unique_ptr<protocol::DOM::Node> root;
    return getDocument(Maybe<int>(), Maybe<bool>(), &root);
  }
  return Response::OK();
}

DEFINE_TRACE(InspectorDOMAgent) {
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
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink

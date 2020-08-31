// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_dom_snapshot_agent.h"

#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/attribute_collection.h"
#include "third_party/blink/renderer/core/dom/character_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_debugger_agent.h"
#include "third_party/blink/renderer/core/inspector/legacy_dom_snapshot_agent.h"
#include "third_party/blink/renderer/core/inspector/thread_debugger.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_paint_order_iterator.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "v8/include/v8-inspector.h"

namespace blink {
using protocol::Maybe;
using protocol::Response;

namespace {

std::unique_ptr<protocol::Array<double>> BuildRectForPhysicalRect(
    const PhysicalRect& rect) {
  return std::make_unique<std::vector<double>, std::initializer_list<double>>(
      {rect.X(), rect.Y(), rect.Width(), rect.Height()});
}

std::unique_ptr<protocol::Array<double>> BuildRectForLayout(const int x,
                                                            const int y,
                                                            const int width,
                                                            const int height) {
  return std::make_unique<std::vector<double>, std::initializer_list<double>>(
      {x, y, width, height});
}

Document* GetEmbeddedDocument(PaintLayer* layer) {
  // Documents are embedded on their own PaintLayer via a LayoutEmbeddedContent.
  if (layer->GetLayoutObject().IsLayoutEmbeddedContent()) {
    FrameView* frame_view =
        ToLayoutEmbeddedContent(layer->GetLayoutObject()).ChildFrameView();
    if (auto* local_frame_view = DynamicTo<LocalFrameView>(frame_view))
      return local_frame_view->GetFrame().GetDocument();
  }
  return nullptr;
}

std::unique_ptr<protocol::DOMSnapshot::RareStringData> StringData() {
  return protocol::DOMSnapshot::RareStringData::create()
      .setIndex(std::make_unique<protocol::Array<int>>())
      .setValue(std::make_unique<protocol::Array<int>>())
      .build();
}

std::unique_ptr<protocol::DOMSnapshot::RareIntegerData> IntegerData() {
  return protocol::DOMSnapshot::RareIntegerData::create()
      .setIndex(std::make_unique<protocol::Array<int>>())
      .setValue(std::make_unique<protocol::Array<int>>())
      .build();
}

std::unique_ptr<protocol::DOMSnapshot::RareBooleanData> BooleanData() {
  return protocol::DOMSnapshot::RareBooleanData::create()
      .setIndex(std::make_unique<protocol::Array<int>>())
      .build();
}

String GetOriginUrlFast(int max_stack_depth) {
  static const v8::StackTrace::StackTraceOptions stackTraceOptions =
      static_cast<v8::StackTrace::StackTraceOptions>(v8::StackTrace::kDetailed);
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  DCHECK(isolate);

  v8::Local<v8::StackTrace> v8StackTrace = v8::StackTrace::CurrentStackTrace(
      isolate, max_stack_depth, stackTraceOptions);
  if (v8StackTrace.IsEmpty())
    return String();
  for (int i = 0, frame_count = v8StackTrace->GetFrameCount(); i < frame_count;
       ++i) {
    v8::Local<v8::StackFrame> frame = v8StackTrace->GetFrame(isolate, i);
    if (frame.IsEmpty())
      continue;
    v8::Local<v8::String> script_name = frame->GetScriptNameOrSourceURL();
    if (script_name.IsEmpty() || !script_name->Length())
      continue;
    return ToCoreString(script_name);
  }
  return String();
}

String GetOriginUrl(const Node* node) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  ThreadDebugger* debugger = ThreadDebugger::From(isolate);
  if (!isolate || !isolate->InContext() || !debugger)
    return String();
  v8::HandleScope handleScope(isolate);
  // Try not getting the entire stack first.
  String url = GetOriginUrlFast(/* maxStackSize=*/5);
  if (!url.IsEmpty())
    return url;
  url = GetOriginUrlFast(/* maxStackSize=*/200);
  if (!url.IsEmpty())
    return url;
  // If we did not get anything from the sync stack, let's try the slow
  // way that also checks async stacks.
  auto trace = debugger->GetV8Inspector()->captureStackTrace(true);
  if (trace)
    url = ToCoreString(trace->firstNonEmptySourceURL());
  if (!url.IsEmpty())
    return url;
  // Fall back to document url.
  return node->GetDocument().Url().GetString();
}

class DOMTreeIterator {
  STACK_ALLOCATED();

 public:
  DOMTreeIterator(Node* root, int root_node_id)
      : current_(root), path_to_current_node_({root_node_id}) {
    DCHECK(current_);
  }

  void Advance(int next_node_id) {
    DCHECK(current_);
    const bool skip_shadow_root =
        current_->GetShadowRoot() && current_->GetShadowRoot()->IsUserAgent();
    if (Node* first_child = skip_shadow_root
                                ? FirstFlatTreeSibling(current_->firstChild())
                                : FlatTreeTraversal::FirstChild(*current_)) {
      current_ = first_child;
      path_to_current_node_.push_back(next_node_id);
      return;
    }
    // No children, let's try siblings, then ancestor siblings.
    while (current_) {
      const bool in_ua_shadow_tree =
          current_->ParentElementShadowRoot() &&
          current_->ParentElementShadowRoot()->IsUserAgent();
      if (Node* node = in_ua_shadow_tree
                           ? FirstFlatTreeSibling(current_->nextSibling())
                           : FlatTreeTraversal::NextSibling(*current_)) {
        path_to_current_node_.back() = next_node_id;
        current_ = node;
        return;
      }
      current_ = in_ua_shadow_tree ? current_->parentNode()
                                   : FlatTreeTraversal::Parent(*current_);
      path_to_current_node_.pop_back();
    }
    DCHECK(path_to_current_node_.IsEmpty());
  }

  Node* CurrentNode() const { return current_; }

  int ParentNodeId() const {
    return path_to_current_node_.size() > 1
               ? *(path_to_current_node_.rbegin() + 1)
               : -1;
  }

 private:
  static Node* FirstFlatTreeSibling(Node* node) {
    while (node && !node->CanParticipateInFlatTree())
      node = node->nextSibling();
    return node;
  }
  Node* current_;
  WTF::Vector<int> path_to_current_node_;
};

}  // namespace

// Returns |layout_object|'s bounding box in document coordinates.
// static
PhysicalRect InspectorDOMSnapshotAgent::RectInDocument(
    const LayoutObject* layout_object) {
  PhysicalRect rect_in_absolute = PhysicalRect::EnclosingRect(
      layout_object->AbsoluteBoundingBoxFloatRect());
  LocalFrameView* local_frame_view = layout_object->GetFrameView();
  // Don't do frame to document coordinate transformation for layout view,
  // whose bounding box is not affected by scroll offset.
  if (local_frame_view && !IsA<LayoutView>(layout_object))
    return local_frame_view->FrameToDocument(rect_in_absolute);
  return rect_in_absolute;
}

// static
PhysicalRect InspectorDOMSnapshotAgent::TextFragmentRectInDocument(
    const LayoutObject* layout_object,
    const LayoutText::TextBoxInfo& text_box) {
  PhysicalRect local_coords_text_box_rect =
      layout_object->FlipForWritingMode(text_box.local_rect);
  PhysicalRect absolute_coords_text_box_rect =
      layout_object->LocalToAbsoluteRect(local_coords_text_box_rect);
  LocalFrameView* local_frame_view = layout_object->GetFrameView();
  return local_frame_view
             ? local_frame_view->FrameToDocument(absolute_coords_text_box_rect)
             : absolute_coords_text_box_rect;
}

InspectorDOMSnapshotAgent::InspectorDOMSnapshotAgent(
    InspectedFrames* inspected_frames,
    InspectorDOMDebuggerAgent* dom_debugger_agent)
    : inspected_frames_(inspected_frames),
      dom_debugger_agent_(dom_debugger_agent),
      enabled_(&agent_state_, /*default_value=*/false) {}

InspectorDOMSnapshotAgent::~InspectorDOMSnapshotAgent() = default;

void InspectorDOMSnapshotAgent::CharacterDataModified(
    CharacterData* character_data) {
  String origin_url = GetOriginUrl(character_data);
  if (origin_url)
    origin_url_map_->insert(DOMNodeIds::IdForNode(character_data), origin_url);
}

void InspectorDOMSnapshotAgent::DidInsertDOMNode(Node* node) {
  String origin_url = GetOriginUrl(node);
  if (origin_url)
    origin_url_map_->insert(DOMNodeIds::IdForNode(node), origin_url);
}

void InspectorDOMSnapshotAgent::EnableAndReset() {
  enabled_.Set(true);
  origin_url_map_ = std::make_unique<OriginUrlMap>();
  instrumenting_agents_->AddInspectorDOMSnapshotAgent(this);
}

void InspectorDOMSnapshotAgent::Restore() {
  if (enabled_.Get())
    EnableAndReset();
}

Response InspectorDOMSnapshotAgent::enable() {
  if (!enabled_.Get())
    EnableAndReset();
  return Response::Success();
}

Response InspectorDOMSnapshotAgent::disable() {
  if (!enabled_.Get())
    return Response::ServerError("DOM snapshot agent hasn't been enabled.");
  enabled_.Clear();
  origin_url_map_.reset();
  instrumenting_agents_->RemoveInspectorDOMSnapshotAgent(this);
  return Response::Success();
}

Response InspectorDOMSnapshotAgent::getSnapshot(
    std::unique_ptr<protocol::Array<String>> style_filter,
    protocol::Maybe<bool> include_event_listeners,
    protocol::Maybe<bool> include_paint_order,
    protocol::Maybe<bool> include_user_agent_shadow_tree,
    std::unique_ptr<protocol::Array<protocol::DOMSnapshot::DOMNode>>* dom_nodes,
    std::unique_ptr<protocol::Array<protocol::DOMSnapshot::LayoutTreeNode>>*
        layout_tree_nodes,
    std::unique_ptr<protocol::Array<protocol::DOMSnapshot::ComputedStyle>>*
        computed_styles) {
  Document* document = inspected_frames_->Root()->GetDocument();
  if (!document)
    return Response::ServerError("Document is not available");
  LegacyDOMSnapshotAgent legacySupport(dom_debugger_agent_,
                                       origin_url_map_.get());
  return legacySupport.GetSnapshot(
      document, std::move(style_filter), std::move(include_event_listeners),
      std::move(include_paint_order), std::move(include_user_agent_shadow_tree),
      dom_nodes, layout_tree_nodes, computed_styles);
}

protocol::Response InspectorDOMSnapshotAgent::captureSnapshot(
    std::unique_ptr<protocol::Array<String>> computed_styles,
    protocol::Maybe<bool> include_paint_order,
    protocol::Maybe<bool> include_dom_rects,
    std::unique_ptr<protocol::Array<protocol::DOMSnapshot::DocumentSnapshot>>*
        documents,
    std::unique_ptr<protocol::Array<String>>* strings) {
  // This function may kick the layout, but external clients may call this
  // function outside of the layout phase.
  FontCachePurgePreventer fontCachePurgePreventer;

  auto* main_window = inspected_frames_->Root()->DomWindow();
  if (!main_window)
    return Response::ServerError("Document is not available");

  strings_ = std::make_unique<protocol::Array<String>>();
  documents_ = std::make_unique<
      protocol::Array<protocol::DOMSnapshot::DocumentSnapshot>>();

  css_property_filter_ = std::make_unique<CSSPropertyFilter>();
  // Look up the CSSPropertyIDs for each entry in |computed_styles|.
  for (String& entry : *computed_styles) {
    CSSPropertyID property_id = cssPropertyID(main_window, entry);
    if (property_id == CSSPropertyID::kInvalid)
      continue;
    css_property_filter_->emplace_back(std::move(entry),
                                       std::move(property_id));
  }

  if (include_paint_order.fromMaybe(false)) {
    paint_order_map_ =
        InspectorDOMSnapshotAgent::BuildPaintLayerTree(main_window->document());
  }

  include_snapshot_dom_rects_ = include_dom_rects.fromMaybe(false);

  for (LocalFrame* frame : *inspected_frames_) {
    if (Document* document = frame->GetDocument())
      document_order_map_.Set(document, document_order_map_.size());
  }
  for (LocalFrame* frame : *inspected_frames_) {
    if (Document* document = frame->GetDocument())
      VisitDocument(document);
  }

  // Extract results from state and reset.
  *documents = std::move(documents_);
  *strings = std::move(strings_);
  css_property_filter_.reset();
  paint_order_map_.reset();
  string_table_.clear();
  document_order_map_.clear();
  documents_.reset();
  return Response::Success();
}

int InspectorDOMSnapshotAgent::AddString(const String& string) {
  if (string.IsEmpty())
    return -1;
  auto it = string_table_.find(string);
  int index;
  if (it == string_table_.end()) {
    index = static_cast<int>(strings_->size());
    strings_->emplace_back(string);
    string_table_.Set(string, index);
  } else {
    index = it->value;
  }
  return index;
}

void InspectorDOMSnapshotAgent::SetRare(
    protocol::DOMSnapshot::RareIntegerData* data,
    int index,
    int value) {
  data->getIndex()->emplace_back(index);
  data->getValue()->emplace_back(value);
}

void InspectorDOMSnapshotAgent::SetRare(
    protocol::DOMSnapshot::RareStringData* data,
    int index,
    const String& value) {
  data->getIndex()->emplace_back(index);
  data->getValue()->emplace_back(AddString(value));
}

void InspectorDOMSnapshotAgent::SetRare(
    protocol::DOMSnapshot::RareBooleanData* data,
    int index) {
  data->getIndex()->emplace_back(index);
}

void InspectorDOMSnapshotAgent::VisitDocument(Document* document) {
  // Update layout before traversal of document so that we inspect a
  // current and consistent state of all trees. No need to do this if paint
  // order was calculated, since layout trees were already updated during
  // TraversePaintLayerTree().
  if (!paint_order_map_)
    document->UpdateStyleAndLayout(DocumentUpdateReason::kInspector);

  DocumentType* doc_type = document->doctype();

  document_ =
      protocol::DOMSnapshot::DocumentSnapshot::create()
          .setDocumentURL(
              AddString(InspectorDOMAgent::DocumentURLString(document)))
          .setBaseURL(
              AddString(InspectorDOMAgent::DocumentBaseURLString(document)))
          .setTitle(AddString(document->title()))
          .setContentLanguage(AddString(document->ContentLanguage()))
          .setEncodingName(AddString(document->EncodingName()))
          .setFrameId(
              AddString(IdentifiersFactory::FrameId(document->GetFrame())))
          .setPublicId(AddString(doc_type ? doc_type->publicId() : String()))
          .setSystemId(AddString(doc_type ? doc_type->systemId() : String()))
          .setNodes(
              protocol::DOMSnapshot::NodeTreeSnapshot::create()
                  .setParentIndex(std::make_unique<protocol::Array<int>>())
                  .setNodeType(std::make_unique<protocol::Array<int>>())
                  .setNodeName(std::make_unique<protocol::Array<int>>())
                  .setNodeValue(std::make_unique<protocol::Array<int>>())
                  .setBackendNodeId(std::make_unique<protocol::Array<int>>())
                  .setAttributes(
                      std::make_unique<protocol::Array<protocol::Array<int>>>())
                  .setTextValue(StringData())
                  .setInputValue(StringData())
                  .setInputChecked(BooleanData())
                  .setOptionSelected(BooleanData())
                  .setContentDocumentIndex(IntegerData())
                  .setPseudoType(StringData())
                  .setIsClickable(BooleanData())
                  .setCurrentSourceURL(StringData())
                  .setOriginURL(StringData())
                  .build())
          .setLayout(
              protocol::DOMSnapshot::LayoutTreeSnapshot::create()
                  .setNodeIndex(std::make_unique<protocol::Array<int>>())
                  .setBounds(std::make_unique<
                             protocol::Array<protocol::Array<double>>>())
                  .setText(std::make_unique<protocol::Array<int>>())
                  .setStyles(
                      std::make_unique<protocol::Array<protocol::Array<int>>>())
                  .setStackingContexts(BooleanData())
                  .build())
          .setTextBoxes(
              protocol::DOMSnapshot::TextBoxSnapshot::create()
                  .setLayoutIndex(std::make_unique<protocol::Array<int>>())
                  .setBounds(std::make_unique<
                             protocol::Array<protocol::Array<double>>>())
                  .setStart(std::make_unique<protocol::Array<int>>())
                  .setLength(std::make_unique<protocol::Array<int>>())
                  .build())
          .build();

  if (document->View() && document->View()->LayoutViewport()) {
    auto offset = document->View()->LayoutViewport()->GetScrollOffset();
    document_->setScrollOffsetX(offset.Width());
    document_->setScrollOffsetY(offset.Height());
    auto contents_size = document->View()->LayoutViewport()->ContentsSize();
    document_->setContentWidth(contents_size.Width());
    document_->setContentHeight(contents_size.Height());
  }

  if (paint_order_map_) {
    document_->getLayout()->setPaintOrders(
        std::make_unique<protocol::Array<int>>());
  }
  if (include_snapshot_dom_rects_) {
    document_->getLayout()->setOffsetRects(
        std::make_unique<protocol::Array<protocol::Array<double>>>());
    document_->getLayout()->setClientRects(
        std::make_unique<protocol::Array<protocol::Array<double>>>());
    document_->getLayout()->setScrollRects(
        std::make_unique<protocol::Array<protocol::Array<double>>>());
  }

  auto* node_names = document_->getNodes()->getNodeName(nullptr);
  for (DOMTreeIterator it(document, node_names->size()); it.CurrentNode();
       it.Advance(node_names->size())) {
    DCHECK(!it.CurrentNode()->IsInUserAgentShadowRoot());
    VisitNode(it.CurrentNode(), it.ParentNodeId());
  }
  documents_->emplace_back(std::move(document_));
}

void InspectorDOMSnapshotAgent::VisitNode(Node* node, int parent_index) {
  String node_value;
  switch (node->getNodeType()) {
    case Node::kTextNode:
    case Node::kAttributeNode:
    case Node::kCommentNode:
    case Node::kCdataSectionNode:
    case Node::kDocumentFragmentNode:
      node_value = node->nodeValue();
      break;
    default:
      break;
  }

  auto* nodes = document_->getNodes();
  int index = static_cast<int>(nodes->getNodeName(nullptr)->size());
  DOMNodeId backend_node_id = DOMNodeIds::IdForNode(node);

  // Create DOMNode object and add it to the result array before traversing
  // children, so that parents appear before their children in the array.
  nodes->getParentIndex(nullptr)->emplace_back(parent_index);

  nodes->getNodeType(nullptr)->emplace_back(
      static_cast<int>(node->getNodeType()));
  nodes->getNodeName(nullptr)->emplace_back(AddString(node->nodeName()));
  nodes->getNodeValue(nullptr)->emplace_back(AddString(node_value));
  nodes->getBackendNodeId(nullptr)->emplace_back(
      IdentifiersFactory::IntIdForNode(node));
  nodes->getAttributes(nullptr)->emplace_back(
      BuildArrayForElementAttributes(node));
  BuildLayoutTreeNode(node->GetLayoutObject(), node, index);

  if (origin_url_map_ && origin_url_map_->Contains(backend_node_id)) {
    String origin_url = origin_url_map_->at(backend_node_id);
    // In common cases, it is implicit that a child node would have the same
    // origin url as its parent, so no need to mark twice.
    if (!node->parentNode() || origin_url_map_->at(DOMNodeIds::IdForNode(
                                   node->parentNode())) != origin_url) {
      SetRare(nodes->getOriginURL(nullptr), index, origin_url);
    }
  }

  if (node->WillRespondToMouseClickEvents())
    SetRare(nodes->getIsClickable(nullptr), index);

  auto* element = DynamicTo<Element>(node);
  if (element) {
    if (auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(node)) {
      if (Document* doc = frame_owner->contentDocument()) {
        SetRare(nodes->getContentDocumentIndex(nullptr), index,
                document_order_map_.at(doc));
      }
    }

    if (auto* textarea_element = DynamicTo<HTMLTextAreaElement>(*element)) {
      SetRare(nodes->getTextValue(nullptr), index, textarea_element->value());
    }

    if (auto* input_element = DynamicTo<HTMLInputElement>(*element)) {
      SetRare(nodes->getInputValue(nullptr), index, input_element->value());
      if ((input_element->type() == input_type_names::kRadio) ||
          (input_element->type() == input_type_names::kCheckbox)) {
        if (input_element->checked()) {
          SetRare(nodes->getInputChecked(nullptr), index);
        }
      }
    }

    if (auto* option_element = DynamicTo<HTMLOptionElement>(*element)) {
      if (option_element->Selected()) {
        SetRare(nodes->getOptionSelected(nullptr), index);
      }
    }

    if (element->GetPseudoId()) {
      SetRare(
          nodes->getPseudoType(nullptr), index,
          InspectorDOMAgent::ProtocolPseudoElementType(element->GetPseudoId()));
    }
    VisitPseudoElements(element, index);

    auto* image_element = DynamicTo<HTMLImageElement>(node);
    if (image_element) {
      SetRare(nodes->getCurrentSourceURL(nullptr), index,
              image_element->currentSrc());
    }
  }
}

void InspectorDOMSnapshotAgent::VisitPseudoElements(Element* parent,
                                                    int parent_index) {
  for (PseudoId pseudo_id : {kPseudoIdFirstLetter, kPseudoIdBefore,
                             kPseudoIdAfter, kPseudoIdMarker}) {
    if (Node* pseudo_node = parent->GetPseudoElement(pseudo_id))
      VisitNode(pseudo_node, parent_index);
  }
}

std::unique_ptr<protocol::Array<int>>
InspectorDOMSnapshotAgent::BuildArrayForElementAttributes(Node* node) {
  auto result = std::make_unique<protocol::Array<int>>();
  auto* element = DynamicTo<Element>(node);
  if (!element)
    return result;
  AttributeCollection attributes = element->Attributes();
  for (const auto& attribute : attributes) {
    result->emplace_back(AddString(attribute.GetName().ToString()));
    result->emplace_back(AddString(attribute.Value()));
  }
  return result;
}

int InspectorDOMSnapshotAgent::BuildLayoutTreeNode(LayoutObject* layout_object,
                                                   Node* node,
                                                   int node_index) {
  if (!layout_object)
    return -1;

  auto* layout_tree_snapshot = document_->getLayout();
  auto* text_box_snapshot = document_->getTextBoxes();

  int layout_index =
      static_cast<int>(layout_tree_snapshot->getNodeIndex()->size());
  layout_tree_snapshot->getNodeIndex()->emplace_back(node_index);
  layout_tree_snapshot->getStyles()->emplace_back(BuildStylesForNode(node));
  layout_tree_snapshot->getBounds()->emplace_back(BuildRectForPhysicalRect(
      InspectorDOMSnapshotAgent::RectInDocument(layout_object)));

  if (include_snapshot_dom_rects_) {
    protocol::Array<protocol::Array<double>>* offsetRects =
        layout_tree_snapshot->getOffsetRects(nullptr);
    DCHECK(offsetRects);

    protocol::Array<protocol::Array<double>>* clientRects =
        layout_tree_snapshot->getClientRects(nullptr);
    DCHECK(clientRects);

    protocol::Array<protocol::Array<double>>* scrollRects =
        layout_tree_snapshot->getScrollRects(nullptr);
    DCHECK(scrollRects);

    if (auto* element = DynamicTo<Element>(node)) {
      offsetRects->emplace_back(
          BuildRectForLayout(element->OffsetLeft(), element->OffsetTop(),
                             element->OffsetWidth(), element->OffsetHeight()));

      clientRects->emplace_back(
          BuildRectForLayout(element->clientLeft(), element->clientTop(),
                             element->clientWidth(), element->clientHeight()));

      scrollRects->emplace_back(
          BuildRectForLayout(element->scrollLeft(), element->scrollTop(),
                             element->scrollWidth(), element->scrollHeight()));
    } else {
      offsetRects->emplace_back(std::make_unique<protocol::Array<double>>());
      clientRects->emplace_back(std::make_unique<protocol::Array<double>>());
      scrollRects->emplace_back(std::make_unique<protocol::Array<double>>());
    }
  }

  if (layout_object->Style() && layout_object->Style()->IsStackingContext())
    SetRare(layout_tree_snapshot->getStackingContexts(), layout_index);

  if (paint_order_map_) {
    PaintLayer* paint_layer = layout_object->EnclosingLayer();
    DCHECK(paint_order_map_->Contains(paint_layer));
    int paint_order = paint_order_map_->at(paint_layer);
    layout_tree_snapshot->getPaintOrders(nullptr)->emplace_back(paint_order);
  }

  String text = layout_object->IsText() ? ToLayoutText(layout_object)->GetText()
                                        : String();
  layout_tree_snapshot->getText()->emplace_back(AddString(text));

  if (node->GetPseudoId()) {
    // For pseudo elements, visit the children of the layout object.
    // Combinding ::before { content: 'hello' } and ::first-letter would produce
    // two boxes for the ::before node, one for 'hello' and one for 'ello'.
    for (LayoutObject* child = layout_object->SlowFirstChild(); child;
         child = child->NextSibling()) {
      if (child->IsAnonymous())
        BuildLayoutTreeNode(child, node, node_index);
    }
  }

  if (!layout_object->IsText())
    return layout_index;

  LayoutText* layout_text = ToLayoutText(layout_object);
  Vector<LayoutText::TextBoxInfo> text_boxes = layout_text->GetTextBoxInfo();
  if (text_boxes.IsEmpty())
    return layout_index;

  for (const auto& text_box : text_boxes) {
    text_box_snapshot->getLayoutIndex()->emplace_back(layout_index);
    text_box_snapshot->getBounds()->emplace_back(BuildRectForPhysicalRect(
        InspectorDOMSnapshotAgent::TextFragmentRectInDocument(layout_object,
                                                              text_box)));
    text_box_snapshot->getStart()->emplace_back(text_box.dom_start_offset);
    text_box_snapshot->getLength()->emplace_back(text_box.dom_length);
  }

  return layout_index;
}

std::unique_ptr<protocol::Array<int>>
InspectorDOMSnapshotAgent::BuildStylesForNode(Node* node) {
  auto* computed_style_info =
      MakeGarbageCollected<CSSComputedStyleDeclaration>(node, true);
  auto result = std::make_unique<protocol::Array<int>>();
  for (const auto& pair : *css_property_filter_) {
    String value = computed_style_info->GetPropertyValue(pair.second);
    result->emplace_back(AddString(value));
  }
  return result;
}

// static
std::unique_ptr<InspectorDOMSnapshotAgent::PaintOrderMap>
InspectorDOMSnapshotAgent::BuildPaintLayerTree(Document* document) {
  auto result = std::make_unique<PaintOrderMap>();
  TraversePaintLayerTree(document, result.get());
  return result;
}

// static
void InspectorDOMSnapshotAgent::TraversePaintLayerTree(
    Document* document,
    PaintOrderMap* paint_order_map) {
  // Update layout before traversal of document so that we inspect a
  // current and consistent state of all trees.
  document->UpdateStyleAndLayout(DocumentUpdateReason::kInspector);

  PaintLayer* root_layer = document->GetLayoutView()->Layer();
  // LayoutView requires a PaintLayer.
  DCHECK(root_layer);

  VisitPaintLayer(root_layer, paint_order_map);
}

// static
void InspectorDOMSnapshotAgent::VisitPaintLayer(
    PaintLayer* layer,
    PaintOrderMap* paint_order_map) {
  DCHECK(!paint_order_map->Contains(layer));

  paint_order_map->Set(layer, paint_order_map->size());

  Document* embedded_document = GetEmbeddedDocument(layer);
  // If there's an embedded document, there shouldn't be any children.
  DCHECK(!embedded_document || !layer->FirstChild());
  if (embedded_document) {
    TraversePaintLayerTree(embedded_document, paint_order_map);
    return;
  }

  PaintLayerPaintOrderIterator iterator(*layer, kAllChildren);
  while (PaintLayer* child_layer = iterator.Next())
    VisitPaintLayer(child_layer, paint_order_map);
}

void InspectorDOMSnapshotAgent::Trace(Visitor* visitor) {
  visitor->Trace(inspected_frames_);
  visitor->Trace(dom_debugger_agent_);
  visitor->Trace(document_order_map_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorDOMSnapshotAgent.h"

#include "bindings/core/v8/ScriptEventListener.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/dom/Attribute.h"
#include "core/dom/AttributeCollection.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentType.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/QualifiedName.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/HTMLTemplateElement.h"
#include "core/html/forms/HTMLInputElement.h"
#include "core/html/forms/HTMLOptionElement.h"
#include "core/html/forms/HTMLTextAreaElement.h"
#include "core/input_type_names.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorDOMDebuggerAgent.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutText.h"
#include "core/layout/line/InlineTextBox.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

using protocol::Maybe;
using protocol::Response;

namespace {

std::unique_ptr<protocol::DOM::Rect> BuildRectForFloatRect(
    const FloatRect& rect) {
  return protocol::DOM::Rect::create()
      .setX(rect.X())
      .setY(rect.Y())
      .setWidth(rect.Width())
      .setHeight(rect.Height())
      .build();
}

}  // namespace

struct InspectorDOMSnapshotAgent::VectorStringHashTraits
    : public WTF::GenericHashTraits<Vector<String>> {
  static unsigned GetHash(const Vector<String>& vec) {
    unsigned h = DefaultHash<size_t>::Hash::GetHash(vec.size());
    for (size_t i = 0; i < vec.size(); i++) {
      h = WTF::HashInts(h, DefaultHash<String>::Hash::GetHash(vec[i]));
    }
    return h;
  }

  static bool Equal(const Vector<String>& a, const Vector<String>& b) {
    if (a.size() != b.size())
      return false;
    for (size_t i = 0; i < a.size(); i++) {
      if (a[i] != b[i])
        return false;
    }
    return true;
  }

  static void ConstructDeletedValue(Vector<String>& vec, bool) {
    new (NotNull, &vec) Vector<String>(WTF::kHashTableDeletedValue);
  }

  static bool IsDeletedValue(const Vector<String>& vec) {
    return vec.IsHashTableDeletedValue();
  }

  static bool IsEmptyValue(const Vector<String>& vec) { return vec.IsEmpty(); }

  static const bool kEmptyValueIsZero = false;
  static const bool safe_to_compare_to_empty_or_deleted = false;
  static const bool kHasIsEmptyValueFunction = true;
};

InspectorDOMSnapshotAgent::InspectorDOMSnapshotAgent(
    InspectedFrames* inspected_frames,
    InspectorDOMDebuggerAgent* dom_debugger_agent)
    : inspected_frames_(inspected_frames),
      dom_debugger_agent_(dom_debugger_agent) {}

InspectorDOMSnapshotAgent::~InspectorDOMSnapshotAgent() = default;

Response InspectorDOMSnapshotAgent::getSnapshot(
    std::unique_ptr<protocol::Array<String>> style_whitelist,
    protocol::Maybe<bool> get_dom_listeners,
    std::unique_ptr<protocol::Array<protocol::DOMSnapshot::DOMNode>>* dom_nodes,
    std::unique_ptr<protocol::Array<protocol::DOMSnapshot::LayoutTreeNode>>*
        layout_tree_nodes,
    std::unique_ptr<protocol::Array<protocol::DOMSnapshot::ComputedStyle>>*
        computed_styles) {
  DCHECK(!dom_nodes_ && !layout_tree_nodes_ && !computed_styles_);

  Document* document = inspected_frames_->Root()->GetDocument();
  if (!document)
    return Response::Error("Document is not available");

  // Setup snapshot.
  dom_nodes_ = protocol::Array<protocol::DOMSnapshot::DOMNode>::create();
  layout_tree_nodes_ =
      protocol::Array<protocol::DOMSnapshot::LayoutTreeNode>::create();
  computed_styles_ =
      protocol::Array<protocol::DOMSnapshot::ComputedStyle>::create();
  computed_styles_map_ = std::make_unique<ComputedStylesMap>();
  css_property_whitelist_ = std::make_unique<CSSPropertyWhitelist>();

  // Look up the CSSPropertyIDs for each entry in |style_whitelist|.
  for (size_t i = 0; i < style_whitelist->length(); i++) {
    CSSPropertyID property_id = cssPropertyID(style_whitelist->get(i));
    if (property_id == CSSPropertyInvalid)
      continue;
    css_property_whitelist_->push_back(
        std::make_pair(style_whitelist->get(i), property_id));
  }

  // Actual traversal.
  VisitNode(document, get_dom_listeners.fromMaybe(false));

  // Extract results from state and reset.
  *dom_nodes = std::move(dom_nodes_);
  *layout_tree_nodes = std::move(layout_tree_nodes_);
  *computed_styles = std::move(computed_styles_);
  computed_styles_map_.reset();
  css_property_whitelist_.reset();
  return Response::OK();
}

int InspectorDOMSnapshotAgent::VisitNode(Node* node,
                                         bool include_event_listeners) {
  if (node->IsDocumentNode()) {
    // Update layout tree before traversal of document so that we inspect a
    // current and consistent state of all trees.
    node->GetDocument().UpdateStyleAndLayoutTree();
  }

  String node_value;
  switch (node->getNodeType()) {
    case Node::kTextNode:
    case Node::kAttributeNode:
    case Node::kCommentNode:
    case Node::kCdataSectionNode:
      node_value = node->nodeValue();
      break;
    default:
      break;
  }

  // Create DOMNode object and add it to the result array before traversing
  // children, so that parents appear before their children in the array.
  std::unique_ptr<protocol::DOMSnapshot::DOMNode> owned_value =
      protocol::DOMSnapshot::DOMNode::create()
          .setNodeType(static_cast<int>(node->getNodeType()))
          .setNodeName(node->nodeName())
          .setNodeValue(node_value)
          .setBackendNodeId(DOMNodeIds::IdForNode(node))
          .build();
  protocol::DOMSnapshot::DOMNode* value = owned_value.get();
  int index = dom_nodes_->length();
  dom_nodes_->addItem(std::move(owned_value));

  int layoutNodeIndex = VisitLayoutTreeNode(node, index);
  if (layoutNodeIndex != -1)
    value->setLayoutNodeIndex(layoutNodeIndex);

  if (node->WillRespondToMouseClickEvents())
    value->setIsClickable(true);

  if (include_event_listeners && node->GetDocument().GetFrame()) {
    ScriptState* script_state =
        ToScriptStateForMainWorld(node->GetDocument().GetFrame());
    if (script_state->ContextIsValid()) {
      ScriptState::Scope scope(script_state);
      v8::Local<v8::Context> context = script_state->GetContext();
      V8EventListenerInfoList event_information;
      InspectorDOMDebuggerAgent::CollectEventListeners(
          script_state->GetIsolate(), node, v8::Local<v8::Value>(), node, true,
          &event_information);
      if (!event_information.IsEmpty()) {
        value->setEventListeners(
            dom_debugger_agent_->BuildObjectsForEventListeners(
                event_information, context, v8_inspector::StringView()));
      }
    }
  }

  if (node->IsElementNode()) {
    Element* element = ToElement(node);
    value->setAttributes(BuildArrayForElementAttributes(element));

    if (node->IsFrameOwnerElement()) {
      const HTMLFrameOwnerElement* frame_owner = ToHTMLFrameOwnerElement(node);
      if (LocalFrame* frame =
              frame_owner->ContentFrame() &&
                      frame_owner->ContentFrame()->IsLocalFrame()
                  ? ToLocalFrame(frame_owner->ContentFrame())
                  : nullptr) {
        value->setFrameId(IdentifiersFactory::FrameId(frame));
      }
      if (Document* doc = frame_owner->contentDocument()) {
        value->setContentDocumentIndex(VisitNode(doc, include_event_listeners));
      }
    }

    if (node->parentNode() && node->parentNode()->IsDocumentNode()) {
      LocalFrame* frame = node->GetDocument().GetFrame();
      if (frame)
        value->setFrameId(IdentifiersFactory::FrameId(frame));
    }

    if (auto* link_element = ToHTMLLinkElementOrNull(*element)) {
      if (link_element->IsImport() && link_element->import() &&
          InspectorDOMAgent::InnerParentNode(link_element->import()) ==
              link_element) {
        value->setImportedDocumentIndex(
            VisitNode(link_element->import(), include_event_listeners));
      }
    }

    if (auto* template_element = ToHTMLTemplateElementOrNull(*element)) {
      value->setTemplateContentIndex(
          VisitNode(template_element->content(), include_event_listeners));
    }

    if (auto* textarea_element = ToHTMLTextAreaElementOrNull(*element))
      value->setTextValue(textarea_element->value());

    if (auto* input_element = ToHTMLInputElementOrNull(*element)) {
      value->setInputValue(input_element->value());
      if ((input_element->type() == InputTypeNames::radio) ||
          (input_element->type() == InputTypeNames::checkbox)) {
        value->setInputChecked(input_element->checked());
      }
    }

    if (auto* option_element = ToHTMLOptionElementOrNull(*element))
      value->setOptionSelected(option_element->Selected());

    if (element->GetPseudoId()) {
      protocol::DOM::PseudoType pseudo_type;
      if (InspectorDOMAgent::GetPseudoElementType(element->GetPseudoId(),
                                                  &pseudo_type)) {
        value->setPseudoType(pseudo_type);
      }
    } else {
      value->setPseudoElementIndexes(
          VisitPseudoElements(element, include_event_listeners));
    }
  } else if (node->IsDocumentNode()) {
    Document* document = ToDocument(node);
    value->setDocumentURL(InspectorDOMAgent::DocumentURLString(document));
    value->setBaseURL(InspectorDOMAgent::DocumentBaseURLString(document));
    if (document->ContentLanguage())
      value->setContentLanguage(document->ContentLanguage().Utf8().data());
    if (document->EncodingName())
      value->setDocumentEncoding(document->EncodingName().Utf8().data());
    value->setFrameId(IdentifiersFactory::FrameId(document->GetFrame()));
  } else if (node->IsDocumentTypeNode()) {
    DocumentType* doc_type = ToDocumentType(node);
    value->setPublicId(doc_type->publicId());
    value->setSystemId(doc_type->systemId());
  }

  if (node->IsContainerNode()) {
    value->setChildNodeIndexes(
        VisitContainerChildren(node, include_event_listeners));
  }
  return index;
}

std::unique_ptr<protocol::Array<int>>
InspectorDOMSnapshotAgent::VisitContainerChildren(
    Node* container,
    bool include_event_listeners) {
  auto children = protocol::Array<int>::create();

  if (!FlatTreeTraversal::HasChildren(*container))
    return nullptr;

  Node* child = FlatTreeTraversal::FirstChild(*container);
  while (child) {
    children->addItem(VisitNode(child, include_event_listeners));
    child = FlatTreeTraversal::NextSibling(*child);
  }

  return children;
}

std::unique_ptr<protocol::Array<int>>
InspectorDOMSnapshotAgent::VisitPseudoElements(Element* parent,
                                               bool include_event_listeners) {
  if (!parent->GetPseudoElement(kPseudoIdBefore) &&
      !parent->GetPseudoElement(kPseudoIdAfter)) {
    return nullptr;
  }

  auto pseudo_elements = protocol::Array<int>::create();

  if (parent->GetPseudoElement(kPseudoIdBefore)) {
    pseudo_elements->addItem(VisitNode(
        parent->GetPseudoElement(kPseudoIdBefore), include_event_listeners));
  }
  if (parent->GetPseudoElement(kPseudoIdAfter)) {
    pseudo_elements->addItem(VisitNode(parent->GetPseudoElement(kPseudoIdAfter),
                                       include_event_listeners));
  }

  return pseudo_elements;
}

std::unique_ptr<protocol::Array<protocol::DOMSnapshot::NameValue>>
InspectorDOMSnapshotAgent::BuildArrayForElementAttributes(Element* element) {
  auto attributes_value =
      protocol::Array<protocol::DOMSnapshot::NameValue>::create();
  AttributeCollection attributes = element->Attributes();
  for (const auto& attribute : attributes) {
    attributes_value->addItem(protocol::DOMSnapshot::NameValue::create()
                                  .setName(attribute.GetName().ToString())
                                  .setValue(attribute.Value())
                                  .build());
  }
  if (attributes_value->length() == 0)
    return nullptr;
  return attributes_value;
}

int InspectorDOMSnapshotAgent::VisitLayoutTreeNode(Node* node, int node_index) {
  LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return -1;

  auto layout_tree_node = protocol::DOMSnapshot::LayoutTreeNode::create()
                              .setDomNodeIndex(node_index)
                              .setBoundingBox(BuildRectForFloatRect(
                                  layout_object->AbsoluteBoundingBoxRect()))
                              .build();

  int style_index = GetStyleIndexForNode(node);
  if (style_index != -1)
    layout_tree_node->setStyleIndex(style_index);

  if (layout_object->IsText()) {
    LayoutText* layout_text = ToLayoutText(layout_object);
    layout_tree_node->setLayoutText(layout_text->GetText());
    if (layout_text->HasTextBoxes()) {
      std::unique_ptr<protocol::Array<protocol::DOMSnapshot::InlineTextBox>>
          inline_text_nodes =
              protocol::Array<protocol::DOMSnapshot::InlineTextBox>::create();
      for (const InlineTextBox* text_box = layout_text->FirstTextBox();
           text_box; text_box = text_box->NextTextBox()) {
        FloatRect local_coords_text_box_rect(text_box->FrameRect());
        FloatRect absolute_coords_text_box_rect =
            layout_object->LocalToAbsoluteQuad(local_coords_text_box_rect)
                .BoundingBox();
        inline_text_nodes->addItem(
            protocol::DOMSnapshot::InlineTextBox::create()
                .setStartCharacterIndex(text_box->Start())
                .setNumCharacters(text_box->Len())
                .setBoundingBox(
                    BuildRectForFloatRect(absolute_coords_text_box_rect))
                .build());
      }
      layout_tree_node->setInlineTextNodes(std::move(inline_text_nodes));
    }
  }

  int index = layout_tree_nodes_->length();
  layout_tree_nodes_->addItem(std::move(layout_tree_node));
  return index;
}

int InspectorDOMSnapshotAgent::GetStyleIndexForNode(Node* node) {
  CSSComputedStyleDeclaration* computed_style_info =
      CSSComputedStyleDeclaration::Create(node, true);

  Vector<String> style;
  bool all_properties_empty = true;
  for (const auto& pair : *css_property_whitelist_) {
    String value = computed_style_info->GetPropertyValue(pair.second);
    if (!value.IsEmpty())
      all_properties_empty = false;
    style.push_back(value);
  }

  // -1 means an empty style.
  if (all_properties_empty)
    return -1;

  ComputedStylesMap::iterator it = computed_styles_map_->find(style);
  if (it != computed_styles_map_->end())
    return it->value;

  // It's a distinct style, so append to |computedStyles|.
  auto style_properties =
      protocol::Array<protocol::DOMSnapshot::NameValue>::create();

  for (size_t i = 0; i < style.size(); i++) {
    if (style[i].IsEmpty())
      continue;
    style_properties->addItem(protocol::DOMSnapshot::NameValue::create()
                                  .setName((*css_property_whitelist_)[i].first)
                                  .setValue(style[i])
                                  .build());
  }

  size_t index = computed_styles_->length();
  computed_styles_->addItem(protocol::DOMSnapshot::ComputedStyle::create()
                                .setProperties(std::move(style_properties))
                                .build());
  computed_styles_map_->insert(std::move(style), index);
  return index;
}

void InspectorDOMSnapshotAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(inspected_frames_);
  visitor->Trace(dom_debugger_agent_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/accessibility/InspectorTypeBuilderHelper.h"

#include "core/dom/DOMNodeIds.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"

namespace blink {

using namespace HTMLNames;
using namespace protocol::Accessibility;

std::unique_ptr<AXProperty> CreateProperty(const String& name,
                                           std::unique_ptr<AXValue> value) {
  return AXProperty::create().setName(name).setValue(std::move(value)).build();
}

String IgnoredReasonName(AXIgnoredReason reason) {
  switch (reason) {
    case kAXActiveModalDialog:
      return "activeModalDialog";
    case kAXAncestorIsLeafNode:
      return "ancestorIsLeafNode";
    case kAXAriaHiddenElement:
      return "ariaHiddenElement";
    case kAXAriaHiddenSubtree:
      return "ariaHiddenSubtree";
    case kAXEmptyAlt:
      return "emptyAlt";
    case kAXEmptyText:
      return "emptyText";
    case kAXInertElement:
      return "inertElement";
    case kAXInertSubtree:
      return "inertSubtree";
    case kAXInheritsPresentation:
      return "inheritsPresentation";
    case kAXLabelContainer:
      return "labelContainer";
    case kAXLabelFor:
      return "labelFor";
    case kAXNotRendered:
      return "notRendered";
    case kAXNotVisible:
      return "notVisible";
    case kAXPresentationalRole:
      return "presentationalRole";
    case kAXProbablyPresentational:
      return "probablyPresentational";
    case kAXStaticTextUsedAsNameFor:
      return "staticTextUsedAsNameFor";
    case kAXUninteresting:
      return "uninteresting";
  }
  NOTREACHED();
  return "";
}

std::unique_ptr<AXProperty> CreateProperty(IgnoredReason reason) {
  if (reason.related_object)
    return CreateProperty(
        IgnoredReasonName(reason.reason),
        CreateRelatedNodeListValue(*(reason.related_object), nullptr,
                                   AXValueTypeEnum::Idref));
  return CreateProperty(IgnoredReasonName(reason.reason),
                        CreateBooleanValue(true));
}

std::unique_ptr<AXValue> CreateValue(const String& value, const String& type) {
  return AXValue::create()
      .setType(type)
      .setValue(protocol::ValueConversions<String>::toValue(value))
      .build();
}

std::unique_ptr<AXValue> CreateValue(int value, const String& type) {
  return AXValue::create()
      .setType(type)
      .setValue(protocol::ValueConversions<int>::toValue(value))
      .build();
}

std::unique_ptr<AXValue> CreateValue(float value, const String& type) {
  return AXValue::create()
      .setType(type)
      .setValue(protocol::ValueConversions<double>::toValue(value))
      .build();
}

std::unique_ptr<AXValue> CreateBooleanValue(bool value, const String& type) {
  return AXValue::create()
      .setType(type)
      .setValue(protocol::ValueConversions<bool>::toValue(value))
      .build();
}

std::unique_ptr<AXRelatedNode> RelatedNodeForAXObject(const AXObject& ax_object,
                                                      String* name = nullptr) {
  Node* node = ax_object.GetNode();
  if (!node)
    return nullptr;
  int backend_node_id = DOMNodeIds::IdForNode(node);
  if (!backend_node_id)
    return nullptr;
  std::unique_ptr<AXRelatedNode> related_node =
      AXRelatedNode::create().setBackendDOMNodeId(backend_node_id).build();
  if (!node->IsElementNode())
    return related_node;

  Element* element = ToElement(node);
  String idref = element->GetIdAttribute();
  if (!idref.IsEmpty())
    related_node->setIdref(idref);

  if (name)
    related_node->setText(*name);
  return related_node;
}

std::unique_ptr<AXValue> CreateRelatedNodeListValue(const AXObject& ax_object,
                                                    String* name,
                                                    const String& value_type) {
  std::unique_ptr<protocol::Array<AXRelatedNode>> related_nodes =
      protocol::Array<AXRelatedNode>::create();
  related_nodes->addItem(RelatedNodeForAXObject(ax_object, name));
  return AXValue::create()
      .setType(value_type)
      .setRelatedNodes(std::move(related_nodes))
      .build();
}

std::unique_ptr<AXValue> CreateRelatedNodeListValue(
    AXRelatedObjectVector& related_objects,
    const String& value_type) {
  std::unique_ptr<protocol::Array<AXRelatedNode>> frontend_related_nodes =
      protocol::Array<AXRelatedNode>::create();
  for (unsigned i = 0; i < related_objects.size(); i++) {
    std::unique_ptr<AXRelatedNode> frontend_related_node =
        RelatedNodeForAXObject(*(related_objects[i]->object),
                               &(related_objects[i]->text));
    if (frontend_related_node)
      frontend_related_nodes->addItem(std::move(frontend_related_node));
  }
  return AXValue::create()
      .setType(value_type)
      .setRelatedNodes(std::move(frontend_related_nodes))
      .build();
}

std::unique_ptr<AXValue> CreateRelatedNodeListValue(
    AXObject::AXObjectVector& ax_objects,
    const String& value_type) {
  std::unique_ptr<protocol::Array<AXRelatedNode>> related_nodes =
      protocol::Array<AXRelatedNode>::create();
  for (unsigned i = 0; i < ax_objects.size(); i++) {
    std::unique_ptr<AXRelatedNode> related_node =
        RelatedNodeForAXObject(*(ax_objects[i].Get()));
    if (related_node)
      related_nodes->addItem(std::move(related_node));
  }
  return AXValue::create()
      .setType(value_type)
      .setRelatedNodes(std::move(related_nodes))
      .build();
}

String ValueSourceType(AXNameFrom name_from) {
  switch (name_from) {
    case kAXNameFromAttribute:
    case kAXNameFromAttributeExplicitlyEmpty:
    case kAXNameFromTitle:
    case kAXNameFromValue:
      return AXValueSourceTypeEnum::Attribute;
    case kAXNameFromContents:
      return AXValueSourceTypeEnum::Contents;
    case kAXNameFromPlaceholder:
      return AXValueSourceTypeEnum::Placeholder;
    case kAXNameFromCaption:
    case kAXNameFromRelatedElement:
      return AXValueSourceTypeEnum::RelatedElement;
    default:
      return AXValueSourceTypeEnum::Implicit;  // TODO(aboxhall): what to do
                                               // here?
  }
}

String NativeSourceType(AXTextFromNativeHTML native_source) {
  switch (native_source) {
    case kAXTextFromNativeHTMLFigcaption:
      return AXValueNativeSourceTypeEnum::Figcaption;
    case kAXTextFromNativeHTMLLabel:
      return AXValueNativeSourceTypeEnum::Label;
    case kAXTextFromNativeHTMLLabelFor:
      return AXValueNativeSourceTypeEnum::Labelfor;
    case kAXTextFromNativeHTMLLabelWrapped:
      return AXValueNativeSourceTypeEnum::Labelwrapped;
    case kAXTextFromNativeHTMLTableCaption:
      return AXValueNativeSourceTypeEnum::Tablecaption;
    case kAXTextFromNativeHTMLLegend:
      return AXValueNativeSourceTypeEnum::Legend;
    case kAXTextFromNativeHTMLTitleElement:
      return AXValueNativeSourceTypeEnum::Title;
    default:
      return AXValueNativeSourceTypeEnum::Other;
  }
}

std::unique_ptr<AXValueSource> CreateValueSource(NameSource& name_source) {
  String type = ValueSourceType(name_source.type);
  std::unique_ptr<AXValueSource> value_source =
      AXValueSource::create().setType(type).build();
  if (!name_source.related_objects.IsEmpty()) {
    if (name_source.attribute == aria_labelledbyAttr ||
        name_source.attribute == aria_labeledbyAttr) {
      std::unique_ptr<AXValue> attribute_value = CreateRelatedNodeListValue(
          name_source.related_objects, AXValueTypeEnum::IdrefList);
      if (!name_source.attribute_value.IsNull())
        attribute_value->setValue(protocol::StringValue::create(
            name_source.attribute_value.GetString()));
      value_source->setAttributeValue(std::move(attribute_value));
    } else if (name_source.attribute == QualifiedName::Null()) {
      value_source->setNativeSourceValue(CreateRelatedNodeListValue(
          name_source.related_objects, AXValueTypeEnum::NodeList));
    }
  } else if (!name_source.attribute_value.IsNull()) {
    value_source->setAttributeValue(CreateValue(name_source.attribute_value));
  }
  if (!name_source.text.IsNull())
    value_source->setValue(
        CreateValue(name_source.text, AXValueTypeEnum::ComputedString));
  if (name_source.attribute != QualifiedName::Null())
    value_source->setAttribute(name_source.attribute.LocalName().GetString());
  if (name_source.superseded)
    value_source->setSuperseded(true);
  if (name_source.invalid)
    value_source->setInvalid(true);
  if (name_source.native_source != kAXTextFromNativeHTMLUninitialized)
    value_source->setNativeSource(NativeSourceType(name_source.native_source));
  return value_source;
}

}  // namespace blink

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/accessibility/InspectorTypeBuilderHelper.h"

#include "core/InspectorTypeBuilder.h"
#include "core/dom/DOMNodeIds.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"

namespace blink {

using TypeBuilder::Accessibility::AXValueNativeSourceType;
using TypeBuilder::Accessibility::AXRelatedNode;
using TypeBuilder::Accessibility::AXValueSourceType;

PassRefPtr<AXProperty> createProperty(String name, PassRefPtr<AXValue> value)
{
    RefPtr<AXProperty> property = AXProperty::create().setName(name).setValue(value);
    return property;
}

PassRefPtr<AXProperty> createProperty(AXGlobalStates::Enum name, PassRefPtr<AXValue> value)
{
    return createProperty(TypeBuilder::getEnumConstantValue(name), value);
}

PassRefPtr<AXProperty> createProperty(AXLiveRegionAttributes::Enum name, PassRefPtr<AXValue> value)
{
    return createProperty(TypeBuilder::getEnumConstantValue(name), value);
}

PassRefPtr<AXProperty> createProperty(AXRelationshipAttributes::Enum name, PassRefPtr<AXValue> value)
{
    return createProperty(TypeBuilder::getEnumConstantValue(name), value);
}

PassRefPtr<AXProperty> createProperty(AXWidgetAttributes::Enum name, PassRefPtr<AXValue> value)
{
    return createProperty(TypeBuilder::getEnumConstantValue(name), value);
}

PassRefPtr<AXProperty> createProperty(AXWidgetStates::Enum name, PassRefPtr<AXValue> value)
{
    return createProperty(TypeBuilder::getEnumConstantValue(name), value);
}

String ignoredReasonName(AXIgnoredReason reason)
{
    switch (reason) {
    case AXActiveModalDialog:
        return "activeModalDialog";
    case AXAncestorDisallowsChild:
        return "ancestorDisallowsChild";
    case AXAncestorIsLeafNode:
        return "ancestorIsLeafNode";
    case AXAriaHidden:
        return "ariaHidden";
    case AXAriaHiddenRoot:
        return "ariaHiddenRoot";
    case AXEmptyAlt:
        return "emptyAlt";
    case AXEmptyText:
        return "emptyText";
    case AXInert:
        return "inert";
    case AXInheritsPresentation:
        return "inheritsPresentation";
    case AXLabelContainer:
        return "labelContainer";
    case AXLabelFor:
        return "labelFor";
    case AXNotRendered:
        return "notRendered";
    case AXNotVisible:
        return "notVisible";
    case AXPresentationalRole:
        return "presentationalRole";
    case AXProbablyPresentational:
        return "probablyPresentational";
    case AXStaticTextUsedAsNameFor:
        return "staticTextUsedAsNameFor";
    case AXUninteresting:
        return "uninteresting";
    }
    ASSERT_NOT_REACHED();
    return "";
}

PassRefPtr<AXProperty> createProperty(IgnoredReason reason)
{
    if (reason.relatedObject)
        return createProperty(ignoredReasonName(reason.reason), createRelatedNodeListValue(reason.relatedObject, nullptr, AXValueType::Idref));
    return createProperty(ignoredReasonName(reason.reason), createBooleanValue(true));
}

PassRefPtr<AXValue> createValue(String value, AXValueType::Enum type)
{
    RefPtr<AXValue> axValue = AXValue::create().setType(type);
    axValue->setValue(JSONString::create(value));
    return axValue;
}

PassRefPtr<AXValue> createValue(int value, AXValueType::Enum type)
{
    RefPtr<AXValue> axValue = AXValue::create().setType(type);
    axValue->setValue(JSONBasicValue::create(value));
    return axValue;
}

PassRefPtr<AXValue> createValue(float value, AXValueType::Enum type)
{
    RefPtr<AXValue> axValue = AXValue::create().setType(type);
    axValue->setValue(JSONBasicValue::create(value));
    return axValue;
}

PassRefPtr<AXValue> createBooleanValue(bool value, AXValueType::Enum type)
{
    RefPtr<AXValue> axValue = AXValue::create().setType(type);
    axValue->setValue(JSONBasicValue::create(value));
    return axValue;
}

PassRefPtr<AXRelatedNode> relatedNodeForAXObject(const AXObject* axObject, String* name = nullptr)
{
    Node* node = axObject->node();
    if (!node)
        return PassRefPtr<AXRelatedNode>();
    int backendNodeId = DOMNodeIds::idForNode(node);
    if (!backendNodeId)
        return PassRefPtr<AXRelatedNode>();
    RefPtr<AXRelatedNode> relatedNode = AXRelatedNode::create().setBackendNodeId(backendNodeId);
    if (!node->isElementNode())
        return relatedNode;

    Element* element = toElement(node);
    const AtomicString& idref = element->getIdAttribute();
    if (!idref.isEmpty())
        relatedNode->setIdref(idref);

    if (name)
        relatedNode->setText(*name);
    return relatedNode;
}

PassRefPtr<AXValue> createRelatedNodeListValue(const AXObject* axObject, String* name, AXValueType::Enum valueType)
{
    RefPtr<AXValue> axValue = AXValue::create().setType(valueType);
    RefPtr<AXRelatedNode> relatedNode = relatedNodeForAXObject(axObject, name);
    RefPtr<TypeBuilder::Array<AXRelatedNode>> relatedNodes = TypeBuilder::Array<AXRelatedNode>::create();
    relatedNodes->addItem(relatedNode);
    axValue->setRelatedNodes(relatedNodes);
    return axValue;
}

PassRefPtr<AXValue> createRelatedNodeListValue(AXRelatedObjectVector& relatedObjects, AXValueType::Enum valueType)
{
    RefPtr<TypeBuilder::Array<AXRelatedNode>> frontendRelatedNodes = TypeBuilder::Array<AXRelatedNode>::create();
    for (unsigned i = 0; i < relatedObjects.size(); i++) {
        RefPtr<AXRelatedNode> frontendRelatedNode = relatedNodeForAXObject(relatedObjects[i]->object, &(relatedObjects[i]->text));
        if (frontendRelatedNode)
            frontendRelatedNodes->addItem(frontendRelatedNode);
    }
    RefPtr<AXValue> axValue = AXValue::create().setType(valueType);
    axValue->setRelatedNodes(frontendRelatedNodes);
    return axValue;
}

PassRefPtr<AXValue> createRelatedNodeListValue(AXObject::AXObjectVector& axObjects, AXValueType::Enum valueType)
{
    RefPtr<TypeBuilder::Array<AXRelatedNode>> relatedNodes = TypeBuilder::Array<AXRelatedNode>::create();
    for (unsigned i = 0; i < axObjects.size(); i++) {
        RefPtr<AXRelatedNode> relatedNode = relatedNodeForAXObject(axObjects[i].get());
        if (relatedNode)
            relatedNodes->addItem(relatedNode);
    }
    RefPtr<AXValue> axValue = AXValue::create().setType(valueType);
    axValue->setRelatedNodes(relatedNodes);
    return axValue;
}

AXValueSourceType::Enum valueSourceType(AXNameFrom nameFrom)
{
    switch (nameFrom) {
    case AXNameFromAttribute:
    case AXNameFromTitle:
    case AXNameFromValue:
        return AXValueSourceType::Attribute;
    case AXNameFromContents:
        return AXValueSourceType::Contents;
    case AXNameFromPlaceholder:
        return AXValueSourceType::Placeholder;
    case AXNameFromCaption:
    case AXNameFromRelatedElement:
        return AXValueSourceType::RelatedElement;
    default:
        return AXValueSourceType::Implicit; // TODO(aboxhall): what to do here?
    }
}

AXValueNativeSourceType::Enum nativeSourceType(AXTextFromNativeHTML nativeSource)
{
    switch (nativeSource) {
    case AXTextFromNativeHTMLFigcaption:
        return AXValueNativeSourceType::Figcaption;
    case AXTextFromNativeHTMLLabel:
        return AXValueNativeSourceType::Label;
    case AXTextFromNativeHTMLLabelFor:
        return AXValueNativeSourceType::Labelfor;
    case AXTextFromNativeHTMLLabelWrapped:
        return AXValueNativeSourceType::Labelwrapped;
    case AXTextFromNativeHTMLTableCaption:
        return AXValueNativeSourceType::Tablecaption;
    case AXTextFromNativeHTMLLegend:
        return AXValueNativeSourceType::Legend;
    case AXTextFromNativeHTMLTitleElement:
        return AXValueNativeSourceType::Title;
    default:
        return AXValueNativeSourceType::Other;
    }
}


PassRefPtr<AXValueSource> createValueSource(NameSource& nameSource)
{
    String attribute = nameSource.attribute.toString();
    AXValueSourceType::Enum type = valueSourceType(nameSource.type);
    RefPtr<AXValueSource> valueSource = AXValueSource::create().setType(type);
    RefPtr<AXValue> value;
    if (!nameSource.relatedObjects.isEmpty()) {
        AXValueType::Enum valueType = nameSource.attributeValue.isNull() ? AXValueType::NodeList : AXValueType::IdrefList;
        value = createRelatedNodeListValue(nameSource.relatedObjects, valueType);
        if (!nameSource.attributeValue.isNull())
            value->setValue(JSONString::create(nameSource.attributeValue.string()));
        valueSource->setValue(value);
    } else if (!nameSource.text.isNull()) {
        valueSource->setValue(createValue(nameSource.text));
    } else if (!nameSource.attributeValue.isNull()) {
        valueSource->setValue(createValue(nameSource.attributeValue));
    }
    if (nameSource.attribute != QualifiedName::null())
        valueSource->setAttribute(nameSource.attribute.localName().string());
    if (nameSource.superseded)
        valueSource->setSuperseded(true);
    if (nameSource.invalid)
        valueSource->setInvalid(true);
    if (nameSource.nativeSource != AXTextFromNativeHTMLUninitialized)
        valueSource->setNativeSource(nativeSourceType(nameSource.nativeSource));
    return valueSource;
}

} // namespace blink

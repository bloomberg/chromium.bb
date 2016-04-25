// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/accessibility/InspectorTypeBuilderHelper.h"

#include "core/dom/DOMNodeIds.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "platform/inspector_protocol/TypeBuilder.h"

namespace blink {

using namespace HTMLNames;
using namespace protocol::Accessibility;

PassOwnPtr<AXProperty> createProperty(const String& name, PassOwnPtr<AXValue> value)
{
    return AXProperty::create().setName(name).setValue(std::move(value)).build();
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

PassOwnPtr<AXProperty> createProperty(IgnoredReason reason)
{
    if (reason.relatedObject)
        return createProperty(ignoredReasonName(reason.reason), createRelatedNodeListValue(reason.relatedObject, nullptr, AXValueTypeEnum::Idref));
    return createProperty(ignoredReasonName(reason.reason), createBooleanValue(true));
}

PassOwnPtr<AXValue> createValue(const String& value, const String& type)
{
    return AXValue::create().setType(type).setValue(protocol::toValue(value)).build();
}

PassOwnPtr<AXValue> createValue(int value, const String& type)
{
    return AXValue::create().setType(type).setValue(protocol::toValue(value)).build();
}

PassOwnPtr<AXValue> createValue(float value, const String& type)
{
    return AXValue::create().setType(type).setValue(protocol::toValue(value)).build();
}

PassOwnPtr<AXValue> createBooleanValue(bool value, const String& type)
{
    return AXValue::create().setType(type).setValue(protocol::toValue(value)).build();
}

PassOwnPtr<AXRelatedNode> relatedNodeForAXObject(const AXObject* axObject, String* name = nullptr)
{
    Node* node = axObject->getNode();
    if (!node)
        return PassOwnPtr<AXRelatedNode>();
    int backendNodeId = DOMNodeIds::idForNode(node);
    if (!backendNodeId)
        return PassOwnPtr<AXRelatedNode>();
    OwnPtr<AXRelatedNode> relatedNode = AXRelatedNode::create().setBackendNodeId(backendNodeId).build();
    if (!node->isElementNode())
        return relatedNode.release();

    Element* element = toElement(node);
    String idref = element->getIdAttribute();
    if (!idref.isEmpty())
        relatedNode->setIdref(idref);

    if (name)
        relatedNode->setText(*name);
    return relatedNode.release();
}

PassOwnPtr<AXValue> createRelatedNodeListValue(const AXObject* axObject, String* name, const String& valueType)
{
    OwnPtr<protocol::Array<AXRelatedNode>> relatedNodes = protocol::Array<AXRelatedNode>::create();
    relatedNodes->addItem(relatedNodeForAXObject(axObject, name));
    return AXValue::create().setType(valueType).setRelatedNodes(relatedNodes.release()).build();
}

PassOwnPtr<AXValue> createRelatedNodeListValue(AXRelatedObjectVector& relatedObjects, const String& valueType)
{
    OwnPtr<protocol::Array<AXRelatedNode>> frontendRelatedNodes = protocol::Array<AXRelatedNode>::create();
    for (unsigned i = 0; i < relatedObjects.size(); i++) {
        OwnPtr<AXRelatedNode> frontendRelatedNode = relatedNodeForAXObject(relatedObjects[i]->object, &(relatedObjects[i]->text));
        if (frontendRelatedNode)
            frontendRelatedNodes->addItem(frontendRelatedNode.release());
    }
    return AXValue::create().setType(valueType).setRelatedNodes(frontendRelatedNodes.release()).build();
}

PassOwnPtr<AXValue> createRelatedNodeListValue(AXObject::AXObjectVector& axObjects, const String& valueType)
{
    OwnPtr<protocol::Array<AXRelatedNode>> relatedNodes = protocol::Array<AXRelatedNode>::create();
    for (unsigned i = 0; i < axObjects.size(); i++) {
        OwnPtr<AXRelatedNode> relatedNode = relatedNodeForAXObject(axObjects[i].get());
        if (relatedNode)
            relatedNodes->addItem(relatedNode.release());
    }
    return AXValue::create().setType(valueType).setRelatedNodes(relatedNodes.release()).build();
}

String valueSourceType(AXNameFrom nameFrom)
{
    switch (nameFrom) {
    case AXNameFromAttribute:
    case AXNameFromTitle:
    case AXNameFromValue:
        return AXValueSourceTypeEnum::Attribute;
    case AXNameFromContents:
        return AXValueSourceTypeEnum::Contents;
    case AXNameFromPlaceholder:
        return AXValueSourceTypeEnum::Placeholder;
    case AXNameFromCaption:
    case AXNameFromRelatedElement:
        return AXValueSourceTypeEnum::RelatedElement;
    default:
        return AXValueSourceTypeEnum::Implicit; // TODO(aboxhall): what to do here?
    }
}

String nativeSourceType(AXTextFromNativeHTML nativeSource)
{
    switch (nativeSource) {
    case AXTextFromNativeHTMLFigcaption:
        return AXValueNativeSourceTypeEnum::Figcaption;
    case AXTextFromNativeHTMLLabel:
        return AXValueNativeSourceTypeEnum::Label;
    case AXTextFromNativeHTMLLabelFor:
        return AXValueNativeSourceTypeEnum::Labelfor;
    case AXTextFromNativeHTMLLabelWrapped:
        return AXValueNativeSourceTypeEnum::Labelwrapped;
    case AXTextFromNativeHTMLTableCaption:
        return AXValueNativeSourceTypeEnum::Tablecaption;
    case AXTextFromNativeHTMLLegend:
        return AXValueNativeSourceTypeEnum::Legend;
    case AXTextFromNativeHTMLTitleElement:
        return AXValueNativeSourceTypeEnum::Title;
    default:
        return AXValueNativeSourceTypeEnum::Other;
    }
}

PassOwnPtr<AXValueSource> createValueSource(NameSource& nameSource)
{
    String type = valueSourceType(nameSource.type);
    OwnPtr<AXValueSource> valueSource = AXValueSource::create().setType(type).build();
    if (!nameSource.relatedObjects.isEmpty()) {
        if (nameSource.attribute == aria_labelledbyAttr || nameSource.attribute == aria_labeledbyAttr) {
            OwnPtr<AXValue> attributeValue = createRelatedNodeListValue(nameSource.relatedObjects, AXValueTypeEnum::IdrefList);
            if (!nameSource.attributeValue.isNull())
                attributeValue->setValue(protocol::StringValue::create(nameSource.attributeValue.getString()));
            valueSource->setAttributeValue(attributeValue.release());
        } else if (nameSource.attribute == QualifiedName::null()) {
            valueSource->setNativeSourceValue(createRelatedNodeListValue(nameSource.relatedObjects, AXValueTypeEnum::NodeList));
        }
    } else if (!nameSource.attributeValue.isNull()) {
        valueSource->setAttributeValue(createValue(nameSource.attributeValue));
    }
    if (!nameSource.text.isNull())
        valueSource->setValue(createValue(nameSource.text, AXValueTypeEnum::ComputedString));
    if (nameSource.attribute != QualifiedName::null())
        valueSource->setAttribute(nameSource.attribute.localName().getString());
    if (nameSource.superseded)
        valueSource->setSuperseded(true);
    if (nameSource.invalid)
        valueSource->setInvalid(true);
    if (nameSource.nativeSource != AXTextFromNativeHTMLUninitialized)
        valueSource->setNativeSource(nativeSourceType(nameSource.nativeSource));
    return valueSource.release();
}

} // namespace blink

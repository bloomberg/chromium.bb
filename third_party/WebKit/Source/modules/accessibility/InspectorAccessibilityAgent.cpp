// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/accessibility/InspectorAccessibilityAgent.h"

#include "core/HTMLNames.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/Element.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorStyleSheet.h"
#include "core/page/Page.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/accessibility/InspectorTypeBuilderHelper.h"
#include "platform/JSONValues.h"

namespace blink {

using TypeBuilder::Accessibility::AXGlobalStates;
using TypeBuilder::Accessibility::AXLiveRegionAttributes;
using TypeBuilder::Accessibility::AXNode;
using TypeBuilder::Accessibility::AXNodeId;
using TypeBuilder::Accessibility::AXProperty;
using TypeBuilder::Accessibility::AXValueSource;
using TypeBuilder::Accessibility::AXValueType;
using TypeBuilder::Accessibility::AXRelatedNode;
using TypeBuilder::Accessibility::AXRelationshipAttributes;
using TypeBuilder::Accessibility::AXValue;
using TypeBuilder::Accessibility::AXWidgetAttributes;
using TypeBuilder::Accessibility::AXWidgetStates;

using namespace HTMLNames;

namespace {

void fillCoreProperties(AXObject* axObject, PassRefPtr<AXNode> nodeObject)
{
    // Description (secondary to the accessible name).
    AXNameFrom nameFrom;
    AXObject::AXObjectVector nameObjects;
    axObject->name(nameFrom, &nameObjects);
    AXDescriptionFrom descriptionFrom;
    AXObject::AXObjectVector descriptionObjects;
    String description = axObject->description(nameFrom, descriptionFrom, &descriptionObjects);
    if (!description.isEmpty())
        nodeObject->setDescription(createValue(description, AXValueType::ComputedString));

    // Value.
    if (axObject->supportsRangeValue()) {
        nodeObject->setValue(createValue(axObject->valueForRange()));
    } else {
        String stringValue = axObject->stringValue();
        if (!stringValue.isEmpty())
            nodeObject->setValue(createValue(stringValue));
    }
}

void fillLiveRegionProperties(AXObject* axObject, PassRefPtr<TypeBuilder::Array<AXProperty>> properties)
{
    if (!axObject->liveRegionRoot())
        return;

    properties->addItem(createProperty(AXLiveRegionAttributes::Live, createValue(axObject->containerLiveRegionStatus(), AXValueType::Token)));
    properties->addItem(createProperty(AXLiveRegionAttributes::Atomic, createBooleanValue(axObject->containerLiveRegionAtomic())));
    properties->addItem(createProperty(AXLiveRegionAttributes::Relevant, createValue(axObject->containerLiveRegionRelevant(), AXValueType::TokenList)));
    properties->addItem(createProperty(AXLiveRegionAttributes::Busy, createBooleanValue(axObject->containerLiveRegionBusy())));

    if (!axObject->isLiveRegion())
        properties->addItem(createProperty(AXLiveRegionAttributes::Root, createRelatedNodeListValue(axObject->liveRegionRoot())));
}

void fillGlobalStates(AXObject* axObject, PassRefPtr<TypeBuilder::Array<AXProperty>> properties)
{
    if (!axObject->isEnabled())
        properties->addItem(createProperty(AXGlobalStates::Disabled, createBooleanValue(true)));

    if (const AXObject* hiddenRoot = axObject->ariaHiddenRoot()) {
        properties->addItem(createProperty(AXGlobalStates::Hidden, createBooleanValue(true)));
        properties->addItem(createProperty(AXGlobalStates::HiddenRoot, createRelatedNodeListValue(hiddenRoot)));
    }

    InvalidState invalidState = axObject->invalidState();
    switch (invalidState) {
    case InvalidStateUndefined:
        break;
    case InvalidStateFalse:
        properties->addItem(createProperty(AXGlobalStates::Invalid, createValue("false", AXValueType::Token)));
        break;
    case InvalidStateTrue:
        properties->addItem(createProperty(AXGlobalStates::Invalid, createValue("true", AXValueType::Token)));
        break;
    case InvalidStateSpelling:
        properties->addItem(createProperty(AXGlobalStates::Invalid, createValue("spelling", AXValueType::Token)));
        break;
    case InvalidStateGrammar:
        properties->addItem(createProperty(AXGlobalStates::Invalid, createValue("grammar", AXValueType::Token)));
        break;
    default:
        // TODO(aboxhall): expose invalid: <nothing> and source: aria-invalid as invalid value
        properties->addItem(createProperty(AXGlobalStates::Invalid, createValue(axObject->ariaInvalidValue(), AXValueType::String)));
        break;
    }
}

bool roleAllowsMultiselectable(AccessibilityRole role)
{
    return role == GridRole || role == ListBoxRole || role == TabListRole || role == TreeGridRole || role == TreeRole;
}

bool roleAllowsOrientation(AccessibilityRole role)
{
    return role == ScrollBarRole || role == SplitterRole || role == SliderRole;
}

bool roleAllowsReadonly(AccessibilityRole role)
{
    return role == GridRole || role == CellRole || role == TextFieldRole || role == ColumnHeaderRole || role == RowHeaderRole || role == TreeGridRole;
}

bool roleAllowsRequired(AccessibilityRole role)
{
    return role == ComboBoxRole || role == CellRole || role == ListBoxRole || role == RadioGroupRole || role == SpinButtonRole || role == TextFieldRole || role == TreeRole || role == ColumnHeaderRole || role == RowHeaderRole || role == TreeGridRole;
}

bool roleAllowsSort(AccessibilityRole role)
{
    return role == ColumnHeaderRole || role == RowHeaderRole;
}

bool roleAllowsChecked(AccessibilityRole role)
{
    return role == MenuItemCheckBoxRole || role == MenuItemRadioRole || role == RadioButtonRole || role == CheckBoxRole || role == TreeItemRole || role == ListBoxOptionRole || role == SwitchRole;
}

bool roleAllowsSelected(AccessibilityRole role)
{
    return role == CellRole || role == ListBoxOptionRole || role == RowRole || role == TabRole || role == ColumnHeaderRole || role == MenuItemRadioRole || role == RadioButtonRole || role == RowHeaderRole || role == TreeItemRole;
}

void fillWidgetProperties(AXObject* axObject, PassRefPtr<TypeBuilder::Array<AXProperty>> properties)
{
    AccessibilityRole role = axObject->roleValue();
    String autocomplete = axObject->ariaAutoComplete();
    if (!autocomplete.isEmpty())
        properties->addItem(createProperty(AXWidgetAttributes::Autocomplete, createValue(autocomplete, AXValueType::Token)));

    if (axObject->hasAttribute(HTMLNames::aria_haspopupAttr)) {
        bool hasPopup = axObject->ariaHasPopup();
        properties->addItem(createProperty(AXWidgetAttributes::Haspopup, createBooleanValue(hasPopup)));
    }

    int headingLevel = axObject->headingLevel();
    if (headingLevel > 0)
        properties->addItem(createProperty(AXWidgetAttributes::Level, createValue(headingLevel)));
    int hierarchicalLevel = axObject->hierarchicalLevel();
    if (hierarchicalLevel > 0 || axObject->hasAttribute(HTMLNames::aria_levelAttr))
        properties->addItem(createProperty(AXWidgetAttributes::Level, createValue(hierarchicalLevel)));

    if (roleAllowsMultiselectable(role)) {
        bool multiselectable = axObject->isMultiSelectable();
        properties->addItem(createProperty(AXWidgetAttributes::Multiselectable, createBooleanValue(multiselectable)));
    }

    if (roleAllowsOrientation(role)) {
        AccessibilityOrientation orientation = axObject->orientation();
        switch (orientation) {
        case AccessibilityOrientationVertical:
            properties->addItem(createProperty(AXWidgetAttributes::Orientation, createValue("vertical", AXValueType::Token)));
            break;
        case AccessibilityOrientationHorizontal:
            properties->addItem(createProperty(AXWidgetAttributes::Orientation, createValue("horizontal", AXValueType::Token)));
            break;
        case AccessibilityOrientationUndefined:
            break;
        }
    }

    if (role == TextFieldRole)
        properties->addItem(createProperty(AXWidgetAttributes::Multiline, createBooleanValue(axObject->isMultiline())));

    if (roleAllowsReadonly(role)) {
        properties->addItem(createProperty(AXWidgetAttributes::Readonly, createBooleanValue(axObject->isReadOnly())));
    }

    if (roleAllowsRequired(role)) {
        properties->addItem(createProperty(AXWidgetAttributes::Required, createBooleanValue(axObject->isRequired())));
    }

    if (roleAllowsSort(role)) {
        // TODO(aboxhall): sort
    }

    if (axObject->isRange()) {
        properties->addItem(createProperty(AXWidgetAttributes::Valuemin, createValue(axObject->minValueForRange())));
        properties->addItem(createProperty(AXWidgetAttributes::Valuemax, createValue(axObject->maxValueForRange())));
        properties->addItem(createProperty(AXWidgetAttributes::Valuetext, createValue(axObject->valueDescription())));
    }
}

void fillWidgetStates(AXObject* axObject, PassRefPtr<TypeBuilder::Array<AXProperty>> properties)
{
    AccessibilityRole role = axObject->roleValue();
    if (roleAllowsChecked(role)) {
        AccessibilityButtonState checked = axObject->checkboxOrRadioValue();
        switch (checked) {
        case ButtonStateOff:
            properties->addItem(createProperty(AXWidgetStates::Checked, createValue("false", AXValueType::Tristate)));
            break;
        case ButtonStateOn:
            properties->addItem(createProperty(AXWidgetStates::Checked, createValue("true", AXValueType::Tristate)));
            break;
        case ButtonStateMixed:
            properties->addItem(createProperty(AXWidgetStates::Checked, createValue("mixed", AXValueType::Tristate)));
            break;
        }
    }

    AccessibilityExpanded expanded = axObject->isExpanded();
    switch (expanded) {
    case ExpandedUndefined:
        break;
    case ExpandedCollapsed:
        properties->addItem(createProperty(AXWidgetStates::Expanded, createBooleanValue(false, AXValueType::BooleanOrUndefined)));
        break;
    case ExpandedExpanded:
        properties->addItem(createProperty(AXWidgetStates::Expanded, createBooleanValue(true, AXValueType::BooleanOrUndefined)));
        break;
    }

    if (role == ToggleButtonRole) {
        if (!axObject->isPressed()) {
            properties->addItem(createProperty(AXWidgetStates::Pressed, createValue("false", AXValueType::Tristate)));
        } else {
            const AtomicString& pressedAttr = axObject->getAttribute(HTMLNames::aria_pressedAttr);
            if (equalIgnoringCase(pressedAttr, "mixed"))
                properties->addItem(createProperty(AXWidgetStates::Pressed, createValue("mixed", AXValueType::Tristate)));
            else
                properties->addItem(createProperty(AXWidgetStates::Pressed, createValue("true", AXValueType::Tristate)));
        }
    }

    if (roleAllowsSelected(role)) {
        properties->addItem(createProperty(AXWidgetStates::Selected, createBooleanValue(axObject->isSelected())));
    }
}

PassRefPtr<AXProperty> createRelatedNodeListProperty(AXRelationshipAttributes::Enum key, AXRelatedObjectVector& nodes)
{
    RefPtr<AXValue> nodeListValue = createRelatedNodeListValue(nodes, AXValueType::NodeList);
    return createProperty(key, nodeListValue);
}

PassRefPtr<AXProperty> createRelatedNodeListProperty(AXRelationshipAttributes::Enum key, AXObject::AXObjectVector& nodes, const QualifiedName& attr, AXObject* axObject)
{
    RefPtr<AXValue> nodeListValue = createRelatedNodeListValue(nodes);
    const AtomicString& attrValue = axObject->getAttribute(attr);
    nodeListValue->setValue(JSONString::create(attrValue));
    return createProperty(key, nodeListValue);
}

void fillRelationships(AXObject* axObject, PassRefPtr<TypeBuilder::Array<AXProperty>> properties)
{
    if (AXObject* activeDescendant = axObject->activeDescendant()) {
        properties->addItem(createProperty(AXRelationshipAttributes::Activedescendant, createRelatedNodeListValue(activeDescendant)));
    }

    AXObject::AXObjectVector results;
    axObject->ariaFlowToElements(results);
    if (!results.isEmpty())
        properties->addItem(createRelatedNodeListProperty(AXRelationshipAttributes::Flowto, results, aria_flowtoAttr, axObject));
    results.clear();

    axObject->ariaControlsElements(results);
    if (!results.isEmpty())
        properties->addItem(createRelatedNodeListProperty(AXRelationshipAttributes::Controls, results, aria_controlsAttr, axObject));
    results.clear();

    axObject->ariaDescribedbyElements(results);
    if (!results.isEmpty())
        properties->addItem(createRelatedNodeListProperty(AXRelationshipAttributes::Describedby, results, aria_describedbyAttr, axObject));
    results.clear();

    axObject->ariaOwnsElements(results);
    if (!results.isEmpty())
        properties->addItem(createRelatedNodeListProperty(AXRelationshipAttributes::Owns, results, aria_ownsAttr, axObject));
    results.clear();
}

PassRefPtr<AXValue> createRoleNameValue(AccessibilityRole role)
{
    AtomicString roleName = AXObject::roleName(role);
    RefPtr<AXValue> roleNameValue;
    if (!roleName.isNull()) {
        roleNameValue = createValue(roleName, AXValueType::Role);
    } else {
        roleNameValue = createValue(AXObject::internalRoleName(role), AXValueType::InternalRole);
    }
    return roleNameValue;
}

PassRefPtr<AXNode> buildObjectForIgnoredNode(Node* node, AXObject* axObject, AXObjectCacheImpl* cacheImpl)
{
    AXObject::IgnoredReasons ignoredReasons;

    AXID axID = 0;
    RefPtr<AXNode> ignoredNodeObject = AXNode::create().setNodeId(String::number(axID)).setIgnored(true);
    if (axObject) {
        axObject->computeAccessibilityIsIgnored(&ignoredReasons);
        axID = axObject->axObjectID();
        AccessibilityRole role = axObject->roleValue();
        ignoredNodeObject->setRole(createRoleNameValue(role));
    } else if (!node->layoutObject()) {
        ignoredReasons.append(IgnoredReason(AXNotRendered));
    }

    RefPtr<TypeBuilder::Array<AXProperty>> ignoredReasonProperties = TypeBuilder::Array<AXProperty>::create();
    for (size_t i = 0; i < ignoredReasons.size(); i++)
        ignoredReasonProperties->addItem(createProperty(ignoredReasons[i]));
    ignoredNodeObject->setIgnoredReasons(ignoredReasonProperties);

    return ignoredNodeObject;
}

PassRefPtr<AXNode> buildObjectForNode(Node* node, AXObject* axObject, AXObjectCacheImpl* cacheImpl, PassRefPtr<TypeBuilder::Array<AXProperty>> properties)
{
    AccessibilityRole role = axObject->roleValue();
    RefPtr<AXNode> nodeObject = AXNode::create().setNodeId(String::number(axObject->axObjectID())).setIgnored(false);
    nodeObject->setRole(createRoleNameValue(role));

    AXObject::NameSources nameSources;
    String computedName = axObject->name(&nameSources);
    if (!nameSources.isEmpty()) {
        RefPtr<AXValue> name = createValue(computedName, AXValueType::ComputedString);
        if (!nameSources.isEmpty()) {
            RefPtr<TypeBuilder::Array<AXValueSource>> nameSourceProperties = TypeBuilder::Array<AXValueSource>::create();
            for (size_t i = 0; i < nameSources.size(); ++i) {
                NameSource& nameSource = nameSources[i];
                nameSourceProperties->addItem(createValueSource(nameSource));
                if (nameSource.text.isNull() || nameSource.superseded)
                    continue;
                if (!nameSource.relatedObjects.isEmpty()) {
                    properties->addItem(createRelatedNodeListProperty(AXRelationshipAttributes::Labelledby, nameSource.relatedObjects));
                }
            }
            name->setSources(nameSourceProperties);
        }
        nodeObject->setProperties(properties);
        nodeObject->setName(name);
    } else {
        nodeObject->setProperties(properties);
    }

    fillCoreProperties(axObject, nodeObject);
    return nodeObject;
}

} // namespace

InspectorAccessibilityAgent::InspectorAccessibilityAgent(Page* page)
    : InspectorBaseAgent<InspectorAccessibilityAgent, InspectorFrontend::Accessibility>("Accessibility")
    , m_page(page)
{
}

void InspectorAccessibilityAgent::getAXNode(ErrorString* errorString, int nodeId, RefPtr<AXNode>& accessibilityNode)
{
    Frame* mainFrame = m_page->mainFrame();
    if (!mainFrame->isLocalFrame()) {
        *errorString = "Can't inspect out of process frames yet";
        return;
    }

    InspectorDOMAgent* domAgent = toLocalFrame(mainFrame)->instrumentingAgents()->inspectorDOMAgent();
    if (!domAgent) {
        *errorString = "DOM agent must be enabled";
        return;
    }
    Node* node = domAgent->assertNode(errorString, nodeId);
    if (!node)
        return;

    Document& document = node->document();
    OwnPtr<ScopedAXObjectCache> cache = ScopedAXObjectCache::create(document);
    AXObjectCacheImpl* cacheImpl = toAXObjectCacheImpl(cache->get());
    AXObject* axObject = cacheImpl->getOrCreate(node);
    if (!axObject || axObject->accessibilityIsIgnored()) {
        accessibilityNode = buildObjectForIgnoredNode(node, axObject, cacheImpl);
        return;
    }

    RefPtr<TypeBuilder::Array<AXProperty>> properties = TypeBuilder::Array<AXProperty>::create();
    fillLiveRegionProperties(axObject, properties);
    fillGlobalStates(axObject, properties);
    fillWidgetProperties(axObject, properties);
    fillWidgetStates(axObject, properties);
    fillRelationships(axObject, properties);

    accessibilityNode = buildObjectForNode(node, axObject, cacheImpl, properties);
}

DEFINE_TRACE(InspectorAccessibilityAgent)
{
    visitor->trace(m_page);
    InspectorBaseAgent::trace(visitor);
}

} // namespace blink

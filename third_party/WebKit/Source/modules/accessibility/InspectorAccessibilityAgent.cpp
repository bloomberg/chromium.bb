// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "modules/accessibility/InspectorAccessibilityAgent.h"

#include "core/dom/AXObjectCache.h"
#include "core/dom/Element.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorNodeIds.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InspectorStyleSheet.h"
#include "core/page/Page.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"

namespace blink {

namespace {

void fillCoreProperties(AXObject* axObject, PassRefPtr<TypeBuilder::Accessibility::AXNode> nodeObject)
{
    // core properties
    String description = axObject->accessibilityDescription();
    if (!description.isEmpty())
        nodeObject->setDescription(description);

    if (axObject->supportsRangeValue()) {
        nodeObject->setValue(JSONBasicValue::create(axObject->valueForRange()));
    } else {
        String stringValue = axObject->stringValue();
        if (!stringValue.isEmpty())
            nodeObject->setValue(JSONString::create(stringValue));
    }

    String help = axObject->helpText();
    if (!help.isEmpty())
        nodeObject->setHelp(help);
}

void fillLiveRegionProperties(AXObject* axObject, PassRefPtr<TypeBuilder::Accessibility::AXNode> nodeObject)
{
    if (!axObject->liveRegionRoot())
        return;

    RefPtr<JSONObject> liveRegionProperties = JSONObject::create();
    liveRegionProperties->setString("live", axObject->containerLiveRegionStatus());
    liveRegionProperties->setBoolean("atomic", axObject->containerLiveRegionAtomic());
    liveRegionProperties->setString("relevant", axObject->containerLiveRegionRelevant());
    liveRegionProperties->setBoolean("busy", axObject->containerLiveRegionBusy());
    /* TODO(aboxhall): implement
    if (!axObject->isLiveRegion())
        liveRegionProperties->setObject("root", getObjectFor(axObject->liveRegionRoot()));
    */
    nodeObject->setLiveRegionProperties(liveRegionProperties);
}

void fillGlobalStates(AXObject* axObject, PassRefPtr<TypeBuilder::Accessibility::AXNode> nodeObject)
{
    RefPtr<JSONObject> globalStates = JSONObject::create();
    if (!axObject->isEnabled())
        globalStates->setBoolean("disabled", true);

    if (axObject->isInertOrAriaHidden()) {
        globalStates->setBoolean("hidden", true);
        // TODO(aboxhall): show hidden by
    }

    InvalidState invalidState = axObject->invalidState();
    switch (invalidState) {
    case InvalidStateUndefined:
        break;
    case InvalidStateFalse:
        globalStates->setString("invalid", "false");
        break;
    case InvalidStateTrue:
        globalStates->setString("invalid", "true");
        break;
    case InvalidStateSpelling:
        globalStates->setString("invalid", "spelling");
        break;
    case InvalidStateGrammar:
        globalStates->setString("invalid", "grammar");
        break;
    default:
        globalStates->setString("invalid", axObject->ariaInvalidValue());
        break;
    }

    if (globalStates->size())
        nodeObject->setGlobalStates(globalStates);
}

void fillWidgetProperties(AXObject* axObject, PassRefPtr<TypeBuilder::Accessibility::AXNode> nodeObject)
{
    AccessibilityRole role = axObject->roleValue();
    RefPtr<JSONObject> widgetProperties = JSONObject::create();
    String autocomplete = axObject->ariaAutoComplete();
    if (!autocomplete.isEmpty())
        widgetProperties->setString("autocomplete", autocomplete);

    if (axObject->hasAttribute(HTMLNames::aria_haspopupAttr)) {
        bool hasPopup = axObject->ariaHasPopup();
        widgetProperties->setBoolean("hasPopup", hasPopup);
    }

    int headingLevel = axObject->headingLevel();
    if (headingLevel > 0)
        widgetProperties->setNumber("level", headingLevel);
    int hierarchicalLevel = axObject->hierarchicalLevel();
    if (hierarchicalLevel > 0 || axObject->hasAttribute(HTMLNames::aria_levelAttr))
        widgetProperties->setNumber("level", hierarchicalLevel);

    if (role == GridRole || role == ListBoxRole || role == TabListRole || role == TreeGridRole || role == TreeRole) {
        bool multiselectable = axObject->isMultiSelectable();
        widgetProperties->setBoolean("multiselectable", multiselectable);
    }

    if (role == ScrollBarRole || role == SplitterRole || role == SliderRole) {
        AccessibilityOrientation orientation = axObject->orientation();
        switch (orientation) {
        case AccessibilityOrientationVertical:
            widgetProperties->setString("orientation", "vertical");
            break;
        case AccessibilityOrientationHorizontal:
            widgetProperties->setString("orientation", "horizontal");
            break;
        case AccessibilityOrientationUndefined:
            break;
        }
    }

    if (role == TextFieldRole)
        widgetProperties->setBoolean("multiline", false);
    else if (role == TextAreaRole)
        widgetProperties->setBoolean("multiline", true);

    if (role == GridRole || role == CellRole || role == TextAreaRole || role == TextFieldRole || role == ColumnHeaderRole || role == RowHeaderRole || role == TreeGridRole) {
        widgetProperties->setBoolean("readonly", axObject->isReadOnly());
    }

    if (role == ComboBoxRole || role == CellRole || role == ListBoxRole || role == RadioGroupRole || role == SpinButtonRole || role == TextAreaRole || role == TextFieldRole || role == TreeRole || role == ColumnHeaderRole || role == RowHeaderRole || role == TreeGridRole) {
        widgetProperties->setBoolean("required", axObject->isRequired());
    }

    if (role == ColumnHeaderRole || role == RowHeaderRole) {
        // TODO(aboxhall): sort
    }

    if (role == ProgressIndicatorRole || role == ScrollBarRole || role == SliderRole || role == SpinButtonRole) {
        widgetProperties->setNumber("valuemin", axObject->minValueForRange());
        widgetProperties->setNumber("valuemax", axObject->maxValueForRange());
        widgetProperties->setString("valuetext", axObject->valueDescription());
    }

    if (widgetProperties->size())
        nodeObject->setWidgetProperties(widgetProperties);
}

void fillWidgetStates(AXObject* axObject, PassRefPtr<TypeBuilder::Accessibility::AXNode> nodeObject)
{
    RefPtr<JSONObject> widgetStates = JSONObject::create();

    AccessibilityRole role = axObject->roleValue();
    if (role == MenuItemCheckBoxRole || role == MenuItemRadioRole || role == RadioButtonRole || role == CheckBoxRole || role == TreeItemRole || role == ListBoxOptionRole) {
        AccessibilityButtonState checked = axObject->checkboxOrRadioValue();
        switch (checked) {
        case ButtonStateOff:
            widgetStates->setString("checked", "false");
            break;
        case ButtonStateOn:
            widgetStates->setString("checked", "true");
            break;
        case ButtonStateMixed:
            widgetStates->setString("checked", "mixed");
            break;
        }
    }

    AccessibilityExpanded expanded = axObject->isExpanded();
    switch (expanded) {
    case ExpandedUndefined:
        break;
    case ExpandedCollapsed:
        widgetStates->setString("expanded", "collapsed");
        break;
    case ExpandedExpanded:
        widgetStates->setString("expanded", "expanded");
        break;
    }

    if (role == ToggleButtonRole) {
        if (!axObject->isPressed()) {
            widgetStates->setString("pressed", "false");
        } else {
            const AtomicString& pressedAttr = axObject->getAttribute(HTMLNames::aria_pressedAttr);
            if (equalIgnoringCase(pressedAttr, "mixed"))
                widgetStates->setString("pressed", "mixed");
            else
                widgetStates->setString("pressed", "true");
        }
    }

    if (role == CellRole || role == ListBoxOptionRole || role == RowRole || role == TabRole || role == ColumnHeaderRole || role == MenuItemRadioRole || role == RadioButtonRole || role == RowHeaderRole || role == TreeItemRole) {
        widgetStates->setBoolean("selected", axObject->isSelected());
    }

    if (widgetStates->size())
        nodeObject->setWidgetStates(widgetStates);
}

PassRefPtr<TypeBuilder::Accessibility::AXNode> buildObjectForNode(Element* element, AXObject* axObject, AXObjectCacheImpl* cacheImpl)
{
    AccessibilityRole role = axObject->roleValue();
    RefPtr<TypeBuilder::Accessibility::AXNode> nodeObject = TypeBuilder::Accessibility::AXNode::create().setNodeId(String::number(axObject->axObjectID())).setRole(AXObject::roleName(role));
    String computedName = cacheImpl->computedNameForNode(element);
    if (!computedName.isEmpty())
        nodeObject->setName(computedName);
    return nodeObject;
}


} // namespace

InspectorAccessibilityAgent::InspectorAccessibilityAgent(Page* page)
    : InspectorBaseAgent<InspectorAccessibilityAgent, InspectorFrontend::Accessibility>("Accessibility")
    , m_page(page)
{
}

void InspectorAccessibilityAgent::getAXNode(ErrorString* errorString, int nodeId, RefPtr<TypeBuilder::Accessibility::AXNode>& accessibilityNode)
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
    Element* element = domAgent->assertElement(errorString, nodeId);
    if (!element)
        return;
    Document& document = element->document();
    ScopedAXObjectCache cache(document);
    AXObjectCacheImpl* cacheImpl = toAXObjectCacheImpl(cache.get());
    AXObject* axObject = cacheImpl->getOrCreate(element);
    if (!axObject)
        return;

    RefPtr<TypeBuilder::Accessibility::AXNode> nodeObject = buildObjectForNode(element, axObject, cacheImpl);
    fillCoreProperties(axObject, nodeObject);
    fillLiveRegionProperties(axObject, nodeObject);
    fillGlobalStates(axObject, nodeObject);
    fillWidgetProperties(axObject, nodeObject);
    fillWidgetStates(axObject, nodeObject);

    accessibilityNode = nodeObject.release();
}

DEFINE_TRACE(InspectorAccessibilityAgent)
{
    visitor->trace(m_page);
    InspectorBaseAgent::trace(visitor);
}

} // namespace blink

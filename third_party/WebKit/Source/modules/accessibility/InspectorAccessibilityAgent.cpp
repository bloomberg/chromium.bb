// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/accessibility/InspectorAccessibilityAgent.h"

#include "core/HTMLNames.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/NodeList.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorStyleSheet.h"
#include "core/page/Page.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/accessibility/InspectorTypeBuilderHelper.h"
#include <memory>

namespace blink {

using protocol::Accessibility::AXGlobalStates;
using protocol::Accessibility::AXLiveRegionAttributes;
using protocol::Accessibility::AXNode;
using protocol::Accessibility::AXNodeId;
using protocol::Accessibility::AXProperty;
using protocol::Accessibility::AXValueSource;
using protocol::Accessibility::AXValueType;
using protocol::Accessibility::AXRelatedNode;
using protocol::Accessibility::AXRelationshipAttributes;
using protocol::Accessibility::AXValue;
using protocol::Accessibility::AXWidgetAttributes;
using protocol::Accessibility::AXWidgetStates;

using namespace HTMLNames;

namespace {

static const AXID kIDForInspectedNodeWithNoAXNode = 0;

void fillLiveRegionProperties(AXObject& axObject,
                              protocol::Array<AXProperty>& properties) {
  if (!axObject.liveRegionRoot())
    return;

  properties.addItem(
      createProperty(AXLiveRegionAttributesEnum::Live,
                     createValue(axObject.containerLiveRegionStatus(),
                                 AXValueTypeEnum::Token)));
  properties.addItem(
      createProperty(AXLiveRegionAttributesEnum::Atomic,
                     createBooleanValue(axObject.containerLiveRegionAtomic())));
  properties.addItem(
      createProperty(AXLiveRegionAttributesEnum::Relevant,
                     createValue(axObject.containerLiveRegionRelevant(),
                                 AXValueTypeEnum::TokenList)));
  properties.addItem(
      createProperty(AXLiveRegionAttributesEnum::Busy,
                     createBooleanValue(axObject.containerLiveRegionBusy())));

  if (!axObject.isLiveRegion()) {
    properties.addItem(createProperty(
        AXLiveRegionAttributesEnum::Root,
        createRelatedNodeListValue(*(axObject.liveRegionRoot()))));
  }
}

void fillGlobalStates(AXObject& axObject,
                      protocol::Array<AXProperty>& properties) {
  if (!axObject.isEnabled())
    properties.addItem(
        createProperty(AXGlobalStatesEnum::Disabled, createBooleanValue(true)));

  if (const AXObject* hiddenRoot = axObject.ariaHiddenRoot()) {
    properties.addItem(
        createProperty(AXGlobalStatesEnum::Hidden, createBooleanValue(true)));
    properties.addItem(createProperty(AXGlobalStatesEnum::HiddenRoot,
                                      createRelatedNodeListValue(*hiddenRoot)));
  }

  InvalidState invalidState = axObject.getInvalidState();
  switch (invalidState) {
    case InvalidStateUndefined:
      break;
    case InvalidStateFalse:
      properties.addItem(
          createProperty(AXGlobalStatesEnum::Invalid,
                         createValue("false", AXValueTypeEnum::Token)));
      break;
    case InvalidStateTrue:
      properties.addItem(
          createProperty(AXGlobalStatesEnum::Invalid,
                         createValue("true", AXValueTypeEnum::Token)));
      break;
    case InvalidStateSpelling:
      properties.addItem(
          createProperty(AXGlobalStatesEnum::Invalid,
                         createValue("spelling", AXValueTypeEnum::Token)));
      break;
    case InvalidStateGrammar:
      properties.addItem(
          createProperty(AXGlobalStatesEnum::Invalid,
                         createValue("grammar", AXValueTypeEnum::Token)));
      break;
    default:
      // TODO(aboxhall): expose invalid: <nothing> and source: aria-invalid as
      // invalid value
      properties.addItem(createProperty(
          AXGlobalStatesEnum::Invalid,
          createValue(axObject.ariaInvalidValue(), AXValueTypeEnum::String)));
      break;
  }
}

bool roleAllowsModal(AccessibilityRole role) {
  return role == DialogRole || role == AlertDialogRole;
}

bool roleAllowsMultiselectable(AccessibilityRole role) {
  return role == GridRole || role == ListBoxRole || role == TabListRole ||
         role == TreeGridRole || role == TreeRole;
}

bool roleAllowsOrientation(AccessibilityRole role) {
  return role == ScrollBarRole || role == SplitterRole || role == SliderRole;
}

bool roleAllowsReadonly(AccessibilityRole role) {
  return role == GridRole || role == CellRole || role == TextFieldRole ||
         role == ColumnHeaderRole || role == RowHeaderRole ||
         role == TreeGridRole;
}

bool roleAllowsRequired(AccessibilityRole role) {
  return role == ComboBoxRole || role == CellRole || role == ListBoxRole ||
         role == RadioGroupRole || role == SpinButtonRole ||
         role == TextFieldRole || role == TreeRole ||
         role == ColumnHeaderRole || role == RowHeaderRole ||
         role == TreeGridRole;
}

bool roleAllowsSort(AccessibilityRole role) {
  return role == ColumnHeaderRole || role == RowHeaderRole;
}

bool roleAllowsChecked(AccessibilityRole role) {
  return role == MenuItemCheckBoxRole || role == MenuItemRadioRole ||
         role == RadioButtonRole || role == CheckBoxRole ||
         role == TreeItemRole || role == ListBoxOptionRole ||
         role == SwitchRole;
}

bool roleAllowsSelected(AccessibilityRole role) {
  return role == CellRole || role == ListBoxOptionRole || role == RowRole ||
         role == TabRole || role == ColumnHeaderRole ||
         role == MenuItemRadioRole || role == RadioButtonRole ||
         role == RowHeaderRole || role == TreeItemRole;
}

void fillWidgetProperties(AXObject& axObject,
                          protocol::Array<AXProperty>& properties) {
  AccessibilityRole role = axObject.roleValue();
  String autocomplete = axObject.ariaAutoComplete();
  if (!autocomplete.isEmpty())
    properties.addItem(
        createProperty(AXWidgetAttributesEnum::Autocomplete,
                       createValue(autocomplete, AXValueTypeEnum::Token)));

  if (axObject.hasAttribute(HTMLNames::aria_haspopupAttr)) {
    bool hasPopup = axObject.ariaHasPopup();
    properties.addItem(createProperty(AXWidgetAttributesEnum::Haspopup,
                                      createBooleanValue(hasPopup)));
  }

  int headingLevel = axObject.headingLevel();
  if (headingLevel > 0) {
    properties.addItem(createProperty(AXWidgetAttributesEnum::Level,
                                      createValue(headingLevel)));
  }
  int hierarchicalLevel = axObject.hierarchicalLevel();
  if (hierarchicalLevel > 0 ||
      axObject.hasAttribute(HTMLNames::aria_levelAttr)) {
    properties.addItem(createProperty(AXWidgetAttributesEnum::Level,
                                      createValue(hierarchicalLevel)));
  }

  if (roleAllowsMultiselectable(role)) {
    bool multiselectable = axObject.isMultiSelectable();
    properties.addItem(createProperty(AXWidgetAttributesEnum::Multiselectable,
                                      createBooleanValue(multiselectable)));
  }

  if (roleAllowsOrientation(role)) {
    AccessibilityOrientation orientation = axObject.orientation();
    switch (orientation) {
      case AccessibilityOrientationVertical:
        properties.addItem(
            createProperty(AXWidgetAttributesEnum::Orientation,
                           createValue("vertical", AXValueTypeEnum::Token)));
        break;
      case AccessibilityOrientationHorizontal:
        properties.addItem(
            createProperty(AXWidgetAttributesEnum::Orientation,
                           createValue("horizontal", AXValueTypeEnum::Token)));
        break;
      case AccessibilityOrientationUndefined:
        break;
    }
  }

  if (role == TextFieldRole) {
    properties.addItem(
        createProperty(AXWidgetAttributesEnum::Multiline,
                       createBooleanValue(axObject.isMultiline())));
  }

  if (roleAllowsReadonly(role)) {
    properties.addItem(
        createProperty(AXWidgetAttributesEnum::Readonly,
                       createBooleanValue(axObject.isReadOnly())));
  }

  if (roleAllowsRequired(role)) {
    properties.addItem(
        createProperty(AXWidgetAttributesEnum::Required,
                       createBooleanValue(axObject.isRequired())));
  }

  if (roleAllowsSort(role)) {
    // TODO(aboxhall): sort
  }

  if (axObject.isRange()) {
    properties.addItem(
        createProperty(AXWidgetAttributesEnum::Valuemin,
                       createValue(axObject.minValueForRange())));
    properties.addItem(
        createProperty(AXWidgetAttributesEnum::Valuemax,
                       createValue(axObject.maxValueForRange())));
    properties.addItem(
        createProperty(AXWidgetAttributesEnum::Valuetext,
                       createValue(axObject.valueDescription())));
  }
}

void fillWidgetStates(AXObject& axObject,
                      protocol::Array<AXProperty>& properties) {
  AccessibilityRole role = axObject.roleValue();
  if (roleAllowsChecked(role)) {
    AccessibilityButtonState checked = axObject.checkboxOrRadioValue();
    switch (checked) {
      case ButtonStateOff:
        properties.addItem(
            createProperty(AXWidgetStatesEnum::Checked,
                           createValue("false", AXValueTypeEnum::Tristate)));
        break;
      case ButtonStateOn:
        properties.addItem(
            createProperty(AXWidgetStatesEnum::Checked,
                           createValue("true", AXValueTypeEnum::Tristate)));
        break;
      case ButtonStateMixed:
        properties.addItem(
            createProperty(AXWidgetStatesEnum::Checked,
                           createValue("mixed", AXValueTypeEnum::Tristate)));
        break;
    }
  }

  AccessibilityExpanded expanded = axObject.isExpanded();
  switch (expanded) {
    case ExpandedUndefined:
      break;
    case ExpandedCollapsed:
      properties.addItem(createProperty(
          AXWidgetStatesEnum::Expanded,
          createBooleanValue(false, AXValueTypeEnum::BooleanOrUndefined)));
      break;
    case ExpandedExpanded:
      properties.addItem(createProperty(
          AXWidgetStatesEnum::Expanded,
          createBooleanValue(true, AXValueTypeEnum::BooleanOrUndefined)));
      break;
  }

  if (role == ToggleButtonRole) {
    if (!axObject.isPressed()) {
      properties.addItem(
          createProperty(AXWidgetStatesEnum::Pressed,
                         createValue("false", AXValueTypeEnum::Tristate)));
    } else {
      const AtomicString& pressedAttr =
          axObject.getAttribute(HTMLNames::aria_pressedAttr);
      if (equalIgnoringCase(pressedAttr, "mixed"))
        properties.addItem(
            createProperty(AXWidgetStatesEnum::Pressed,
                           createValue("mixed", AXValueTypeEnum::Tristate)));
      else
        properties.addItem(
            createProperty(AXWidgetStatesEnum::Pressed,
                           createValue("true", AXValueTypeEnum::Tristate)));
    }
  }

  if (roleAllowsSelected(role)) {
    properties.addItem(
        createProperty(AXWidgetStatesEnum::Selected,
                       createBooleanValue(axObject.isSelected())));
  }

  if (roleAllowsModal(role)) {
    properties.addItem(createProperty(AXWidgetStatesEnum::Modal,
                                      createBooleanValue(axObject.isModal())));
  }
}

std::unique_ptr<AXProperty> createRelatedNodeListProperty(
    const String& key,
    AXRelatedObjectVector& nodes) {
  std::unique_ptr<AXValue> nodeListValue =
      createRelatedNodeListValue(nodes, AXValueTypeEnum::NodeList);
  return createProperty(key, std::move(nodeListValue));
}

std::unique_ptr<AXProperty> createRelatedNodeListProperty(
    const String& key,
    AXObject::AXObjectVector& nodes,
    const QualifiedName& attr,
    AXObject& axObject) {
  std::unique_ptr<AXValue> nodeListValue = createRelatedNodeListValue(nodes);
  const AtomicString& attrValue = axObject.getAttribute(attr);
  nodeListValue->setValue(protocol::StringValue::create(attrValue));
  return createProperty(key, std::move(nodeListValue));
}

class SparseAttributeAXPropertyAdapter
    : public GarbageCollected<SparseAttributeAXPropertyAdapter>,
      public AXSparseAttributeClient {
 public:
  SparseAttributeAXPropertyAdapter(AXObject& axObject,
                                   protocol::Array<AXProperty>& properties)
      : m_axObject(&axObject), m_properties(properties) {}

  DEFINE_INLINE_TRACE() { visitor->trace(m_axObject); }

 private:
  Member<AXObject> m_axObject;
  protocol::Array<AXProperty>& m_properties;

  void addBoolAttribute(AXBoolAttribute attribute, bool value) {
    // Implement this when we add the first sparse bool attribute.
  }

  void addStringAttribute(AXStringAttribute attribute, const String& value) {
    switch (attribute) {
      case AXStringAttribute::AriaKeyShortcuts:
        m_properties.addItem(
            createProperty(AXGlobalStatesEnum::Keyshortcuts,
                           createValue(value, AXValueTypeEnum::String)));
        break;
      case AXStringAttribute::AriaRoleDescription:
        m_properties.addItem(
            createProperty(AXGlobalStatesEnum::Roledescription,
                           createValue(value, AXValueTypeEnum::String)));
        break;
    }
  }

  void addObjectAttribute(AXObjectAttribute attribute, AXObject& object) {
    switch (attribute) {
      case AXObjectAttribute::AriaActiveDescendant:
        m_properties.addItem(
            createProperty(AXRelationshipAttributesEnum::Activedescendant,
                           createRelatedNodeListValue(object)));
        break;
      case AXObjectAttribute::AriaErrorMessage:
        m_properties.addItem(
            createProperty(AXRelationshipAttributesEnum::Errormessage,
                           createRelatedNodeListValue(object)));
        break;
    }
  }

  void addObjectVectorAttribute(AXObjectVectorAttribute attribute,
                                HeapVector<Member<AXObject>>& objects) {
    switch (attribute) {
      case AXObjectVectorAttribute::AriaControls:
        m_properties.addItem(createRelatedNodeListProperty(
            AXRelationshipAttributesEnum::Controls, objects, aria_controlsAttr,
            *m_axObject));
        break;
      case AXObjectVectorAttribute::AriaDetails:
        m_properties.addItem(createRelatedNodeListProperty(
            AXRelationshipAttributesEnum::Details, objects, aria_controlsAttr,
            *m_axObject));
        break;
      case AXObjectVectorAttribute::AriaFlowTo:
        m_properties.addItem(createRelatedNodeListProperty(
            AXRelationshipAttributesEnum::Flowto, objects, aria_flowtoAttr,
            *m_axObject));
        break;
    }
  }
};

void fillRelationships(AXObject& axObject,
                       protocol::Array<AXProperty>& properties) {
  AXObject::AXObjectVector results;
  axObject.ariaDescribedbyElements(results);
  if (!results.isEmpty())
    properties.addItem(
        createRelatedNodeListProperty(AXRelationshipAttributesEnum::Describedby,
                                      results, aria_describedbyAttr, axObject));
  results.clear();

  axObject.ariaOwnsElements(results);
  if (!results.isEmpty())
    properties.addItem(createRelatedNodeListProperty(
        AXRelationshipAttributesEnum::Owns, results, aria_ownsAttr, axObject));
  results.clear();
}

std::unique_ptr<AXValue> createRoleNameValue(AccessibilityRole role) {
  AtomicString roleName = AXObject::roleName(role);
  std::unique_ptr<AXValue> roleNameValue;
  if (!roleName.isNull()) {
    roleNameValue = createValue(roleName, AXValueTypeEnum::Role);
  } else {
    roleNameValue = createValue(AXObject::internalRoleName(role),
                                AXValueTypeEnum::InternalRole);
  }
  return roleNameValue;
}

}  // namespace

InspectorAccessibilityAgent::InspectorAccessibilityAgent(
    Page* page,
    InspectorDOMAgent* domAgent)
    : m_page(page), m_domAgent(domAgent) {}

Response InspectorAccessibilityAgent::getPartialAXTree(
    int domNodeId,
    Maybe<bool> fetchRelatives,
    std::unique_ptr<protocol::Array<AXNode>>* nodes) {
  if (!m_domAgent->enabled())
    return Response::Error("DOM agent must be enabled");
  Node* domNode = nullptr;
  Response response = m_domAgent->assertNode(domNodeId, domNode);
  if (!response.isSuccess())
    return response;

  Document& document = domNode->document();
  document.updateStyleAndLayoutIgnorePendingStylesheets();
  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      document.lifecycle());
  LocalFrame* localFrame = document.frame();
  if (!localFrame)
    return Response::Error("Frame is detached.");
  std::unique_ptr<ScopedAXObjectCache> scopedCache =
      ScopedAXObjectCache::create(document);
  AXObjectCacheImpl* cache = toAXObjectCacheImpl(scopedCache->get());

  AXObject* inspectedAXObject = cache->getOrCreate(domNode);
  *nodes = protocol::Array<protocol::Accessibility::AXNode>::create();
  if (!inspectedAXObject || inspectedAXObject->accessibilityIsIgnored()) {
    (*nodes)->addItem(buildObjectForIgnoredNode(domNode, inspectedAXObject,
                                                fetchRelatives.fromMaybe(true),
                                                *nodes, *cache));
    return Response::OK();
  } else {
    (*nodes)->addItem(
        buildProtocolAXObject(*inspectedAXObject, inspectedAXObject,
                              fetchRelatives.fromMaybe(true), *nodes, *cache));
  }

  if (!inspectedAXObject)
    return Response::OK();

  AXObject* parent = inspectedAXObject->parentObjectUnignored();
  if (!parent)
    return Response::OK();

  if (fetchRelatives.fromMaybe(true))
    addAncestors(*parent, inspectedAXObject, *nodes, *cache);

  return Response::OK();
}

void InspectorAccessibilityAgent::addAncestors(
    AXObject& firstAncestor,
    AXObject* inspectedAXObject,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  AXObject* ancestor = &firstAncestor;
  while (ancestor) {
    nodes->addItem(buildProtocolAXObject(*ancestor, inspectedAXObject, true,
                                         nodes, cache));
    ancestor = ancestor->parentObjectUnignored();
  }
}

std::unique_ptr<AXNode> InspectorAccessibilityAgent::buildObjectForIgnoredNode(
    Node* domNode,
    AXObject* axObject,
    bool fetchRelatives,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  AXObject::IgnoredReasons ignoredReasons;
  AXID axID = kIDForInspectedNodeWithNoAXNode;
  if (axObject && axObject->isAXLayoutObject())
    axID = axObject->axObjectID();
  std::unique_ptr<AXNode> ignoredNodeObject =
      AXNode::create().setNodeId(String::number(axID)).setIgnored(true).build();
  AccessibilityRole role = AccessibilityRole::IgnoredRole;
  ignoredNodeObject->setRole(createRoleNameValue(role));

  if (axObject && axObject->isAXLayoutObject()) {
    axObject->computeAccessibilityIsIgnored(&ignoredReasons);

    AXObject* parentObject = axObject->parentObjectUnignored();
    if (parentObject && fetchRelatives)
      addAncestors(*parentObject, axObject, nodes, cache);
  } else if (domNode && !domNode->layoutObject()) {
    if (fetchRelatives) {
      populateDOMNodeAncestors(*domNode, *(ignoredNodeObject.get()), nodes,
                               cache);
    }
    ignoredReasons.push_back(IgnoredReason(AXNotRendered));
  }

  if (domNode)
    ignoredNodeObject->setBackendDOMNodeId(DOMNodeIds::idForNode(domNode));

  std::unique_ptr<protocol::Array<AXProperty>> ignoredReasonProperties =
      protocol::Array<AXProperty>::create();
  for (size_t i = 0; i < ignoredReasons.size(); i++)
    ignoredReasonProperties->addItem(createProperty(ignoredReasons[i]));
  ignoredNodeObject->setIgnoredReasons(std::move(ignoredReasonProperties));

  return ignoredNodeObject;
}

void InspectorAccessibilityAgent::populateDOMNodeAncestors(
    Node& inspectedDOMNode,
    AXNode& nodeObject,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  // Walk up parents until an AXObject can be found.
  Node* parentNode = inspectedDOMNode.isShadowRoot()
                         ? &toShadowRoot(inspectedDOMNode).host()
                         : FlatTreeTraversal::parent(inspectedDOMNode);
  AXObject* parentAXObject = cache.getOrCreate(parentNode);
  while (parentNode && !parentAXObject) {
    parentNode = parentNode->isShadowRoot()
                     ? &toShadowRoot(parentNode)->host()
                     : FlatTreeTraversal::parent(*parentNode);
    parentAXObject = cache.getOrCreate(parentNode);
  }

  if (!parentAXObject)
    return;

  if (parentAXObject->accessibilityIsIgnored())
    parentAXObject = parentAXObject->parentObjectUnignored();
  if (!parentAXObject)
    return;

  // Populate parent and ancestors.
  std::unique_ptr<AXNode> parentNodeObject =
      buildProtocolAXObject(*parentAXObject, nullptr, true, nodes, cache);
  std::unique_ptr<protocol::Array<AXNodeId>> childIds =
      protocol::Array<AXNodeId>::create();
  childIds->addItem(String::number(kIDForInspectedNodeWithNoAXNode));
  parentNodeObject->setChildIds(std::move(childIds));
  nodes->addItem(std::move(parentNodeObject));

  AXObject* grandparentAXObject = parentAXObject->parentObjectUnignored();
  if (grandparentAXObject)
    addAncestors(*grandparentAXObject, nullptr, nodes, cache);
}

std::unique_ptr<AXNode> InspectorAccessibilityAgent::buildProtocolAXObject(
    AXObject& axObject,
    AXObject* inspectedAXObject,
    bool fetchRelatives,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  AccessibilityRole role = axObject.roleValue();
  std::unique_ptr<AXNode> nodeObject =
      AXNode::create()
          .setNodeId(String::number(axObject.axObjectID()))
          .setIgnored(false)
          .build();
  nodeObject->setRole(createRoleNameValue(role));

  std::unique_ptr<protocol::Array<AXProperty>> properties =
      protocol::Array<AXProperty>::create();
  fillLiveRegionProperties(axObject, *(properties.get()));
  fillGlobalStates(axObject, *(properties.get()));
  fillWidgetProperties(axObject, *(properties.get()));
  fillWidgetStates(axObject, *(properties.get()));
  fillRelationships(axObject, *(properties.get()));

  SparseAttributeAXPropertyAdapter adapter(axObject, *properties);
  axObject.getSparseAXAttributes(adapter);

  AXObject::NameSources nameSources;
  String computedName = axObject.name(&nameSources);
  if (!nameSources.isEmpty()) {
    std::unique_ptr<AXValue> name =
        createValue(computedName, AXValueTypeEnum::ComputedString);
    if (!nameSources.isEmpty()) {
      std::unique_ptr<protocol::Array<AXValueSource>> nameSourceProperties =
          protocol::Array<AXValueSource>::create();
      for (size_t i = 0; i < nameSources.size(); ++i) {
        NameSource& nameSource = nameSources[i];
        nameSourceProperties->addItem(createValueSource(nameSource));
        if (nameSource.text.isNull() || nameSource.superseded)
          continue;
        if (!nameSource.relatedObjects.isEmpty()) {
          properties->addItem(createRelatedNodeListProperty(
              AXRelationshipAttributesEnum::Labelledby,
              nameSource.relatedObjects));
        }
      }
      name->setSources(std::move(nameSourceProperties));
    }
    nodeObject->setProperties(std::move(properties));
    nodeObject->setName(std::move(name));
  } else {
    nodeObject->setProperties(std::move(properties));
  }

  fillCoreProperties(axObject, inspectedAXObject, fetchRelatives,
                     *(nodeObject.get()), nodes, cache);
  return nodeObject;
}

void InspectorAccessibilityAgent::fillCoreProperties(
    AXObject& axObject,
    AXObject* inspectedAXObject,
    bool fetchRelatives,
    AXNode& nodeObject,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  AXNameFrom nameFrom;
  AXObject::AXObjectVector nameObjects;
  axObject.name(nameFrom, &nameObjects);

  AXDescriptionFrom descriptionFrom;
  AXObject::AXObjectVector descriptionObjects;
  String description =
      axObject.description(nameFrom, descriptionFrom, &descriptionObjects);
  if (!description.isEmpty()) {
    nodeObject.setDescription(
        createValue(description, AXValueTypeEnum::ComputedString));
  }
  // Value.
  if (axObject.supportsRangeValue()) {
    nodeObject.setValue(createValue(axObject.valueForRange()));
  } else {
    String stringValue = axObject.stringValue();
    if (!stringValue.isEmpty())
      nodeObject.setValue(createValue(stringValue));
  }

  if (fetchRelatives)
    populateRelatives(axObject, inspectedAXObject, nodeObject, nodes, cache);

  Node* node = axObject.getNode();
  if (node)
    nodeObject.setBackendDOMNodeId(DOMNodeIds::idForNode(node));
}

void InspectorAccessibilityAgent::populateRelatives(
    AXObject& axObject,
    AXObject* inspectedAXObject,
    AXNode& nodeObject,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  AXObject* parentObject = axObject.parentObject();
  if (parentObject && parentObject != inspectedAXObject) {
    // Use unignored parent unless parent is inspected ignored object.
    parentObject = axObject.parentObjectUnignored();
  }

  std::unique_ptr<protocol::Array<AXNodeId>> childIds =
      protocol::Array<AXNodeId>::create();

  if (&axObject != inspectedAXObject ||
      (inspectedAXObject && !inspectedAXObject->accessibilityIsIgnored())) {
    addChildren(axObject, inspectedAXObject, childIds, nodes, cache);
  }
  nodeObject.setChildIds(std::move(childIds));
}

void InspectorAccessibilityAgent::addChildren(
    AXObject& axObject,
    AXObject* inspectedAXObject,
    std::unique_ptr<protocol::Array<AXNodeId>>& childIds,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  if (inspectedAXObject && inspectedAXObject->accessibilityIsIgnored() &&
      &axObject == inspectedAXObject->parentObjectUnignored()) {
    childIds->addItem(String::number(inspectedAXObject->axObjectID()));
    return;
  }

  const AXObject::AXObjectVector& children = axObject.children();
  for (unsigned i = 0; i < children.size(); i++) {
    AXObject& childAXObject = *children[i].get();
    childIds->addItem(String::number(childAXObject.axObjectID()));
    if (&childAXObject == inspectedAXObject)
      continue;
    if (&axObject != inspectedAXObject &&
        (axObject.getNode() ||
         axObject.parentObjectUnignored() != inspectedAXObject)) {
      continue;
    }

    // Only add children of inspected node (or un-inspectable children of
    // inspected node) to returned nodes.
    std::unique_ptr<AXNode> childNode = buildProtocolAXObject(
        childAXObject, inspectedAXObject, true, nodes, cache);
    nodes->addItem(std::move(childNode));
  }
}

DEFINE_TRACE(InspectorAccessibilityAgent) {
  visitor->trace(m_page);
  visitor->trace(m_domAgent);
  InspectorBaseAgent::trace(visitor);
}

}  // namespace blink

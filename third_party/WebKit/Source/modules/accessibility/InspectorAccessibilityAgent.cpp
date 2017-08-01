// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/accessibility/InspectorAccessibilityAgent.h"

#include <memory>
#include "core/HTMLNames.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/Element.h"
#include "core/dom/ElementShadow.h"
#include "core/dom/Node.h"
#include "core/dom/NodeList.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorStyleSheet.h"
#include "core/page/Page.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/accessibility/InspectorTypeBuilderHelper.h"

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
using protocol::Maybe;
using protocol::Response;

using namespace HTMLNames;

namespace {

static const AXID kIDForInspectedNodeWithNoAXNode = 0;

void FillLiveRegionProperties(AXObject& ax_object,
                              protocol::Array<AXProperty>& properties) {
  if (!ax_object.LiveRegionRoot())
    return;

  properties.addItem(
      CreateProperty(AXLiveRegionAttributesEnum::Live,
                     CreateValue(ax_object.ContainerLiveRegionStatus(),
                                 AXValueTypeEnum::Token)));
  properties.addItem(CreateProperty(
      AXLiveRegionAttributesEnum::Atomic,
      CreateBooleanValue(ax_object.ContainerLiveRegionAtomic())));
  properties.addItem(
      CreateProperty(AXLiveRegionAttributesEnum::Relevant,
                     CreateValue(ax_object.ContainerLiveRegionRelevant(),
                                 AXValueTypeEnum::TokenList)));

  if (!ax_object.IsLiveRegion()) {
    properties.addItem(CreateProperty(
        AXLiveRegionAttributesEnum::Root,
        CreateRelatedNodeListValue(*(ax_object.LiveRegionRoot()))));
  }
}

void FillGlobalStates(AXObject& ax_object,
                      protocol::Array<AXProperty>& properties) {
  if (ax_object.Restriction() == kDisabled)
    properties.addItem(
        CreateProperty(AXGlobalStatesEnum::Disabled, CreateBooleanValue(true)));

  if (const AXObject* hidden_root = ax_object.AriaHiddenRoot()) {
    properties.addItem(
        CreateProperty(AXGlobalStatesEnum::Hidden, CreateBooleanValue(true)));
    properties.addItem(
        CreateProperty(AXGlobalStatesEnum::HiddenRoot,
                       CreateRelatedNodeListValue(*hidden_root)));
  }

  InvalidState invalid_state = ax_object.GetInvalidState();
  switch (invalid_state) {
    case kInvalidStateUndefined:
      break;
    case kInvalidStateFalse:
      properties.addItem(
          CreateProperty(AXGlobalStatesEnum::Invalid,
                         CreateValue("false", AXValueTypeEnum::Token)));
      break;
    case kInvalidStateTrue:
      properties.addItem(
          CreateProperty(AXGlobalStatesEnum::Invalid,
                         CreateValue("true", AXValueTypeEnum::Token)));
      break;
    case kInvalidStateSpelling:
      properties.addItem(
          CreateProperty(AXGlobalStatesEnum::Invalid,
                         CreateValue("spelling", AXValueTypeEnum::Token)));
      break;
    case kInvalidStateGrammar:
      properties.addItem(
          CreateProperty(AXGlobalStatesEnum::Invalid,
                         CreateValue("grammar", AXValueTypeEnum::Token)));
      break;
    default:
      // TODO(aboxhall): expose invalid: <nothing> and source: aria-invalid as
      // invalid value
      properties.addItem(CreateProperty(
          AXGlobalStatesEnum::Invalid,
          CreateValue(ax_object.AriaInvalidValue(), AXValueTypeEnum::String)));
      break;
  }
}

bool RoleAllowsModal(AccessibilityRole role) {
  return role == kDialogRole || role == kAlertDialogRole;
}

bool RoleAllowsMultiselectable(AccessibilityRole role) {
  return role == kGridRole || role == kListBoxRole || role == kTabListRole ||
         role == kTreeGridRole || role == kTreeRole;
}

bool RoleAllowsOrientation(AccessibilityRole role) {
  return role == kScrollBarRole || role == kSplitterRole || role == kSliderRole;
}

bool RoleAllowsReadonly(AccessibilityRole role) {
  return role == kGridRole || role == kCellRole || role == kTextFieldRole ||
         role == kColumnHeaderRole || role == kRowHeaderRole ||
         role == kTreeGridRole;
}

bool RoleAllowsRequired(AccessibilityRole role) {
  return role == kComboBoxRole || role == kCellRole || role == kListBoxRole ||
         role == kRadioGroupRole || role == kSpinButtonRole ||
         role == kTextFieldRole || role == kTreeRole ||
         role == kColumnHeaderRole || role == kRowHeaderRole ||
         role == kTreeGridRole;
}

bool RoleAllowsSort(AccessibilityRole role) {
  return role == kColumnHeaderRole || role == kRowHeaderRole;
}

bool RoleAllowsSelected(AccessibilityRole role) {
  return role == kCellRole || role == kListBoxOptionRole || role == kRowRole ||
         role == kTabRole || role == kColumnHeaderRole ||
         role == kMenuItemRadioRole || role == kRadioButtonRole ||
         role == kRowHeaderRole || role == kTreeItemRole;
}

void FillWidgetProperties(AXObject& ax_object,
                          protocol::Array<AXProperty>& properties) {
  AccessibilityRole role = ax_object.RoleValue();
  String autocomplete = ax_object.AriaAutoComplete();
  if (!autocomplete.IsEmpty())
    properties.addItem(
        CreateProperty(AXWidgetAttributesEnum::Autocomplete,
                       CreateValue(autocomplete, AXValueTypeEnum::Token)));

  bool has_popup = ax_object.AriaHasPopup();
  if (has_popup || ax_object.HasAttribute(HTMLNames::aria_haspopupAttr)) {
    properties.addItem(CreateProperty(AXWidgetAttributesEnum::Haspopup,
                                      CreateBooleanValue(has_popup)));
  }

  int heading_level = ax_object.HeadingLevel();
  if (heading_level > 0) {
    properties.addItem(CreateProperty(AXWidgetAttributesEnum::Level,
                                      CreateValue(heading_level)));
  }
  int hierarchical_level = ax_object.HierarchicalLevel();
  if (hierarchical_level > 0 ||
      ax_object.HasAttribute(HTMLNames::aria_levelAttr)) {
    properties.addItem(CreateProperty(AXWidgetAttributesEnum::Level,
                                      CreateValue(hierarchical_level)));
  }

  if (RoleAllowsMultiselectable(role)) {
    bool multiselectable = ax_object.IsMultiSelectable();
    properties.addItem(CreateProperty(AXWidgetAttributesEnum::Multiselectable,
                                      CreateBooleanValue(multiselectable)));
  }

  if (RoleAllowsOrientation(role)) {
    AccessibilityOrientation orientation = ax_object.Orientation();
    switch (orientation) {
      case kAccessibilityOrientationVertical:
        properties.addItem(
            CreateProperty(AXWidgetAttributesEnum::Orientation,
                           CreateValue("vertical", AXValueTypeEnum::Token)));
        break;
      case kAccessibilityOrientationHorizontal:
        properties.addItem(
            CreateProperty(AXWidgetAttributesEnum::Orientation,
                           CreateValue("horizontal", AXValueTypeEnum::Token)));
        break;
      case kAccessibilityOrientationUndefined:
        break;
    }
  }

  if (role == kTextFieldRole) {
    properties.addItem(
        CreateProperty(AXWidgetAttributesEnum::Multiline,
                       CreateBooleanValue(ax_object.IsMultiline())));
  }

  if (RoleAllowsReadonly(role)) {
    properties.addItem(CreateProperty(
        AXWidgetAttributesEnum::Readonly,
        CreateBooleanValue(ax_object.Restriction() == kReadOnly)));
  }

  if (RoleAllowsRequired(role)) {
    properties.addItem(
        CreateProperty(AXWidgetAttributesEnum::Required,
                       CreateBooleanValue(ax_object.IsRequired())));
  }

  if (RoleAllowsSort(role)) {
    // TODO(aboxhall): sort
  }

  if (ax_object.IsRange()) {
    properties.addItem(
        CreateProperty(AXWidgetAttributesEnum::Valuemin,
                       CreateValue(ax_object.MinValueForRange())));
    properties.addItem(
        CreateProperty(AXWidgetAttributesEnum::Valuemax,
                       CreateValue(ax_object.MaxValueForRange())));
    properties.addItem(
        CreateProperty(AXWidgetAttributesEnum::Valuetext,
                       CreateValue(ax_object.ValueDescription())));
  }
}

void FillWidgetStates(AXObject& ax_object,
                      protocol::Array<AXProperty>& properties) {
  AccessibilityRole role = ax_object.RoleValue();
  const char* checked_prop_val = 0;
  switch (ax_object.CheckedState()) {
    case kCheckedStateTrue:
      checked_prop_val = "true";
      break;
    case kCheckedStateMixed:
      checked_prop_val = "mixed";
      break;
    case kCheckedStateFalse:
      checked_prop_val = "false";
      break;
    case kCheckedStateUndefined:
      break;
  }
  if (checked_prop_val) {
    const auto checked_prop_name = role == kToggleButtonRole
                                       ? AXWidgetStatesEnum::Pressed
                                       : AXWidgetStatesEnum::Checked;
    properties.addItem(CreateProperty(
        checked_prop_name,
        CreateValue(checked_prop_val, AXValueTypeEnum::Tristate)));
  }

  AccessibilityExpanded expanded = ax_object.IsExpanded();
  switch (expanded) {
    case kExpandedUndefined:
      break;
    case kExpandedCollapsed:
      properties.addItem(CreateProperty(
          AXWidgetStatesEnum::Expanded,
          CreateBooleanValue(false, AXValueTypeEnum::BooleanOrUndefined)));
      break;
    case kExpandedExpanded:
      properties.addItem(CreateProperty(
          AXWidgetStatesEnum::Expanded,
          CreateBooleanValue(true, AXValueTypeEnum::BooleanOrUndefined)));
      break;
  }

  if (RoleAllowsSelected(role)) {
    properties.addItem(
        CreateProperty(AXWidgetStatesEnum::Selected,
                       CreateBooleanValue(ax_object.IsSelected())));
  }

  if (RoleAllowsModal(role)) {
    properties.addItem(CreateProperty(AXWidgetStatesEnum::Modal,
                                      CreateBooleanValue(ax_object.IsModal())));
  }
}

std::unique_ptr<AXProperty> CreateRelatedNodeListProperty(
    const String& key,
    AXRelatedObjectVector& nodes) {
  std::unique_ptr<AXValue> node_list_value =
      CreateRelatedNodeListValue(nodes, AXValueTypeEnum::NodeList);
  return CreateProperty(key, std::move(node_list_value));
}

std::unique_ptr<AXProperty> CreateRelatedNodeListProperty(
    const String& key,
    AXObject::AXObjectVector& nodes,
    const QualifiedName& attr,
    AXObject& ax_object) {
  std::unique_ptr<AXValue> node_list_value = CreateRelatedNodeListValue(nodes);
  const AtomicString& attr_value = ax_object.GetAttribute(attr);
  node_list_value->setValue(protocol::StringValue::create(attr_value));
  return CreateProperty(key, std::move(node_list_value));
}

class SparseAttributeAXPropertyAdapter
    : public GarbageCollected<SparseAttributeAXPropertyAdapter>,
      public AXSparseAttributeClient {
 public:
  SparseAttributeAXPropertyAdapter(AXObject& ax_object,
                                   protocol::Array<AXProperty>& properties)
      : ax_object_(&ax_object), properties_(properties) {}

  DEFINE_INLINE_TRACE() { visitor->Trace(ax_object_); }

 private:
  Member<AXObject> ax_object_;
  protocol::Array<AXProperty>& properties_;

  void AddBoolAttribute(AXBoolAttribute attribute, bool value) {
    switch (attribute) {
      case AXBoolAttribute::kAriaBusy:
        properties_.addItem(
            CreateProperty(AXGlobalStatesEnum::Busy,
                           CreateValue(value, AXValueTypeEnum::Boolean)));
        break;
    }
  }

  void AddStringAttribute(AXStringAttribute attribute, const String& value) {
    switch (attribute) {
      case AXStringAttribute::kAriaKeyShortcuts:
        properties_.addItem(
            CreateProperty(AXGlobalStatesEnum::Keyshortcuts,
                           CreateValue(value, AXValueTypeEnum::String)));
        break;
      case AXStringAttribute::kAriaRoleDescription:
        properties_.addItem(
            CreateProperty(AXGlobalStatesEnum::Roledescription,
                           CreateValue(value, AXValueTypeEnum::String)));
        break;
    }
  }

  void AddObjectAttribute(AXObjectAttribute attribute, AXObject& object) {
    switch (attribute) {
      case AXObjectAttribute::kAriaActiveDescendant:
        properties_.addItem(
            CreateProperty(AXRelationshipAttributesEnum::Activedescendant,
                           CreateRelatedNodeListValue(object)));
        break;
      case AXObjectAttribute::kAriaDetails:
        properties_.addItem(
            CreateProperty(AXRelationshipAttributesEnum::Details,
                           CreateRelatedNodeListValue(object)));
        break;
      case AXObjectAttribute::kAriaErrorMessage:
        properties_.addItem(
            CreateProperty(AXRelationshipAttributesEnum::Errormessage,
                           CreateRelatedNodeListValue(object)));
        break;
    }
  }

  void AddObjectVectorAttribute(AXObjectVectorAttribute attribute,
                                HeapVector<Member<AXObject>>& objects) {
    switch (attribute) {
      case AXObjectVectorAttribute::kAriaControls:
        properties_.addItem(CreateRelatedNodeListProperty(
            AXRelationshipAttributesEnum::Controls, objects, aria_controlsAttr,
            *ax_object_));
        break;
      case AXObjectVectorAttribute::kAriaFlowTo:
        properties_.addItem(CreateRelatedNodeListProperty(
            AXRelationshipAttributesEnum::Flowto, objects, aria_flowtoAttr,
            *ax_object_));
        break;
    }
  }
};

void FillRelationships(AXObject& ax_object,
                       protocol::Array<AXProperty>& properties) {
  AXObject::AXObjectVector results;
  ax_object.AriaDescribedbyElements(results);
  if (!results.IsEmpty())
    properties.addItem(CreateRelatedNodeListProperty(
        AXRelationshipAttributesEnum::Describedby, results,
        aria_describedbyAttr, ax_object));
  results.clear();

  ax_object.AriaOwnsElements(results);
  if (!results.IsEmpty())
    properties.addItem(CreateRelatedNodeListProperty(
        AXRelationshipAttributesEnum::Owns, results, aria_ownsAttr, ax_object));
  results.clear();
}

std::unique_ptr<AXValue> CreateRoleNameValue(AccessibilityRole role) {
  AtomicString role_name = AXObject::RoleName(role);
  std::unique_ptr<AXValue> role_name_value;
  if (!role_name.IsNull()) {
    role_name_value = CreateValue(role_name, AXValueTypeEnum::Role);
  } else {
    role_name_value = CreateValue(AXObject::InternalRoleName(role),
                                  AXValueTypeEnum::InternalRole);
  }
  return role_name_value;
}

}  // namespace

InspectorAccessibilityAgent::InspectorAccessibilityAgent(
    Page* page,
    InspectorDOMAgent* dom_agent)
    : page_(page), dom_agent_(dom_agent) {}

Response InspectorAccessibilityAgent::getPartialAXTree(
    int dom_node_id,
    Maybe<bool> fetch_relatives,
    std::unique_ptr<protocol::Array<AXNode>>* nodes) {
  if (!dom_agent_->Enabled())
    return Response::Error("DOM agent must be enabled");
  Node* dom_node = nullptr;
  Response response = dom_agent_->AssertNode(dom_node_id, dom_node);
  if (!response.isSuccess())
    return response;

  Document& document = dom_node->GetDocument();
  document.UpdateStyleAndLayoutIgnorePendingStylesheets();
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      document.Lifecycle());
  LocalFrame* local_frame = document.GetFrame();
  if (!local_frame)
    return Response::Error("Frame is detached.");
  std::unique_ptr<ScopedAXObjectCache> scoped_cache =
      ScopedAXObjectCache::Create(document);
  AXObjectCacheImpl* cache = ToAXObjectCacheImpl(scoped_cache->Get());

  AXObject* inspected_ax_object = cache->GetOrCreate(dom_node);
  *nodes = protocol::Array<protocol::Accessibility::AXNode>::create();
  if (!inspected_ax_object || inspected_ax_object->AccessibilityIsIgnored()) {
    (*nodes)->addItem(BuildObjectForIgnoredNode(dom_node, inspected_ax_object,
                                                fetch_relatives.fromMaybe(true),
                                                *nodes, *cache));
    return Response::OK();
  } else {
    (*nodes)->addItem(
        BuildProtocolAXObject(*inspected_ax_object, inspected_ax_object,
                              fetch_relatives.fromMaybe(true), *nodes, *cache));
  }

  if (!inspected_ax_object)
    return Response::OK();

  AXObject* parent = inspected_ax_object->ParentObjectUnignored();
  if (!parent)
    return Response::OK();

  if (fetch_relatives.fromMaybe(true))
    AddAncestors(*parent, inspected_ax_object, *nodes, *cache);

  return Response::OK();
}

void InspectorAccessibilityAgent::AddAncestors(
    AXObject& first_ancestor,
    AXObject* inspected_ax_object,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  AXObject* ancestor = &first_ancestor;
  while (ancestor) {
    nodes->addItem(BuildProtocolAXObject(*ancestor, inspected_ax_object, true,
                                         nodes, cache));
    ancestor = ancestor->ParentObjectUnignored();
  }
}

std::unique_ptr<AXNode> InspectorAccessibilityAgent::BuildObjectForIgnoredNode(
    Node* dom_node,
    AXObject* ax_object,
    bool fetch_relatives,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  AXObject::IgnoredReasons ignored_reasons;
  AXID ax_id = kIDForInspectedNodeWithNoAXNode;
  if (ax_object && ax_object->IsAXLayoutObject())
    ax_id = ax_object->AxObjectID();
  std::unique_ptr<AXNode> ignored_node_object =
      AXNode::create()
          .setNodeId(String::Number(ax_id))
          .setIgnored(true)
          .build();
  AccessibilityRole role = AccessibilityRole::kIgnoredRole;
  ignored_node_object->setRole(CreateRoleNameValue(role));

  if (ax_object && ax_object->IsAXLayoutObject()) {
    ax_object->ComputeAccessibilityIsIgnored(&ignored_reasons);

    AXObject* parent_object = ax_object->ParentObjectUnignored();
    if (parent_object && fetch_relatives)
      AddAncestors(*parent_object, ax_object, nodes, cache);
  } else if (dom_node && !dom_node->GetLayoutObject()) {
    if (fetch_relatives) {
      PopulateDOMNodeAncestors(*dom_node, *(ignored_node_object.get()), nodes,
                               cache);
    }
    ignored_reasons.push_back(IgnoredReason(kAXNotRendered));
  }

  if (dom_node)
    ignored_node_object->setBackendDOMNodeId(DOMNodeIds::IdForNode(dom_node));

  std::unique_ptr<protocol::Array<AXProperty>> ignored_reason_properties =
      protocol::Array<AXProperty>::create();
  for (size_t i = 0; i < ignored_reasons.size(); i++)
    ignored_reason_properties->addItem(CreateProperty(ignored_reasons[i]));
  ignored_node_object->setIgnoredReasons(std::move(ignored_reason_properties));

  return ignored_node_object;
}

void InspectorAccessibilityAgent::PopulateDOMNodeAncestors(
    Node& inspected_dom_node,
    AXNode& node_object,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  // Walk up parents until an AXObject can be found.
  Node* parent_node = inspected_dom_node.IsShadowRoot()
                          ? &ToShadowRoot(inspected_dom_node).host()
                          : FlatTreeTraversal::Parent(inspected_dom_node);
  AXObject* parent_ax_object = cache.GetOrCreate(parent_node);
  while (parent_node && !parent_ax_object) {
    parent_node = parent_node->IsShadowRoot()
                      ? &ToShadowRoot(parent_node)->host()
                      : FlatTreeTraversal::Parent(*parent_node);
    parent_ax_object = cache.GetOrCreate(parent_node);
  }

  if (!parent_ax_object)
    return;

  if (parent_ax_object->AccessibilityIsIgnored())
    parent_ax_object = parent_ax_object->ParentObjectUnignored();
  if (!parent_ax_object)
    return;

  // Populate parent and ancestors.
  std::unique_ptr<AXNode> parent_node_object =
      BuildProtocolAXObject(*parent_ax_object, nullptr, true, nodes, cache);
  std::unique_ptr<protocol::Array<AXNodeId>> child_ids =
      protocol::Array<AXNodeId>::create();
  child_ids->addItem(String::Number(kIDForInspectedNodeWithNoAXNode));
  parent_node_object->setChildIds(std::move(child_ids));
  nodes->addItem(std::move(parent_node_object));

  AXObject* grandparent_ax_object = parent_ax_object->ParentObjectUnignored();
  if (grandparent_ax_object)
    AddAncestors(*grandparent_ax_object, nullptr, nodes, cache);
}

std::unique_ptr<AXNode> InspectorAccessibilityAgent::BuildProtocolAXObject(
    AXObject& ax_object,
    AXObject* inspected_ax_object,
    bool fetch_relatives,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  AccessibilityRole role = ax_object.RoleValue();
  std::unique_ptr<AXNode> node_object =
      AXNode::create()
          .setNodeId(String::Number(ax_object.AxObjectID()))
          .setIgnored(false)
          .build();
  node_object->setRole(CreateRoleNameValue(role));

  std::unique_ptr<protocol::Array<AXProperty>> properties =
      protocol::Array<AXProperty>::create();
  FillLiveRegionProperties(ax_object, *(properties.get()));
  FillGlobalStates(ax_object, *(properties.get()));
  FillWidgetProperties(ax_object, *(properties.get()));
  FillWidgetStates(ax_object, *(properties.get()));
  FillRelationships(ax_object, *(properties.get()));

  SparseAttributeAXPropertyAdapter adapter(ax_object, *properties);
  ax_object.GetSparseAXAttributes(adapter);

  AXObject::NameSources name_sources;
  String computed_name = ax_object.GetName(&name_sources);
  if (!name_sources.IsEmpty()) {
    std::unique_ptr<AXValue> name =
        CreateValue(computed_name, AXValueTypeEnum::ComputedString);
    if (!name_sources.IsEmpty()) {
      std::unique_ptr<protocol::Array<AXValueSource>> name_source_properties =
          protocol::Array<AXValueSource>::create();
      for (size_t i = 0; i < name_sources.size(); ++i) {
        NameSource& name_source = name_sources[i];
        name_source_properties->addItem(CreateValueSource(name_source));
        if (name_source.text.IsNull() || name_source.superseded)
          continue;
        if (!name_source.related_objects.IsEmpty()) {
          properties->addItem(CreateRelatedNodeListProperty(
              AXRelationshipAttributesEnum::Labelledby,
              name_source.related_objects));
        }
      }
      name->setSources(std::move(name_source_properties));
    }
    node_object->setProperties(std::move(properties));
    node_object->setName(std::move(name));
  } else {
    node_object->setProperties(std::move(properties));
  }

  FillCoreProperties(ax_object, inspected_ax_object, fetch_relatives,
                     *(node_object.get()), nodes, cache);
  return node_object;
}

void InspectorAccessibilityAgent::FillCoreProperties(
    AXObject& ax_object,
    AXObject* inspected_ax_object,
    bool fetch_relatives,
    AXNode& node_object,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  AXNameFrom name_from;
  AXObject::AXObjectVector name_objects;
  ax_object.GetName(name_from, &name_objects);

  AXDescriptionFrom description_from;
  AXObject::AXObjectVector description_objects;
  String description =
      ax_object.Description(name_from, description_from, &description_objects);
  if (!description.IsEmpty()) {
    node_object.setDescription(
        CreateValue(description, AXValueTypeEnum::ComputedString));
  }
  // Value.
  if (ax_object.SupportsRangeValue()) {
    node_object.setValue(CreateValue(ax_object.ValueForRange()));
  } else {
    String string_value = ax_object.StringValue();
    if (!string_value.IsEmpty())
      node_object.setValue(CreateValue(string_value));
  }

  if (fetch_relatives)
    PopulateRelatives(ax_object, inspected_ax_object, node_object, nodes,
                      cache);

  Node* node = ax_object.GetNode();
  if (node)
    node_object.setBackendDOMNodeId(DOMNodeIds::IdForNode(node));
}

void InspectorAccessibilityAgent::PopulateRelatives(
    AXObject& ax_object,
    AXObject* inspected_ax_object,
    AXNode& node_object,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  AXObject* parent_object = ax_object.ParentObject();
  if (parent_object && parent_object != inspected_ax_object) {
    // Use unignored parent unless parent is inspected ignored object.
    parent_object = ax_object.ParentObjectUnignored();
  }

  std::unique_ptr<protocol::Array<AXNodeId>> child_ids =
      protocol::Array<AXNodeId>::create();

  if (&ax_object != inspected_ax_object ||
      (inspected_ax_object && !inspected_ax_object->AccessibilityIsIgnored())) {
    AddChildren(ax_object, inspected_ax_object, child_ids, nodes, cache);
  }
  node_object.setChildIds(std::move(child_ids));
}

void InspectorAccessibilityAgent::AddChildren(
    AXObject& ax_object,
    AXObject* inspected_ax_object,
    std::unique_ptr<protocol::Array<AXNodeId>>& child_ids,
    std::unique_ptr<protocol::Array<AXNode>>& nodes,
    AXObjectCacheImpl& cache) const {
  if (inspected_ax_object && inspected_ax_object->AccessibilityIsIgnored() &&
      &ax_object == inspected_ax_object->ParentObjectUnignored()) {
    child_ids->addItem(String::Number(inspected_ax_object->AxObjectID()));
    return;
  }

  const AXObject::AXObjectVector& children = ax_object.Children();
  for (unsigned i = 0; i < children.size(); i++) {
    AXObject& child_ax_object = *children[i].Get();
    child_ids->addItem(String::Number(child_ax_object.AxObjectID()));
    if (&child_ax_object == inspected_ax_object)
      continue;
    if (&ax_object != inspected_ax_object) {
      if (!inspected_ax_object)
        continue;
      if (&ax_object != inspected_ax_object->ParentObjectUnignored())
        continue;
    }

    // Only add children of inspected node (or un-inspectable children of
    // inspected node) to returned nodes.
    std::unique_ptr<AXNode> child_node = BuildProtocolAXObject(
        child_ax_object, inspected_ax_object, true, nodes, cache);
    nodes->addItem(std::move(child_node));
  }
}

DEFINE_TRACE(InspectorAccessibilityAgent) {
  visitor->Trace(page_);
  visitor->Trace(dom_agent_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink

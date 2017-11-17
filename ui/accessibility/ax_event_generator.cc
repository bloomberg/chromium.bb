// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_event_generator.h"

#include "base/stl_util.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"

namespace ui {

AXEventGenerator::TargetedEvent::TargetedEvent(ui::AXNode* node, Event event)
    : node(node), event(event) {}

AXEventGenerator::Iterator::Iterator(
    const std::map<AXNode*, std::set<Event>>& map,
    const std::map<AXNode*, std::set<Event>>::const_iterator& head)
    : map_(map), map_iter_(head) {
  if (map_iter_ != map.end())
    set_iter_ = map_iter_->second.begin();
}

AXEventGenerator::Iterator::Iterator(const AXEventGenerator::Iterator& other) =
    default;

AXEventGenerator::Iterator::~Iterator() = default;

bool AXEventGenerator::Iterator::operator!=(
    const AXEventGenerator::Iterator& rhs) const {
  return map_iter_ != rhs.map_iter_ ||
         (map_iter_ != map_.end() && set_iter_ != rhs.set_iter_);
}

AXEventGenerator::Iterator& AXEventGenerator::Iterator::operator++() {
  if (map_iter_ == map_.end())
    return *this;

  set_iter_++;
  while (map_iter_ != map_.end() && set_iter_ == map_iter_->second.end()) {
    map_iter_++;
    if (map_iter_ != map_.end())
      set_iter_ = map_iter_->second.begin();
  }

  return *this;
}

AXEventGenerator::TargetedEvent AXEventGenerator::Iterator::operator*() const {
  DCHECK(map_iter_ != map_.end() && set_iter_ != map_iter_->second.end());
  return AXEventGenerator::TargetedEvent(map_iter_->first, *set_iter_);
}

AXEventGenerator::AXEventGenerator() : tree_(nullptr) {}

AXEventGenerator::AXEventGenerator(AXTree* tree) : tree_(tree) {
  if (tree_)
    tree_->SetDelegate(this);
}

AXEventGenerator::~AXEventGenerator() {
  if (tree_)
    tree_->SetDelegate(nullptr);
}

void AXEventGenerator::SetTree(AXTree* new_tree) {
  if (tree_)
    tree_->SetDelegate(nullptr);
  tree_ = new_tree;
  if (tree_)
    tree_->SetDelegate(this);
}

void AXEventGenerator::ReleaseTree() {
  tree_ = nullptr;
}

void AXEventGenerator::ClearEvents() {
  tree_events_.clear();
}

void AXEventGenerator::AddEvent(ui::AXNode* node,
                                AXEventGenerator::Event event) {
  if (node->data().role == AX_ROLE_INLINE_TEXT_BOX)
    return;

  // A newly created live region or alert should not *also* fire a
  // live region changed event.
  if (event == Event::LIVE_REGION_CHANGED &&
      (base::ContainsKey(tree_events_[node], Event::ALERT) ||
       base::ContainsKey(tree_events_[node], Event::LIVE_REGION_CREATED))) {
    return;
  }

  tree_events_[node].insert(event);
}

void AXEventGenerator::OnNodeDataWillChange(AXTree* tree,
                                            const AXNodeData& old_node_data,
                                            const AXNodeData& new_node_data) {
  DCHECK_EQ(tree_, tree);
  // Fire CHILDREN_CHANGED events when the list of children updates.
  // Internally we store inline text box nodes as children of a static text
  // node, which enables us to determine character bounds and line layout.
  // We don't expose those to platform APIs, though, so suppress
  // CHILDREN_CHANGED events on static text nodes.
  if (new_node_data.child_ids != old_node_data.child_ids &&
      new_node_data.role != ui::AX_ROLE_STATIC_TEXT) {
    AXNode* node = tree_->GetFromId(new_node_data.id);
    tree_events_[node].insert(Event::CHILDREN_CHANGED);
  }
}

void AXEventGenerator::OnRoleChanged(AXTree* tree,
                                     AXNode* node,
                                     AXRole old_role,
                                     AXRole new_role) {
  DCHECK_EQ(tree_, tree);
  AddEvent(node, Event::ROLE_CHANGED);
}

void AXEventGenerator::OnStateChanged(AXTree* tree,
                                      AXNode* node,
                                      AXState state,
                                      bool new_value) {
  DCHECK_EQ(tree_, tree);

  AddEvent(node, Event::STATE_CHANGED);
  if (state == ui::AX_STATE_EXPANDED) {
    AddEvent(node, new_value ? Event::EXPANDED : Event::COLLAPSED);
    if (node->data().role == ui::AX_ROLE_ROW ||
        node->data().role == ui::AX_ROLE_TREE_ITEM) {
      ui::AXNode* container = node;
      while (container && !ui::IsRowContainer(container->data().role))
        container = container->parent();
      if (container)
        AddEvent(container, Event::ROW_COUNT_CHANGED);
    }
  }
  if (state == ui::AX_STATE_SELECTED) {
    AddEvent(node, Event::SELECTED_CHANGED);
    ui::AXNode* container = node;
    while (container &&
           !ui::IsContainerWithSelectableChildrenRole(container->data().role))
      container = container->parent();
    if (container)
      AddEvent(container, Event::SELECTED_CHILDREN_CHANGED);
  }
}

void AXEventGenerator::OnStringAttributeChanged(AXTree* tree,
                                                AXNode* node,
                                                AXStringAttribute attr,
                                                const std::string& old_value,
                                                const std::string& new_value) {
  DCHECK_EQ(tree_, tree);

  switch (attr) {
    case ui::AX_ATTR_NAME:
      AddEvent(node, Event::NAME_CHANGED);
      if (node->data().HasStringAttribute(ui::AX_ATTR_CONTAINER_LIVE_STATUS)) {
        FireLiveRegionEvents(node);
      }
      break;
    case ui::AX_ATTR_DESCRIPTION:
      AddEvent(node, Event::DESCRIPTION_CHANGED);
      break;
    case ui::AX_ATTR_VALUE:
      AddEvent(node, Event::VALUE_CHANGED);
      break;
    case ui::AX_ATTR_ARIA_INVALID_VALUE:
      AddEvent(node, Event::INVALID_STATUS_CHANGED);
      break;
    case ui::AX_ATTR_LIVE_STATUS:
      if (node->data().role != ui::AX_ROLE_ALERT)
        AddEvent(node, Event::LIVE_REGION_CREATED);
      break;
    default:
      AddEvent(node, Event::OTHER_ATTRIBUTE_CHANGED);
      break;
  }
}

void AXEventGenerator::OnIntAttributeChanged(AXTree* tree,
                                             AXNode* node,
                                             AXIntAttribute attr,
                                             int32_t old_value,
                                             int32_t new_value) {
  DCHECK_EQ(tree_, tree);

  switch (attr) {
    case ui::AX_ATTR_ACTIVEDESCENDANT_ID:
      AddEvent(node, Event::ACTIVE_DESCENDANT_CHANGED);
      active_descendant_changed_.push_back(node);
      break;
    case ui::AX_ATTR_CHECKED_STATE:
      AddEvent(node, Event::CHECKED_STATE_CHANGED);
      break;
    case ui::AX_ATTR_INVALID_STATE:
      AddEvent(node, Event::INVALID_STATUS_CHANGED);
      break;
    case ui::AX_ATTR_RESTRICTION:
      AddEvent(node, Event::STATE_CHANGED);
      break;
    case ui::AX_ATTR_SCROLL_X:
    case ui::AX_ATTR_SCROLL_Y:
      AddEvent(node, Event::SCROLL_POSITION_CHANGED);
      break;
    default:
      AddEvent(node, Event::OTHER_ATTRIBUTE_CHANGED);
      break;
  }
}

void AXEventGenerator::OnFloatAttributeChanged(AXTree* tree,
                                               AXNode* node,
                                               AXFloatAttribute attr,
                                               float old_value,
                                               float new_value) {
  DCHECK_EQ(tree_, tree);

  if (attr == ui::AX_ATTR_VALUE_FOR_RANGE)
    AddEvent(node, Event::VALUE_CHANGED);
  else
    AddEvent(node, Event::OTHER_ATTRIBUTE_CHANGED);
}

void AXEventGenerator::OnBoolAttributeChanged(AXTree* tree,
                                              AXNode* node,
                                              AXBoolAttribute attr,
                                              bool new_value) {
  DCHECK_EQ(tree_, tree);

  AddEvent(node, Event::OTHER_ATTRIBUTE_CHANGED);
}

void AXEventGenerator::OnIntListAttributeChanged(
    AXTree* tree,
    AXNode* node,
    AXIntListAttribute attr,
    const std::vector<int32_t>& old_value,
    const std::vector<int32_t>& new_value) {
  DCHECK_EQ(tree_, tree);
  AddEvent(node, Event::OTHER_ATTRIBUTE_CHANGED);
}

void AXEventGenerator::OnStringListAttributeChanged(
    AXTree* tree,
    AXNode* node,
    AXStringListAttribute attr,
    const std::vector<std::string>& old_value,
    const std::vector<std::string>& new_value) {
  DCHECK_EQ(tree_, tree);
}

void AXEventGenerator::OnTreeDataChanged(AXTree* tree,
                                         const ui::AXTreeData& old_tree_data,
                                         const ui::AXTreeData& new_tree_data) {
  DCHECK_EQ(tree_, tree);

  if (new_tree_data.loaded && !old_tree_data.loaded)
    AddEvent(tree->root(), Event::LOAD_COMPLETE);
  if (new_tree_data.sel_anchor_object_id !=
          old_tree_data.sel_anchor_object_id ||
      new_tree_data.sel_anchor_offset != old_tree_data.sel_anchor_offset ||
      new_tree_data.sel_anchor_affinity != old_tree_data.sel_anchor_affinity ||
      new_tree_data.sel_focus_object_id != old_tree_data.sel_focus_object_id ||
      new_tree_data.sel_focus_offset != old_tree_data.sel_focus_offset ||
      new_tree_data.sel_focus_affinity != old_tree_data.sel_focus_affinity) {
    AddEvent(tree->root(), Event::DOCUMENT_SELECTION_CHANGED);
  }
  if (new_tree_data.title != old_tree_data.title)
    AddEvent(tree->root(), Event::DOCUMENT_TITLE_CHANGED);
}

void AXEventGenerator::OnNodeWillBeDeleted(AXTree* tree, AXNode* node) {
  if (node->data().HasStringAttribute(ui::AX_ATTR_CONTAINER_LIVE_STATUS) &&
      node->data().HasStringAttribute(ui::AX_ATTR_NAME)) {
    FireLiveRegionEvents(node);
  }

  DCHECK_EQ(tree_, tree);
  tree_events_.erase(node);
}

void AXEventGenerator::OnSubtreeWillBeDeleted(AXTree* tree, AXNode* node) {
  DCHECK_EQ(tree_, tree);
}

void AXEventGenerator::OnNodeWillBeReparented(AXTree* tree, AXNode* node) {
  DCHECK_EQ(tree_, tree);
  tree_events_.erase(node);
}

void AXEventGenerator::OnSubtreeWillBeReparented(AXTree* tree, AXNode* node) {
  DCHECK_EQ(tree_, tree);
}

void AXEventGenerator::OnNodeCreated(AXTree* tree, AXNode* node) {
  DCHECK_EQ(tree_, tree);
}

void AXEventGenerator::OnNodeReparented(AXTree* tree, AXNode* node) {
  DCHECK_EQ(tree_, tree);
}

void AXEventGenerator::OnNodeChanged(AXTree* tree, AXNode* node) {
  DCHECK_EQ(tree_, tree);
}

void AXEventGenerator::OnAtomicUpdateFinished(
    AXTree* tree,
    bool root_changed,
    const std::vector<Change>& changes) {
  DCHECK_EQ(tree_, tree);

  if (root_changed && tree->data().loaded)
    AddEvent(tree->root(), Event::LOAD_COMPLETE);

  for (const auto& change : changes) {
    if ((change.type == NODE_CREATED || change.type == SUBTREE_CREATED)) {
      if (change.node->data().HasStringAttribute(ui::AX_ATTR_LIVE_STATUS)) {
        if (change.node->data().role == ui::AX_ROLE_ALERT)
          AddEvent(change.node, Event::ALERT);
        else
          AddEvent(change.node, Event::LIVE_REGION_CREATED);
      } else if (change.node->data().HasStringAttribute(
                     ui::AX_ATTR_CONTAINER_LIVE_STATUS) &&
                 change.node->data().HasStringAttribute(ui::AX_ATTR_NAME)) {
        FireLiveRegionEvents(change.node);
      }
    }
  }

  FireActiveDescendantEvents();
}

void AXEventGenerator::FireLiveRegionEvents(AXNode* node) {
  ui::AXNode* live_root = node;
  while (live_root &&
         !live_root->data().HasStringAttribute(ui::AX_ATTR_LIVE_STATUS))
    live_root = live_root->parent();

  if (live_root && !live_root->data().GetBoolAttribute(ui::AX_ATTR_BUSY)) {
    // Fire LIVE_REGION_NODE_CHANGED on each node that changed.
    if (!node->data().GetStringAttribute(ui::AX_ATTR_NAME).empty())
      AddEvent(node, Event::LIVE_REGION_NODE_CHANGED);
    // Fire LIVE_REGION_NODE_CHANGED on the root of the live region.
    AddEvent(live_root, Event::LIVE_REGION_CHANGED);
  }
}

void AXEventGenerator::FireActiveDescendantEvents() {
  for (AXNode* node : active_descendant_changed_) {
    AXNode* descendant = tree_->GetFromId(
        node->data().GetIntAttribute(ui::AX_ATTR_ACTIVEDESCENDANT_ID));
    if (!descendant)
      continue;
    switch (descendant->data().role) {
      case ui::AX_ROLE_MENU_ITEM:
      case ui::AX_ROLE_MENU_ITEM_CHECK_BOX:
      case ui::AX_ROLE_MENU_ITEM_RADIO:
      case ui::AX_ROLE_MENU_LIST_OPTION:
        AddEvent(descendant, Event::MENU_ITEM_SELECTED);
        break;
      default:
        break;
    }
  }
  active_descendant_changed_.clear();
}

}  // namespace ui

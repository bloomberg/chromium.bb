// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node.h"

#include <string.h>

#include <algorithm>
#include <utility>

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_computed_node_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_hypertext.h"
#include "ui/accessibility/ax_language_detection.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_table_info.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/transform.h"

namespace ui {

// Definition of static class members.
constexpr char16_t AXNode::kEmbeddedCharacter[];
constexpr int AXNode::kEmbeddedCharacterLength;

AXNode::AXNode(AXNode::OwnerTree* tree,
               AXNode* parent,
               AXNodeID id,
               size_t index_in_parent,
               size_t unignored_index_in_parent)
    : tree_(tree),
      index_in_parent_(index_in_parent),
      unignored_index_in_parent_(unignored_index_in_parent),
      parent_(parent) {
  data_.id = id;
}

AXNode::~AXNode() = default;

AXNodeData&& AXNode::TakeData() {
  return std::move(data_);
}

const std::vector<AXNode*>& AXNode::GetAllChildren() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return children_;
}

size_t AXNode::GetChildCount() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return children_.size();
}

size_t AXNode::GetChildCountCrossingTreeBoundary() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());

  const AXTreeManager* child_tree_manager =
      AXTreeManagerMap::GetInstance().GetManagerForChildTree(*this);
  if (child_tree_manager)
    return 1u;

  return GetChildCount();
}

size_t AXNode::GetUnignoredChildCount() const {
  // TODO(nektar): Should DCHECK that this node is not ignored.
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return unignored_child_count_;
}

size_t AXNode::GetUnignoredChildCountCrossingTreeBoundary() const {
  // TODO(nektar): Should DCHECK that this node is not ignored.
  DCHECK(!tree_->GetTreeUpdateInProgressState());

  const AXTreeManager* child_tree_manager =
      AXTreeManagerMap::GetInstance().GetManagerForChildTree(*this);
  if (child_tree_manager) {
    DCHECK_EQ(unignored_child_count_, 0u)
        << "A node cannot be hosting both a child tree and other nodes as "
           "children.";
    return 1u;  // A child tree is never ignored.
  }

  return unignored_child_count_;
}

AXNode* AXNode::GetChildAtIndex(size_t index) const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  if (index >= GetChildCount())
    return nullptr;
  return children_[index];
}

AXNode* AXNode::GetChildAtIndexCrossingTreeBoundary(size_t index) const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());

  const AXTreeManager* child_tree_manager =
      AXTreeManagerMap::GetInstance().GetManagerForChildTree(*this);
  if (child_tree_manager) {
    DCHECK_EQ(index, 0u)
        << "A node cannot be hosting both a child tree and other nodes as "
           "children.";
    return child_tree_manager->GetRootAsAXNode();
  }

  return GetChildAtIndex(index);
}

AXNode* AXNode::GetUnignoredChildAtIndex(size_t index) const {
  // TODO(nektar): Should DCHECK that this node is not ignored.
  DCHECK(!tree_->GetTreeUpdateInProgressState());

  for (auto it = UnignoredChildrenBegin(); it != UnignoredChildrenEnd(); ++it) {
    if (index == 0)
      return it.get();
    --index;
  }

  return nullptr;
}

AXNode* AXNode::GetUnignoredChildAtIndexCrossingTreeBoundary(
    size_t index) const {
  // TODO(nektar): Should DCHECK that this node is not ignored.
  DCHECK(!tree_->GetTreeUpdateInProgressState());

  const AXTreeManager* child_tree_manager =
      AXTreeManagerMap::GetInstance().GetManagerForChildTree(*this);
  if (child_tree_manager) {
    DCHECK_EQ(index, 0u)
        << "A node cannot be hosting both a child tree and other nodes as "
           "children.";
    // A child tree is never ignored.
    return child_tree_manager->GetRootAsAXNode();
  }

  return GetUnignoredChildAtIndex(index);
}

AXNode* AXNode::GetParent() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return parent_;
}

AXNode* AXNode::GetParentCrossingTreeBoundary() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  if (parent_)
    return parent_;
  const AXTreeManager* manager =
      AXTreeManagerMap::GetInstance().GetManager(tree_->GetAXTreeID());
  if (manager)
    return manager->GetParentNodeFromParentTreeAsAXNode();
  return nullptr;
}

AXNode* AXNode::GetUnignoredParent() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  AXNode* unignored_parent = GetParent();
  while (unignored_parent && unignored_parent->IsIgnored())
    unignored_parent = unignored_parent->GetParent();

  return unignored_parent;
}

AXNode* AXNode::GetUnignoredParentCrossingTreeBoundary() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  AXNode* unignored_parent = GetUnignoredParent();
  if (!unignored_parent) {
    const AXTreeManager* manager =
        AXTreeManagerMap::GetInstance().GetManager(tree_->GetAXTreeID());
    if (manager)
      unignored_parent = manager->GetParentNodeFromParentTreeAsAXNode();
  }
  return unignored_parent;
}

size_t AXNode::GetIndexInParent() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return index_in_parent_;
}

size_t AXNode::GetUnignoredIndexInParent() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return unignored_index_in_parent_;
}

AXNode* AXNode::GetFirstChild() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return GetChildAtIndex(0);
}

AXNode* AXNode::GetFirstChildCrossingTreeBoundary() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return GetChildAtIndexCrossingTreeBoundary(0);
}

AXNode* AXNode::GetFirstUnignoredChild() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return ComputeFirstUnignoredChildRecursive();
}

AXNode* AXNode::GetFirstUnignoredChildCrossingTreeBoundary() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());

  const AXTreeManager* child_tree_manager =
      AXTreeManagerMap::GetInstance().GetManagerForChildTree(*this);
  if (child_tree_manager)
    return child_tree_manager->GetRootAsAXNode();

  return ComputeFirstUnignoredChildRecursive();
}

AXNode* AXNode::GetLastChild() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  size_t n = GetChildCount();
  if (n == 0)
    return nullptr;
  return GetChildAtIndex(n - 1);
}

AXNode* AXNode::GetLastChildCrossingTreeBoundary() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  size_t n = GetChildCountCrossingTreeBoundary();
  if (n == 0)
    return nullptr;
  return GetChildAtIndexCrossingTreeBoundary(n - 1);
}

AXNode* AXNode::GetLastUnignoredChild() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return ComputeLastUnignoredChildRecursive();
}

AXNode* AXNode::GetLastUnignoredChildCrossingTreeBoundary() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());

  const AXTreeManager* child_tree_manager =
      AXTreeManagerMap::GetInstance().GetManagerForChildTree(*this);
  if (child_tree_manager)
    return child_tree_manager->GetRootAsAXNode();

  return ComputeLastUnignoredChildRecursive();
}

AXNode* AXNode::GetDeepestFirstChild() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  if (!GetChildCount())
    return nullptr;

  AXNode* deepest_child = GetFirstChild();
  while (deepest_child->GetChildCount())
    deepest_child = deepest_child->GetFirstChild();

  return deepest_child;
}

AXNode* AXNode::GetDeepestFirstUnignoredChild() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  if (!GetUnignoredChildCount())
    return nullptr;

  AXNode* deepest_child = GetFirstUnignoredChild();
  while (deepest_child->GetUnignoredChildCount())
    deepest_child = deepest_child->GetFirstUnignoredChild();

  return deepest_child;
}

AXNode* AXNode::GetDeepestLastChild() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  if (!GetChildCount())
    return nullptr;

  AXNode* deepest_child = GetLastChild();
  while (deepest_child->GetChildCount())
    deepest_child = deepest_child->GetLastChild();

  return deepest_child;
}

AXNode* AXNode::GetDeepestLastUnignoredChild() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  if (!GetUnignoredChildCount())
    return nullptr;

  AXNode* deepest_child = GetLastUnignoredChild();
  while (deepest_child->GetUnignoredChildCount())
    deepest_child = deepest_child->GetLastUnignoredChild();

  return deepest_child;
}

AXNode* AXNode::GetNextSibling() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  AXNode* parent = GetParent();
  if (!parent)
    return nullptr;
  DCHECK(parent || !GetIndexInParent())
      << "Root nodes lack a parent. Their index_in_parent should be 0.";
  size_t nextIndex = GetIndexInParent() + 1;
  if (nextIndex >= parent->GetChildCount())
    return nullptr;
  return parent->GetChildAtIndex(nextIndex);
}

// Search for the next sibling of this node, skipping over any ignored nodes
// encountered.
//
// In our search:
//   If we find an ignored sibling, we consider its children as our siblings.
//   If we run out of siblings, we consider an ignored parent's siblings as our
//     own siblings.
//
// Note: this behaviour of 'skipping over' an ignored node makes this subtly
// different to finding the next (direct) sibling which is unignored.
//
// Consider a tree, where (i) marks a node as ignored:
//
//   1
//   ├── 2
//   ├── 3(i)
//   │   └── 5
//   └── 4
//
// The next sibling of node 2 is node 3, which is ignored.
// The next unignored sibling of node 2 could be either:
//  1) node 4 - next unignored sibling in the literal tree, or
//  2) node 5 - next unignored sibling in the logical document.
//
// There is no next sibling of node 5.
// The next unignored sibling of node 5 could be either:
//  1) null   - no next sibling in the literal tree, or
//  2) node 4 - next unignored sibling in the logical document.
//
// In both cases, this method implements approach (2).
//
// TODO(chrishall): Can we remove this non-reflexive case by forbidding
//   GetNextUnignoredSibling calls on an ignored started node?
// Note: this means that Next/Previous-UnignoredSibling are not reflexive if
// either of the nodes in question are ignored. From above we get an example:
//   NextUnignoredSibling(3)     is 4, but
//   PreviousUnignoredSibling(4) is 5.
//
// The view of unignored siblings for node 3 includes both node 2 and node 4:
//    2 <-- [3(i)] --> 4
//
// Whereas nodes 2, 5, and 4 do not consider node 3 to be an unignored sibling:
// null <-- [2] --> 5
//    2 <-- [5] --> 4
//    5 <-- [4] --> null
AXNode* AXNode::GetNextUnignoredSibling() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXNode* current = this;

  // If there are children of the |current| node still to consider.
  bool considerChildren = false;

  while (current) {
    // A |candidate| sibling to consider.
    // If it is unignored then we have found our result.
    // Otherwise promote it to |current| and consider its children.
    AXNode* candidate;

    if (considerChildren && (candidate = current->GetFirstChild())) {
      if (!candidate->IsIgnored())
        return candidate;
      current = candidate;

    } else if ((candidate = current->GetNextSibling())) {
      if (!candidate->IsIgnored())
        return candidate;
      current = candidate;
      // Look through the ignored candidate node to consider their children as
      // though they were siblings.
      considerChildren = true;

    } else {
      // Continue our search through a parent iff they are ignored.
      //
      // If |current| has an ignored parent, then we consider the parent's
      // siblings as though they were siblings of |current|.
      //
      // Given a tree:
      //   1
      //   ├── 2(?)
      //   │   └── [4]
      //   └── 3
      //
      // Node 4's view of siblings:
      //   literal tree:   null <-- [4] --> null
      //
      // If node 2 is not ignored, then node 4's view doesn't change, and we
      // have no more nodes to consider:
      //   unignored tree: null <-- [4] --> null
      //
      // If instead node 2 is ignored, then node 4's view of siblings grows to
      // include node 3, and we have more nodes to consider:
      //   unignored tree: null <-- [4] --> 3
      current = current->GetParent();
      if (!current || !current->IsIgnored())
        return nullptr;

      // We have already considered all relevant descendants of |current|.
      considerChildren = false;
    }
  }

  return nullptr;
}

AXNode* AXNode::GetPreviousSibling() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  DCHECK(GetParent() || !GetIndexInParent())
      << "Root nodes lack a parent. Their index_in_parent should be 0.";
  size_t index = GetIndexInParent();
  if (index == 0)
    return nullptr;
  return GetParent()->GetChildAtIndex(index - 1);
}

// Search for the previous sibling of this node, skipping over any ignored nodes
// encountered.
//
// In our search for a sibling:
//   If we find an ignored sibling, we may consider its children as siblings.
//   If we run out of siblings, we may consider an ignored parent's siblings as
//     our own.
//
// See the documentation for |GetNextUnignoredSibling| for more details.
AXNode* AXNode::GetPreviousUnignoredSibling() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXNode* current = this;

  // If there are children of the |current| node still to consider.
  bool considerChildren = false;

  while (current) {
    // A |candidate| sibling to consider.
    // If it is unignored then we have found our result.
    // Otherwise promote it to |current| and consider its children.
    AXNode* candidate;

    if (considerChildren && (candidate = current->GetLastChild())) {
      if (!candidate->IsIgnored())
        return candidate;
      current = candidate;

    } else if ((candidate = current->GetPreviousSibling())) {
      if (!candidate->IsIgnored())
        return candidate;
      current = candidate;
      // Look through the ignored candidate node to consider their children as
      // though they were siblings.
      considerChildren = true;

    } else {
      // Continue our search through a parent iff they are ignored.
      //
      // If |current| has an ignored parent, then we consider the parent's
      // siblings as though they were siblings of |current|.
      //
      // Given a tree:
      //   1
      //   ├── 2
      //   └── 3(?)
      //       └── [4]
      //
      // Node 4's view of siblings:
      //   literal tree:   null <-- [4] --> null
      //
      // If node 3 is not ignored, then node 4's view doesn't change, and we
      // have no more nodes to consider:
      //   unignored tree: null <-- [4] --> null
      //
      // If instead node 3 is ignored, then node 4's view of siblings grows to
      // include node 2, and we have more nodes to consider:
      //   unignored tree:    2 <-- [4] --> null
      current = current->GetParent();
      if (!current || !current->IsIgnored())
        return nullptr;

      // We have already considered all relevant descendants of |current|.
      considerChildren = false;
    }
  }

  return nullptr;
}

AXNode* AXNode::GetNextUnignoredInTreeOrder() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  if (GetUnignoredChildCount())
    return GetFirstUnignoredChild();

  const AXNode* node = this;
  while (node) {
    AXNode* sibling = node->GetNextUnignoredSibling();
    if (sibling)
      return sibling;

    node = node->GetUnignoredParent();
  }

  return nullptr;
}

AXNode* AXNode::GetPreviousUnignoredInTreeOrder() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  AXNode* sibling = GetPreviousUnignoredSibling();
  if (!sibling)
    return GetUnignoredParent();

  if (sibling->GetUnignoredChildCount())
    return sibling->GetDeepestLastUnignoredChild();

  return sibling;
}

AXNode::AllChildIterator AXNode::AllChildrenBegin() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return AllChildIterator(this, GetFirstChild());
}

AXNode::AllChildIterator AXNode::AllChildrenEnd() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return AllChildIterator(this, nullptr);
}

AXNode::AllChildCrossingTreeBoundaryIterator
AXNode::AllChildrenCrossingTreeBoundaryBegin() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return AllChildCrossingTreeBoundaryIterator(
      this, GetFirstChildCrossingTreeBoundary());
}

AXNode::AllChildCrossingTreeBoundaryIterator
AXNode::AllChildrenCrossingTreeBoundaryEnd() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return AllChildCrossingTreeBoundaryIterator(this, nullptr);
}

AXNode::UnignoredChildIterator AXNode::UnignoredChildrenBegin() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return UnignoredChildIterator(this, GetFirstUnignoredChild());
}

AXNode::UnignoredChildIterator AXNode::UnignoredChildrenEnd() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return UnignoredChildIterator(this, nullptr);
}

AXNode::UnignoredChildCrossingTreeBoundaryIterator
AXNode::UnignoredChildrenCrossingTreeBoundaryBegin() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return UnignoredChildCrossingTreeBoundaryIterator(
      this, GetFirstUnignoredChildCrossingTreeBoundary());
}

AXNode::UnignoredChildCrossingTreeBoundaryIterator
AXNode::UnignoredChildrenCrossingTreeBoundaryEnd() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return UnignoredChildCrossingTreeBoundaryIterator(this, nullptr);
}

bool AXNode::IsText() const {
  // In Legacy Layout, a list marker has no children and is thus represented on
  // all platforms as a leaf node that exposes the marker itself, i.e., it forms
  // part of the AX tree's text representation. In contrast, in Layout NG, a
  // list marker has a static text child.
  if (data().role == ax::mojom::Role::kListMarker)
    return !GetChildCount();
  return ui::IsText(data().role);
}

bool AXNode::IsLineBreak() const {
  return data().role == ax::mojom::Role::kLineBreak ||
         (data().role == ax::mojom::Role::kInlineTextBox &&
          GetBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject));
}

void AXNode::SetData(const AXNodeData& src) {
  data_ = src;
}

void AXNode::SetLocation(AXNodeID offset_container_id,
                         const gfx::RectF& location,
                         gfx::Transform* transform) {
  data_.relative_bounds.offset_container_id = offset_container_id;
  data_.relative_bounds.bounds = location;
  if (transform) {
    data_.relative_bounds.transform =
        std::make_unique<gfx::Transform>(*transform);
  } else {
    data_.relative_bounds.transform.reset();
  }
}

void AXNode::SetIndexInParent(size_t index_in_parent) {
  index_in_parent_ = index_in_parent;
}

void AXNode::UpdateUnignoredCachedValues() {
  computed_node_data_.reset();
  if (!IsIgnored())
    UpdateUnignoredCachedValuesRecursive(0);
}

void AXNode::SwapChildren(std::vector<AXNode*>* children) {
  children->swap(children_);
}

void AXNode::Destroy() {
  delete this;
}

bool AXNode::IsDescendantOf(const AXNode* ancestor) const {
  if (!ancestor)
    return false;

  if (this == ancestor)
    return true;
  if (GetParent())
    return GetParent()->IsDescendantOf(ancestor);

  return false;
}

std::vector<int> AXNode::GetOrComputeLineStartOffsets() {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  std::vector<int> line_offsets;
  if (GetIntListAttribute(ax::mojom::IntListAttribute::kCachedLineStarts,
                          &line_offsets)) {
    return line_offsets;
  }

  int start_offset = 0;
  ComputeLineStartOffsets(&line_offsets, &start_offset);
  data_.AddIntListAttribute(ax::mojom::IntListAttribute::kCachedLineStarts,
                            line_offsets);
  return line_offsets;
}

void AXNode::ComputeLineStartOffsets(std::vector<int>* line_offsets,
                                     int* start_offset) const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  DCHECK(line_offsets);
  DCHECK(start_offset);
  for (auto iter = AllChildrenCrossingTreeBoundaryBegin();
       iter != AllChildrenCrossingTreeBoundaryEnd(); ++iter) {
    if (iter->GetChildCountCrossingTreeBoundary()) {
      iter->ComputeLineStartOffsets(line_offsets, start_offset);
      continue;
    }

    // Don't report if the first piece of text starts a new line or not.
    if (*start_offset &&
        !iter->HasIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId)) {
      // If there are multiple objects with an empty accessible label at the
      // start of a line, only include a single line start offset.
      if (line_offsets->empty() || line_offsets->back() != *start_offset)
        line_offsets->push_back(*start_offset);
    }

    std::u16string text =
        iter->GetString16Attribute(ax::mojom::StringAttribute::kName);
    *start_offset += static_cast<int>(text.length());
  }
}

SkColor AXNode::ComputeColor() const {
  return ComputeColorAttribute(ax::mojom::IntAttribute::kColor);
}

SkColor AXNode::ComputeBackgroundColor() const {
  return ComputeColorAttribute(ax::mojom::IntAttribute::kBackgroundColor);
}

SkColor AXNode::ComputeColorAttribute(ax::mojom::IntAttribute attr) const {
  SkColor color = GetIntAttribute(attr);
  AXNode* ancestor = GetParent();

  // If the color has some transparency, keep blending with background
  // colors until we get an opaque color or reach the root.
  while (ancestor && SkColorGetA(color) != SK_AlphaOPAQUE) {
    SkColor background_color = ancestor->GetIntAttribute(attr);
    color = color_utils::GetResultingPaintColor(color, background_color);
    ancestor = ancestor->GetParent();
  }

  return color;
}

bool AXNode::HasStringAttribute(ax::mojom::StringAttribute attribute) const {
  return GetComputedNodeData().HasOrCanComputeAttribute(attribute);
}

const std::string& AXNode::GetStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  return GetComputedNodeData().GetOrComputeAttributeUTF8(attribute);
}

bool AXNode::GetStringAttribute(ax::mojom::StringAttribute attribute,
                                std::string* value) const {
  if (GetComputedNodeData().HasOrCanComputeAttribute(attribute)) {
    *value = GetComputedNodeData().GetOrComputeAttributeUTF8(attribute);
    return true;
  }
  return false;
}

std::u16string AXNode::GetString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  return GetComputedNodeData().GetOrComputeAttributeUTF16(attribute);
}

bool AXNode::GetString16Attribute(ax::mojom::StringAttribute attribute,
                                  std::u16string* value) const {
  if (GetComputedNodeData().HasOrCanComputeAttribute(attribute)) {
    *value = GetComputedNodeData().GetOrComputeAttributeUTF16(attribute);
    return true;
  }
  return false;
}

const std::string& AXNode::GetInheritedStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  const AXNode* current_node = this;
  do {
    if (current_node->HasStringAttribute(attribute))
      return current_node->GetStringAttribute(attribute);
    current_node = current_node->GetParent();
  } while (current_node);
  return base::EmptyString();
}

std::u16string AXNode::GetInheritedString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  return base::UTF8ToUTF16(GetInheritedStringAttribute(attribute));
}

AXLanguageInfo* AXNode::GetLanguageInfo() const {
  return language_info_.get();
}

void AXNode::SetLanguageInfo(std::unique_ptr<AXLanguageInfo> lang_info) {
  language_info_ = std::move(lang_info);
}

void AXNode::ClearLanguageInfo() {
  language_info_.reset();
}

const AXComputedNodeData& AXNode::GetComputedNodeData() const {
  if (!computed_node_data_)
    computed_node_data_ = std::make_unique<AXComputedNodeData>(*this);
  return *computed_node_data_;
}

void AXNode::ClearComputedNodeData() {
  computed_node_data_.reset();
}

const std::u16string& AXNode::GetHypertext() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  // TODO(nektar): Introduce proper caching of hypertext via
  // `AXHypertext::needs_update`.
  hypertext_ = AXHypertext();

  // Hypertext is not exposed for descendants of leaf nodes. For such nodes,
  // their inner text is equivalent to their hypertext. Otherwise, we would
  // never be able to compute equivalent ancestor positions in atomic text
  // fields given an AXPosition on an inline text box descendant, because there
  // is often an ignored generic container between the text descendants and the
  // text field node.
  //
  // For example, look at the following accessibility tree and the text
  // positions indicated using "<>" symbols in the inner text of every node, and
  // then imagine what would happen if the generic container was represented by
  // an "embedded object replacement character" in the text of its text field
  // parent.
  // ++kTextField "Hell<o>" IsLeaf=true
  // ++++kGenericContainer "Hell<o>" ignored IsChildOfLeaf=true
  // ++++++kStaticText "Hell<o>" IsChildOfLeaf=true
  // ++++++++kInlineTextBox "Hell<o>" IsChildOfLeaf=true

  if (IsLeaf() || IsChildOfLeaf()) {
    hypertext_.hypertext = base::UTF8ToUTF16(GetInnerText());
  } else {
    // Construct the hypertext for this node, which contains the concatenation
    // of the inner text of this node's textual children, and an "object
    // replacement character" for all the other children.
    //
    // Note that the word "hypertext" comes from the IAccessible2 Standard and
    // has nothing to do with HTML.
    static const base::NoDestructor<std::u16string> embedded_character_str(
        AXNode::kEmbeddedCharacter);
    auto first = UnignoredChildrenCrossingTreeBoundaryBegin();
    for (auto iter = first; iter != UnignoredChildrenCrossingTreeBoundaryEnd();
         ++iter) {
      // Similar to Firefox, we don't expose text nodes in IAccessible2 and ATK
      // hypertext with the embedded object character. We copy all of their text
      // instead.
      if (iter->IsText()) {
        hypertext_.hypertext += iter->GetInnerTextUTF16();
      } else {
        int character_offset = static_cast<int>(hypertext_.hypertext.size());
        auto inserted =
            hypertext_.hypertext_offset_to_hyperlink_child_index.emplace(
                character_offset, static_cast<int>(std::distance(first, iter)));
        DCHECK(inserted.second) << "An embedded object at " << character_offset
                                << " has already been encountered.";
        hypertext_.hypertext += *embedded_character_str;
      }
    }
  }

  hypertext_.needs_update = false;
  return hypertext_.hypertext;
}

void AXNode::SetNeedsToUpdateHypertext() {
  old_hypertext_ = hypertext_;
  hypertext_.needs_update = true;
  // TODO(nektar): Introduce proper caching of hypertext via
  // `AXHypertext::needs_update`.
  GetHypertext();  // Forces `hypertext_` to immediately update.
}

const std::map<int, int>& AXNode::GetHypertextOffsetToHyperlinkChildIndex()
    const {
  // TODO(nektar): Introduce proper caching of hypertext via
  // `AXHypertext::needs_update`.
  GetHypertext();  // Update `hypertext_` if not up-to-date.
  return hypertext_.hypertext_offset_to_hyperlink_child_index;
}

const AXHypertext& AXNode::GetOldHypertext() const {
  // TODO(nektar): Introduce proper caching of hypertext via
  // `AXHypertext::needs_update`.
  GetHypertext();  // Update `hypertext_` if not up-to-date.
  return old_hypertext_;
}

const std::string& AXNode::GetInnerText() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return GetComputedNodeData().GetOrComputeInnerTextUTF8();
}

const std::u16string& AXNode::GetInnerTextUTF16() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return GetComputedNodeData().GetOrComputeInnerTextUTF16();
}

int AXNode::GetInnerTextLength() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return GetComputedNodeData().GetOrComputeInnerTextLengthUTF8();
}

int AXNode::GetInnerTextLengthUTF16() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  return GetComputedNodeData().GetOrComputeInnerTextLengthUTF16();
}

std::string AXNode::GetLanguage() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  // Walk up tree considering both detected and author declared languages.
  for (const AXNode* cur = this; cur; cur = cur->GetParent()) {
    // If language detection has assigned a language then we prefer that.
    const AXLanguageInfo* lang_info = cur->GetLanguageInfo();
    if (lang_info && !lang_info->language.empty())
      return lang_info->language;

    // If the page author has declared a language attribute we fallback to that.
    if (cur->HasStringAttribute(ax::mojom::StringAttribute::kLanguage))
      return cur->GetStringAttribute(ax::mojom::StringAttribute::kLanguage);
  }

  return std::string();
}

std::string AXNode::GetValueForControl() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  if (data().IsTextField())
    return GetValueForTextField();
  if (data().IsRangeValueSupported())
    return GetTextForRangeValue();
  if (data().role == ax::mojom::Role::kColorWell)
    return GetValueForColorWell();
  if (!IsControl(data().role))
    return std::string();
  return GetStringAttribute(ax::mojom::StringAttribute::kValue);
}

std::ostream& operator<<(std::ostream& stream, const AXNode& node) {
  return stream << node.data().ToString();
}

bool AXNode::IsTable() const {
  return IsTableLike(data().role);
}

absl::optional<int> AXNode::GetTableColCount() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return absl::nullopt;
  return static_cast<int>(table_info->col_count);
}

absl::optional<int> AXNode::GetTableRowCount() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return absl::nullopt;
  return static_cast<int>(table_info->row_count);
}

absl::optional<int> AXNode::GetTableAriaColCount() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return absl::nullopt;
  return absl::make_optional(table_info->aria_col_count);
}

absl::optional<int> AXNode::GetTableAriaRowCount() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return absl::nullopt;
  return absl::make_optional(table_info->aria_row_count);
}

absl::optional<int> AXNode::GetTableCellCount() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return absl::nullopt;

  return static_cast<int>(table_info->unique_cell_ids.size());
}

absl::optional<bool> AXNode::GetTableHasColumnOrRowHeaderNode() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return absl::nullopt;

  return !table_info->all_headers.empty();
}

AXNode* AXNode::GetTableCellFromIndex(int index) const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return nullptr;

  // There is a table but there is no cell with the given index.
  if (index < 0 ||
      static_cast<size_t>(index) >= table_info->unique_cell_ids.size()) {
    return nullptr;
  }

  return tree_->GetFromId(
      table_info->unique_cell_ids[static_cast<size_t>(index)]);
}

AXNode* AXNode::GetTableCaption() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return nullptr;

  return tree_->GetFromId(table_info->caption_id);
}

AXNode* AXNode::GetTableCellFromCoords(int row_index, int col_index) const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return nullptr;

  // There is a table but the given coordinates are outside the table.
  if (row_index < 0 ||
      static_cast<size_t>(row_index) >= table_info->row_count ||
      col_index < 0 ||
      static_cast<size_t>(col_index) >= table_info->col_count) {
    return nullptr;
  }

  return tree_->GetFromId(table_info->cell_ids[static_cast<size_t>(row_index)]
                                              [static_cast<size_t>(col_index)]);
}

std::vector<AXNodeID> AXNode::GetTableColHeaderNodeIds() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::vector<AXNodeID>();

  std::vector<AXNodeID> col_header_ids;
  // Flatten and add column header ids of each column to |col_header_ids|.
  for (std::vector<AXNodeID> col_headers_at_index : table_info->col_headers) {
    col_header_ids.insert(col_header_ids.end(), col_headers_at_index.begin(),
                          col_headers_at_index.end());
  }

  return col_header_ids;
}

std::vector<AXNodeID> AXNode::GetTableColHeaderNodeIds(int col_index) const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::vector<AXNodeID>();

  if (col_index < 0 || static_cast<size_t>(col_index) >= table_info->col_count)
    return std::vector<AXNodeID>();

  return std::vector<AXNodeID>(
      table_info->col_headers[static_cast<size_t>(col_index)]);
}

std::vector<AXNodeID> AXNode::GetTableRowHeaderNodeIds(int row_index) const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::vector<AXNodeID>();

  if (row_index < 0 || static_cast<size_t>(row_index) >= table_info->row_count)
    return std::vector<AXNodeID>();

  return std::vector<AXNodeID>(
      table_info->row_headers[static_cast<size_t>(row_index)]);
}

std::vector<AXNodeID> AXNode::GetTableUniqueCellIds() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return std::vector<AXNodeID>();

  return std::vector<AXNodeID>(table_info->unique_cell_ids);
}

const std::vector<AXNode*>* AXNode::GetExtraMacNodes() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  // Should only be available on the table node itself, not any of its children.
  const AXTableInfo* table_info = tree_->GetTableInfo(this);
  if (!table_info)
    return nullptr;

  return &table_info->extra_mac_nodes;
}

//
// Table row-like nodes.
//

bool AXNode::IsTableRow() const {
  return ui::IsTableRow(data().role);
}

absl::optional<int> AXNode::GetTableRowRowIndex() const {
  if (!IsTableRow())
    return absl::nullopt;

  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return absl::nullopt;

  const auto& iter = table_info->row_id_to_index.find(id());
  if (iter == table_info->row_id_to_index.end())
    return absl::nullopt;
  return static_cast<int>(iter->second);
}

std::vector<AXNodeID> AXNode::GetTableRowNodeIds() const {
  std::vector<AXNodeID> row_node_ids;
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return row_node_ids;

  for (AXNode* node : table_info->row_nodes)
    row_node_ids.push_back(node->data().id);

  return row_node_ids;
}

#if defined(OS_APPLE)

//
// Table column-like nodes. These nodes are only present on macOS.
//

bool AXNode::IsTableColumn() const {
  return ui::IsTableColumn(data().role);
}

absl::optional<int> AXNode::GetTableColColIndex() const {
  if (!IsTableColumn())
    return absl::nullopt;

  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return absl::nullopt;

  int index = 0;
  for (const AXNode* node : table_info->extra_mac_nodes) {
    if (node == this)
      break;
    index++;
  }
  return index;
}

#endif  // defined(OS_APPLE)

//
// Table cell-like nodes.
//

bool AXNode::IsTableCellOrHeader() const {
  return IsCellOrTableHeader(data().role);
}

absl::optional<int> AXNode::GetTableCellIndex() const {
  if (!IsTableCellOrHeader())
    return absl::nullopt;

  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return absl::nullopt;

  const auto& iter = table_info->cell_id_to_index.find(id());
  if (iter != table_info->cell_id_to_index.end())
    return static_cast<int>(iter->second);
  return absl::nullopt;
}

absl::optional<int> AXNode::GetTableCellColIndex() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return absl::nullopt;

  absl::optional<int> index = GetTableCellIndex();
  if (!index)
    return absl::nullopt;

  return static_cast<int>(table_info->cell_data_vector[*index].col_index);
}

absl::optional<int> AXNode::GetTableCellRowIndex() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return absl::nullopt;

  absl::optional<int> index = GetTableCellIndex();
  if (!index)
    return absl::nullopt;

  return static_cast<int>(table_info->cell_data_vector[*index].row_index);
}

absl::optional<int> AXNode::GetTableCellColSpan() const {
  // If it's not a table cell, don't return a col span.
  if (!IsTableCellOrHeader())
    return absl::nullopt;

  // Otherwise, try to return a colspan, with 1 as the default if it's not
  // specified.
  int col_span;
  if (GetIntAttribute(ax::mojom::IntAttribute::kTableCellColumnSpan, &col_span))
    return col_span;
  return 1;
}

absl::optional<int> AXNode::GetTableCellRowSpan() const {
  // If it's not a table cell, don't return a row span.
  if (!IsTableCellOrHeader())
    return absl::nullopt;

  // Otherwise, try to return a row span, with 1 as the default if it's not
  // specified.
  int row_span;
  if (GetIntAttribute(ax::mojom::IntAttribute::kTableCellRowSpan, &row_span))
    return row_span;
  return 1;
}

absl::optional<int> AXNode::GetTableCellAriaColIndex() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return absl::nullopt;

  absl::optional<int> index = GetTableCellIndex();
  if (!index)
    return absl::nullopt;

  int aria_col_index =
      static_cast<int>(table_info->cell_data_vector[*index].aria_col_index);
  // |aria-colindex| attribute is one-based, value less than 1 is invalid.
  // https://www.w3.org/TR/wai-aria-1.2/#aria-colindex
  return (aria_col_index > 0) ? absl::optional<int>(aria_col_index)
                              : absl::nullopt;
}

absl::optional<int> AXNode::GetTableCellAriaRowIndex() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info)
    return absl::nullopt;

  absl::optional<int> index = GetTableCellIndex();
  if (!index)
    return absl::nullopt;

  int aria_row_index =
      static_cast<int>(table_info->cell_data_vector[*index].aria_row_index);
  // |aria-rowindex| attribute is one-based, value less than 1 is invalid.
  // https://www.w3.org/TR/wai-aria-1.2/#aria-rowindex
  return (aria_row_index > 0) ? absl::optional<int>(aria_row_index)
                              : absl::nullopt;
}

std::vector<AXNodeID> AXNode::GetTableCellColHeaderNodeIds() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info || table_info->col_count <= 0)
    return std::vector<AXNodeID>();

  // If this node is not a cell, then return the headers for the first column.
  int col_index = GetTableCellColIndex().value_or(0);

  return std::vector<AXNodeID>(table_info->col_headers[col_index]);
}

void AXNode::GetTableCellColHeaders(std::vector<AXNode*>* col_headers) const {
  DCHECK(col_headers);

  std::vector<AXNodeID> col_header_ids = GetTableCellColHeaderNodeIds();
  IdVectorToNodeVector(col_header_ids, col_headers);
}

std::vector<AXNodeID> AXNode::GetTableCellRowHeaderNodeIds() const {
  const AXTableInfo* table_info = GetAncestorTableInfo();
  if (!table_info || table_info->row_count <= 0)
    return std::vector<AXNodeID>();

  // If this node is not a cell, then return the headers for the first row.
  int row_index = GetTableCellRowIndex().value_or(0);

  return std::vector<AXNodeID>(table_info->row_headers[row_index]);
}

void AXNode::GetTableCellRowHeaders(std::vector<AXNode*>* row_headers) const {
  DCHECK(row_headers);

  std::vector<AXNodeID> row_header_ids = GetTableCellRowHeaderNodeIds();
  IdVectorToNodeVector(row_header_ids, row_headers);
}

bool AXNode::IsCellOrHeaderOfARIATable() const {
  if (!IsTableCellOrHeader())
    return false;

  const AXNode* node = this;
  while (node && !node->IsTable())
    node = node->GetParent();
  if (!node)
    return false;

  return node->data().role == ax::mojom::Role::kTable;
}

bool AXNode::IsCellOrHeaderOfARIAGrid() const {
  if (!IsTableCellOrHeader())
    return false;

  const AXNode* node = this;
  while (node && !node->IsTable())
    node = node->GetParent();
  if (!node)
    return false;

  return node->data().role == ax::mojom::Role::kGrid ||
         node->data().role == ax::mojom::Role::kTreeGrid;
}

AXTableInfo* AXNode::GetAncestorTableInfo() const {
  const AXNode* node = this;
  while (node && !node->IsTable())
    node = node->GetParent();
  if (node)
    return tree_->GetTableInfo(node);
  return nullptr;
}

void AXNode::IdVectorToNodeVector(const std::vector<AXNodeID>& ids,
                                  std::vector<AXNode*>* nodes) const {
  for (AXNodeID id : ids) {
    AXNode* node = tree_->GetFromId(id);
    if (node)
      nodes->push_back(node);
  }
}

absl::optional<int> AXNode::GetHierarchicalLevel() const {
  int hierarchical_level =
      GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);

  // According to the WAI_ARIA spec, a defined hierarchical level value is
  // greater than 0.
  // https://www.w3.org/TR/wai-aria-1.1/#aria-level
  if (hierarchical_level > 0)
    return hierarchical_level;

  return absl::nullopt;
}

bool AXNode::IsOrderedSetItem() const {
  return ui::IsItemLike(data().role);
}

bool AXNode::IsOrderedSet() const {
  return ui::IsSetLike(data().role);
}

// Uses AXTree's cache to calculate node's PosInSet.
absl::optional<int> AXNode::GetPosInSet() {
  return tree_->GetPosInSet(*this);
}

// Uses AXTree's cache to calculate node's SetSize.
absl::optional<int> AXNode::GetSetSize() {
  return tree_->GetSetSize(*this);
}

// Returns true if the role of ordered set matches the role of item.
// Returns false otherwise.
bool AXNode::SetRoleMatchesItemRole(const AXNode* ordered_set) const {
  ax::mojom::Role item_role = data().role;
  // Switch on role of ordered set
  switch (ordered_set->data().role) {
    case ax::mojom::Role::kFeed:
      return item_role == ax::mojom::Role::kArticle;
    case ax::mojom::Role::kList:
      return item_role == ax::mojom::Role::kListItem;
    case ax::mojom::Role::kGroup:
      return item_role == ax::mojom::Role::kComment ||
             item_role == ax::mojom::Role::kListItem ||
             item_role == ax::mojom::Role::kMenuItem ||
             item_role == ax::mojom::Role::kMenuItemRadio ||
             item_role == ax::mojom::Role::kListBoxOption ||
             item_role == ax::mojom::Role::kTreeItem;
    case ax::mojom::Role::kMenu:
      return item_role == ax::mojom::Role::kMenuItem ||
             item_role == ax::mojom::Role::kMenuItemRadio ||
             item_role == ax::mojom::Role::kMenuItemCheckBox;
    case ax::mojom::Role::kMenuBar:
      return item_role == ax::mojom::Role::kMenuItem ||
             item_role == ax::mojom::Role::kMenuItemRadio ||
             item_role == ax::mojom::Role::kMenuItemCheckBox;
    case ax::mojom::Role::kTabList:
      return item_role == ax::mojom::Role::kTab;
    case ax::mojom::Role::kTree:
      return item_role == ax::mojom::Role::kTreeItem;
    case ax::mojom::Role::kListBox:
      return item_role == ax::mojom::Role::kListBoxOption;
    case ax::mojom::Role::kMenuListPopup:
      return item_role == ax::mojom::Role::kMenuListOption ||
             item_role == ax::mojom::Role::kMenuItem ||
             item_role == ax::mojom::Role::kMenuItemRadio ||
             item_role == ax::mojom::Role::kMenuItemCheckBox;
    case ax::mojom::Role::kRadioGroup:
      return item_role == ax::mojom::Role::kRadioButton;
    case ax::mojom::Role::kDescriptionList:
      // Only the term for each description list entry should receive posinset
      // and setsize.
      return item_role == ax::mojom::Role::kDescriptionListTerm ||
             item_role == ax::mojom::Role::kTerm;
    case ax::mojom::Role::kPopUpButton:
      // kPopUpButtons can wrap a kMenuListPopUp.
      return item_role == ax::mojom::Role::kMenuListPopup;
    default:
      return false;
  }
}

bool AXNode::IsIgnoredContainerForOrderedSet() const {
  return IsIgnored() || IsEmbeddedGroup() ||
         data().role == ax::mojom::Role::kListItem ||
         data().role == ax::mojom::Role::kGenericContainer ||
         data().role == ax::mojom::Role::kUnknown;
}

int AXNode::UpdateUnignoredCachedValuesRecursive(int startIndex) {
  int count = 0;
  for (AXNode* child : children()) {
    if (child->IsIgnored()) {
      child->unignored_index_in_parent_ = 0;
      count += child->UpdateUnignoredCachedValuesRecursive(startIndex + count);
    } else {
      child->unignored_index_in_parent_ = startIndex + count++;
    }
  }
  unignored_child_count_ = count;
  return count;
}

// Finds ordered set that contains node.
// Is not required for set's role to match node's role.
AXNode* AXNode::GetOrderedSet() const {
  AXNode* result = GetParent();
  // Continue walking up while parent is invalid, ignored, a generic container,
  // unknown, or embedded group.
  while (result && result->IsIgnoredContainerForOrderedSet()) {
    result = result->GetParent();
  }

  return result;
}

AXNode* AXNode::ComputeLastUnignoredChildRecursive() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  if (children().empty())
    return nullptr;

  for (int i = static_cast<int>(children().size()) - 1; i >= 0; --i) {
    AXNode* child = children_[i];
    if (!child->IsIgnored())
      return child;

    AXNode* descendant = child->ComputeLastUnignoredChildRecursive();
    if (descendant)
      return descendant;
  }
  return nullptr;
}

AXNode* AXNode::ComputeFirstUnignoredChildRecursive() const {
  DCHECK(!tree_->GetTreeUpdateInProgressState());
  for (size_t i = 0; i < children().size(); i++) {
    AXNode* child = children_[i];
    if (!child->IsIgnored())
      return child;

    AXNode* descendant = child->ComputeFirstUnignoredChildRecursive();
    if (descendant)
      return descendant;
  }
  return nullptr;
}

std::string AXNode::GetTextForRangeValue() const {
  DCHECK(data().IsRangeValueSupported());
  std::string range_value =
      GetStringAttribute(ax::mojom::StringAttribute::kValue);
  float numeric_value;
  if (range_value.empty() &&
      GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                        &numeric_value)) {
    range_value = base::NumberToString(numeric_value);
  }
  return range_value;
}

std::string AXNode::GetValueForColorWell() const {
  DCHECK_EQ(data().role, ax::mojom::Role::kColorWell);
  // static cast because SkColor is a 4-byte unsigned int
  unsigned int color = static_cast<unsigned int>(
      GetIntAttribute(ax::mojom::IntAttribute::kColorValue));

  unsigned int red = SkColorGetR(color);
  unsigned int green = SkColorGetG(color);
  unsigned int blue = SkColorGetB(color);
  return base::StringPrintf("%d%% red %d%% green %d%% blue", red * 100 / 255,
                            green * 100 / 255, blue * 100 / 255);
}

std::string AXNode::GetValueForTextField() const {
  DCHECK(data().IsTextField());
  std::string value = GetStringAttribute(ax::mojom::StringAttribute::kValue);
  // Some screen readers like Jaws and VoiceOver require a value to be set in
  // text fields with rich content, even though the same information is
  // available on the children.
  if (value.empty() && data().IsNonAtomicTextField())
    return GetInnerText();
  return value;
}

bool AXNode::IsIgnored() const {
  return data().IsIgnored();
}

bool AXNode::IsIgnoredForTextNavigation() const {
  if (data().role == ax::mojom::Role::kSplitter)
    return true;

  // A generic container without any unignored children that is not editable
  // should not be used for text-based navigation. Such nodes don't make sense
  // for screen readers to land on, since no text will be announced and no
  // action is possible.
  if (data().role == ax::mojom::Role::kGenericContainer &&
      !GetUnignoredChildCount() &&
      !data().HasState(ax::mojom::State::kEditable)) {
    return true;
  }

  return false;
}

bool AXNode::IsInvisibleOrIgnored() const {
  if (!data().IsInvisibleOrIgnored())
    return false;

  return !IsFocusedWithinThisTree();
}

bool AXNode::IsFocusedWithinThisTree() const {
  return id() == tree_->data().focus_id;
}

bool AXNode::IsChildOfLeaf() const {
  for (const AXNode* ancestor = GetUnignoredParent(); ancestor;
       ancestor = ancestor->GetUnignoredParent()) {
    if (ancestor->IsLeaf())
      return true;
  }
  return false;
}

bool AXNode::IsEmptyLeaf() const {
  if (!IsLeaf())
    return false;
  if (GetUnignoredChildCountCrossingTreeBoundary())
    return !GetInnerTextLength();
  // Text exposed by ignored leaf (text) nodes is not exposed to the platforms'
  // accessibility layer, hence such leaf nodes are in effect empty.
  return IsIgnored() || !GetInnerTextLength();
}

bool AXNode::IsLeaf() const {
  // A node is a leaf if it has no descendants, i.e. if it is at the bottom of
  // the tree, regardless whether it is ignored or not.
  if (!GetChildCountCrossingTreeBoundary())
    return true;

  // Ignored nodes with any kind of descendants, (ignored or unignored), cannot
  // be leaves because: A) If some of their descendants are unignored then those
  // descendants need to be exposed to the platform layer, and B) If all of
  // their descendants are ignored they are still not at the bottom of the tree.
  if (IsIgnored())
    return false;

  // An unignored node is a leaf if all of its descendants are ignored.
  int child_count = GetUnignoredChildCountCrossingTreeBoundary();
  if (!child_count)
    return true;

#if defined(OS_WIN)
  // On Windows, we want to hide the subtree of a collapsed <select> element.
  // Otherwise, ATs are always going to announce its options whether it's
  // collapsed or expanded. In the AXTree, this element corresponds to a node
  // with role ax::mojom::Role::kPopUpButton that is the parent of a node with
  // role ax::mojom::Role::kMenuListPopup.
  if (IsCollapsedMenuListPopUpButton())
    return true;
#endif  // defined(OS_WIN)

  // These types of objects may have children that we use as internal
  // implementation details, but we want to expose them as leaves to platform
  // accessibility APIs because screen readers might be confused if they find
  // any children.
  // TODO(kschmi): <input type="search" contenteditable="true"> will cause
  // different return values here, even though 'contenteditable' has no effect.
  // This needs to be modified from the Blink side, so 'kRichlyEditable' isn't
  // added in this case.
  if (data().IsAtomicTextField() || IsText())
    return true;

  // Roles whose children are only presentational according to the ARIA and
  // HTML5 Specs should be hidden from screen readers.
  switch (data().role) {
    // According to the ARIA and Core-AAM specs:
    // https://w3c.github.io/aria/#button,
    // https://www.w3.org/TR/core-aam-1.1/#exclude_elements
    // buttons' children are presentational only and should be hidden from
    // screen readers. However, we cannot enforce the leafiness of buttons
    // because they may contain many rich, interactive descendants such as a day
    // in a calendar, and screen readers will need to interact with these
    // contents. See https://crbug.com/689204.
    // So we decided to not enforce the leafiness of buttons and expose all
    // children.
    case ax::mojom::Role::kButton:
      return false;
    case ax::mojom::Role::kImage: {
      // Images are not leaves when they are image maps. Therefore, do not
      // truncate descendants except in the case where ARIA role=img.
      std::string role = GetStringAttribute(ax::mojom::StringAttribute::kRole);
      return role == "img" || role == "image";
    }
    case ax::mojom::Role::kDocCover:
    case ax::mojom::Role::kGraphicsSymbol:
    case ax::mojom::Role::kMeter:
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSpinButton:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kProgressIndicator:
      return true;
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kMath:  // role="math" is flat, unlike <math>.
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kPopUpButton:
    case ax::mojom::Role::kToggleButton:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kTab: {
      // For historical reasons, truncate the children of these roles when they
      // have a single text child and are not editable.
      // TODO(accessibility) Consider removing this in the future, and exposing
      // all descendants, as it seems ATs do a good job of avoiding redundant
      // speech even if they have a text child. Removing this rule would allow
      // AT users to select any text visible in the page, and ensure that all
      // text is available to ATs that use the position of objects on the
      // screen. This has been manually tested in JAWS, NVDA, VoiceOver, Orca
      // and ChromeVox.
      // Note that the ARIA spec says, "User agents SHOULD NOT expose
      // descendants of this element through the platform accessibility API. If
      // user agents do not hide the descendant nodes, some information may be
      // read twice." However, this is not a MUST, and in non-simple cases
      // Chrome and Firefox already expose descendants, without causing issues.
      // Allow up to 2 text nodes so that list items with bullets are leaves.
      if (child_count > 2 || HasState(ax::mojom::State::kEditable))
        return false;
      AXNode* child1 = GetFirstUnignoredChildCrossingTreeBoundary();
      if (!child1 || child1->GetRole() != ax::mojom::Role::kStaticText)
        return false;
      AXNode* child2 = child1->GetNextSibling();
      return !child2 || child2->GetRole() == ax::mojom::Role::kStaticText;
    }
    default:
      return false;
  }
}

bool AXNode::IsInListMarker() const {
  if (data().role == ax::mojom::Role::kListMarker)
    return true;

  // The children of a list marker node can only be text nodes.
  if (!IsText())
    return false;

  // There is no need to iterate over all the ancestors of the current node
  // since a list marker has descendants that are only 2 levels deep, i.e.:
  // AXLayoutObject role=kListMarker
  // ++StaticText
  // ++++InlineTextBox
  AXNode* parent_node = GetUnignoredParent();
  if (parent_node && parent_node->data().role == ax::mojom::Role::kListMarker)
    return true;

  AXNode* grandparent_node = parent_node->GetUnignoredParent();
  return grandparent_node &&
         grandparent_node->data().role == ax::mojom::Role::kListMarker;
}

bool AXNode::IsCollapsedMenuListPopUpButton() const {
  if (data().role != ax::mojom::Role::kPopUpButton ||
      !data().HasState(ax::mojom::State::kCollapsed)) {
    return false;
  }

  // When a popup button contains a menu list popup, its only child is unignored
  // and is a menu list popup.
  AXNode* node = GetFirstUnignoredChild();
  if (!node)
    return false;

  return node->data().role == ax::mojom::Role::kMenuListPopup;
}

AXNode* AXNode::GetCollapsedMenuListPopUpButtonAncestor() const {
  AXNode* node = GetOrderedSet();

  if (!node)
    return nullptr;

  // The ordered set returned is either the popup element child of the popup
  // button (e.g., the AXMenuListPopup) or the popup button itself. We need
  // |node| to point to the popup button itself.
  if (node->data().role != ax::mojom::Role::kPopUpButton) {
    node = node->GetParent();
    if (!node)
      return nullptr;
  }

  return node->IsCollapsedMenuListPopUpButton() ? node : nullptr;
}

bool AXNode::IsEmbeddedGroup() const {
  if (data().role != ax::mojom::Role::kGroup || !GetParent())
    return false;

  return ui::IsSetLike(GetParent()->data().role);
}

AXNode* AXNode::GetLowestPlatformAncestor() const {
  AXNode* current_node = const_cast<AXNode*>(this);
  AXNode* lowest_unignored_node = current_node;
  for (; lowest_unignored_node && lowest_unignored_node->IsIgnored();
       lowest_unignored_node = lowest_unignored_node->GetParent()) {
  }

  // `highest_leaf_node` could be nullptr.
  AXNode* highest_leaf_node = lowest_unignored_node;
  // For the purposes of this method, a leaf node does not include leaves in the
  // internal accessibility tree, only in the platform exposed tree.
  for (AXNode* ancestor_node = lowest_unignored_node; ancestor_node;
       ancestor_node = ancestor_node->GetUnignoredParent()) {
    if (ancestor_node->IsLeaf())
      highest_leaf_node = ancestor_node;
  }
  if (highest_leaf_node)
    return highest_leaf_node;

  if (lowest_unignored_node)
    return lowest_unignored_node;
  return current_node;
}

AXNode* AXNode::GetTextFieldAncestor() const {
  // The descendants of a text field usually have State::kEditable, however in
  // the case of Role::kSearchBox or Role::kSpinButton being the text field
  // ancestor, its immediate descendant can have Role::kGenericContainer without
  // State::kEditable. Same with inline text boxes and placeholder text.
  // TODO(nektar): Fix all such inconsistencies in Blink.
  for (AXNode* ancestor = const_cast<AXNode*>(this);
       ancestor &&
       (ancestor->data().HasState(ax::mojom::State::kEditable) ||
        ancestor->data().role == ax::mojom::Role::kGenericContainer ||
        ancestor->IsText());
       ancestor = ancestor->GetUnignoredParent()) {
    if (ancestor->data().IsTextField())
      return ancestor;
  }
  return nullptr;
}

bool AXNode::IsDescendantOfAtomicTextField() const {
  AXNode* text_field_node = GetTextFieldAncestor();
  return text_field_node && text_field_node->data().IsAtomicTextField();
}

}  // namespace ui

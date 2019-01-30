// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_delegate_base.h"

#include <vector>

#include "base/no_destructor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_data.h"

namespace ui {

AXPlatformNodeDelegateBase::AXPlatformNodeDelegateBase() = default;

AXPlatformNodeDelegateBase::~AXPlatformNodeDelegateBase() = default;

const AXNodeData& AXPlatformNodeDelegateBase::GetData() const {
  static base::NoDestructor<AXNodeData> empty_data;
  return *empty_data;
}

const AXTreeData& AXPlatformNodeDelegateBase::GetTreeData() const {
  static base::NoDestructor<AXTreeData> empty_data;
  return *empty_data;
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::GetNSWindow() {
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::GetParent() {
  return nullptr;
}

int AXPlatformNodeDelegateBase::GetChildCount() {
  return 0;
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::ChildAtIndex(int index) {
  return nullptr;
}

gfx::Rect AXPlatformNodeDelegateBase::GetClippedScreenBoundsRect() const {
  return gfx::Rect();
}

gfx::Rect AXPlatformNodeDelegateBase::GetUnclippedScreenBoundsRect() const {
  return gfx::Rect();
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::HitTestSync(int x,
                                                                  int y) {
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::GetFocus() {
  return nullptr;
}

AXPlatformNode* AXPlatformNodeDelegateBase::GetFromNodeID(int32_t id) {
  return nullptr;
}

int AXPlatformNodeDelegateBase::GetIndexInParent() const {
  return -1;
}

gfx::AcceleratedWidget
AXPlatformNodeDelegateBase::GetTargetForNativeAccessibilityEvent() {
  return gfx::kNullAcceleratedWidget;
}

bool AXPlatformNodeDelegateBase::IsTable() const {
  return false;
}

int AXPlatformNodeDelegateBase::GetTableRowCount() const {
  return 0;
}

int AXPlatformNodeDelegateBase::GetTableColCount() const {
  return 0;
}

int32_t AXPlatformNodeDelegateBase::GetTableAriaColCount() const {
  return 0;
}

int32_t AXPlatformNodeDelegateBase::GetTableAriaRowCount() const {
  return 0;
}

int32_t AXPlatformNodeDelegateBase::GetTableCellCount() const {
  return 0;
}

const std::vector<int32_t> AXPlatformNodeDelegateBase::GetColHeaderNodeIds()
    const {
  return std::vector<int32_t>();
}

const std::vector<int32_t> AXPlatformNodeDelegateBase::GetColHeaderNodeIds(
    int32_t col_index) const {
  return std::vector<int32_t>();
}

const std::vector<int32_t> AXPlatformNodeDelegateBase::GetRowHeaderNodeIds()
    const {
  return std::vector<int32_t>();
}

const std::vector<int32_t> AXPlatformNodeDelegateBase::GetRowHeaderNodeIds(
    int32_t row_index) const {
  return std::vector<int32_t>();
}

bool AXPlatformNodeDelegateBase::IsTableRow() const {
  return false;
}

int32_t AXPlatformNodeDelegateBase::GetTableRowRowIndex() const {
  return 0;
}

bool AXPlatformNodeDelegateBase::IsTableCellOrHeader() const {
  return false;
}

int32_t AXPlatformNodeDelegateBase::GetTableCellColIndex() const {
  return 0;
}

int32_t AXPlatformNodeDelegateBase::GetTableCellRowIndex() const {
  return 0;
}

int32_t AXPlatformNodeDelegateBase::GetTableCellColSpan() const {
  return 0;
}

int32_t AXPlatformNodeDelegateBase::GetTableCellRowSpan() const {
  return 0;
}

int32_t AXPlatformNodeDelegateBase::GetTableCellAriaColIndex() const {
  return 0;
}

int32_t AXPlatformNodeDelegateBase::GetTableCellAriaRowIndex() const {
  return 0;
}

int32_t AXPlatformNodeDelegateBase::GetCellId(int32_t row_index,
                                              int32_t col_index) const {
  return -1;
}

int32_t AXPlatformNodeDelegateBase::GetTableCellIndex() const {
  return -1;
}

int32_t AXPlatformNodeDelegateBase::CellIndexToId(int32_t cell_index) const {
  return -1;
}

bool AXPlatformNodeDelegateBase::AccessibilityPerformAction(
    const ui::AXActionData& data) {
  return false;
}

bool AXPlatformNodeDelegateBase::ShouldIgnoreHoveredStateForTesting() {
  return true;
}

bool AXPlatformNodeDelegateBase::IsOffscreen() const {
  return false;
}

AXPlatformNode* AXPlatformNodeDelegateBase::GetTargetNodeForRelation(
    ax::mojom::IntAttribute attr) {
  DCHECK(IsNodeIdIntAttribute(attr));

  int target_id;
  if (!GetData().GetIntAttribute(attr, &target_id))
    return nullptr;

  return GetFromNodeID(target_id);
}

std::set<AXPlatformNode*> AXPlatformNodeDelegateBase::GetNodesForNodeIds(
    const std::set<int32_t>& ids) {
  std::set<AXPlatformNode*> nodes;
  for (int32_t node_id : ids) {
    if (AXPlatformNode* node = GetFromNodeID(node_id)) {
      nodes.insert(node);
    }
  }
  return nodes;
}

std::set<AXPlatformNode*> AXPlatformNodeDelegateBase::GetTargetNodesForRelation(
    ax::mojom::IntListAttribute attr) {
  DCHECK(IsNodeIdIntListAttribute(attr));
  std::vector<int32_t> target_ids;
  if (!GetData().GetIntListAttribute(attr, &target_ids))
    return std::set<AXPlatformNode*>();

  std::set<int32_t> target_id_set(target_ids.begin(), target_ids.end());
  return GetNodesForNodeIds(target_id_set);
}

std::set<AXPlatformNode*> AXPlatformNodeDelegateBase::GetReverseRelations(
    ax::mojom::IntAttribute attr) {
  return std::set<AXPlatformNode*>();
}

std::set<AXPlatformNode*> AXPlatformNodeDelegateBase::GetReverseRelations(
    ax::mojom::IntListAttribute attr) {
  return std::set<AXPlatformNode*>();
}

const AXUniqueId& AXPlatformNodeDelegateBase::GetUniqueId() const {
  static base::NoDestructor<AXUniqueId> dummy_unique_id;
  return *dummy_unique_id;
}

bool AXPlatformNodeDelegateBase::IsOrderedSetItem() const {
  return false;
}

bool AXPlatformNodeDelegateBase::IsOrderedSet() const {
  return false;
}

int32_t AXPlatformNodeDelegateBase::GetPosInSet() const {
  return 0;
}

int32_t AXPlatformNodeDelegateBase::GetSetSize() const {
  return 0;
}

}  // namespace ui

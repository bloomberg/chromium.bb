// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/test_ax_node_wrapper.h"

#include <unordered_map>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_table_info.h"
#include "ui/accessibility/ax_tree_observer.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace ui {

namespace {

// A global map from AXNodes to TestAXNodeWrappers.
std::unordered_map<AXNode*, TestAXNodeWrapper*> g_node_to_wrapper_map;

// A global coordinate offset.
gfx::Vector2d g_offset;

// A global map that stores which node is focused on a determined tree.
//   - If a tree has no node being focused, there shouldn't be any entry on the
//     map associated with such tree, i.e. a pair {tree, nullptr} is invalid.
//   - For testing purposes, assume there is a single node being focused in the
//     entire tree and if such node is deleted, focus is completely lost.
std::unordered_map<AXTree*, AXNode*> g_focused_node_in_tree;

// A global indicating the last node which ShowContextMenu was called from.
AXNode* g_node_from_last_show_context_menu;

// A simple implementation of AXTreeObserver to catch when AXNodes are
// deleted so we can delete their wrappers.
class TestAXTreeObserver : public AXTreeObserver {
 private:
  void OnNodeWillBeDeleted(AXTree* tree, AXNode* node) override {
    auto iter = g_node_to_wrapper_map.find(node);
    if (iter != g_node_to_wrapper_map.end()) {
      TestAXNodeWrapper* wrapper = iter->second;
      delete wrapper;
      g_node_to_wrapper_map.erase(iter->first);
    }
  }
};

TestAXTreeObserver g_ax_tree_observer;

}  // namespace

// static
TestAXNodeWrapper* TestAXNodeWrapper::GetOrCreate(AXTree* tree, AXNode* node) {
  if (!tree || !node)
    return nullptr;

  if (!tree->HasObserver(&g_ax_tree_observer))
    tree->AddObserver(&g_ax_tree_observer);
  auto iter = g_node_to_wrapper_map.find(node);
  if (iter != g_node_to_wrapper_map.end())
    return iter->second;
  TestAXNodeWrapper* wrapper = new TestAXNodeWrapper(tree, node);
  g_node_to_wrapper_map[node] = wrapper;
  return wrapper;
}

// static
void TestAXNodeWrapper::SetGlobalCoordinateOffset(const gfx::Vector2d& offset) {
  g_offset = offset;
}

// static
const AXNode* TestAXNodeWrapper::GetNodeFromLastShowContextMenu() {
  return g_node_from_last_show_context_menu;
}

TestAXNodeWrapper::~TestAXNodeWrapper() {
  platform_node_->Destroy();
}

const AXNodeData& TestAXNodeWrapper::GetData() const {
  return node_->data();
}

const AXTreeData& TestAXNodeWrapper::GetTreeData() const {
  return tree_->data();
}

AXNodePosition::AXPositionInstance TestAXNodeWrapper::CreateTextPositionAt(
    int offset,
    ax::mojom::TextAffinity affinity) const {
  return ui::AXNodePosition::CreateTextPosition(GetTreeData().tree_id,
                                                node_->id(), offset, affinity);
}

gfx::NativeViewAccessible TestAXNodeWrapper::GetParent() {
  TestAXNodeWrapper* parent_wrapper = GetOrCreate(tree_, node_->parent());
  return parent_wrapper ?
      parent_wrapper->ax_platform_node()->GetNativeViewAccessible() :
      nullptr;
}

int TestAXNodeWrapper::GetChildCount() {
  return node_->child_count();
}

gfx::NativeViewAccessible TestAXNodeWrapper::ChildAtIndex(int index) {
  CHECK_GE(index, 0);
  CHECK_LT(index, GetChildCount());
  TestAXNodeWrapper* child_wrapper =
      GetOrCreate(tree_, node_->children()[index]);
  return child_wrapper ?
      child_wrapper->ax_platform_node()->GetNativeViewAccessible() :
      nullptr;
}

gfx::Rect TestAXNodeWrapper::GetClippedScreenBoundsRect() const {
  // We could add clipping here if needed.
  gfx::RectF bounds = GetData().relative_bounds.bounds;
  bounds.Offset(g_offset);
  return gfx::ToEnclosingRect(bounds);
}

gfx::Rect TestAXNodeWrapper::GetUnclippedScreenBoundsRect() const {
  gfx::RectF bounds = GetData().relative_bounds.bounds;
  bounds.Offset(g_offset);
  return gfx::ToEnclosingRect(bounds);
}

gfx::Rect TestAXNodeWrapper::GetScreenBoundsForRange(int start,
                                                     int len,
                                                     bool clipped) const {
  // Ignoring start, len, and clipped, as there's no clean way to map these
  // via unit tests.
  gfx::RectF bounds = GetData().relative_bounds.bounds;
  bounds.Offset(g_offset);
  return gfx::ToEnclosingRect(bounds);
}

TestAXNodeWrapper* TestAXNodeWrapper::HitTestSyncInternal(int x, int y) {
  // Here we find the deepest child whose bounding box contains the given point.
  // The assuptions are that there are no overlapping bounding rects and that
  // all children have smaller bounding rects than their parents.
  if (!GetClippedScreenBoundsRect().Contains(gfx::Rect(x, y, 0, 0)))
    return nullptr;

  for (int i = 0; i < GetChildCount(); i++) {
    TestAXNodeWrapper* child = GetOrCreate(tree_, node_->children()[i]);
    if (!child)
      return nullptr;

    TestAXNodeWrapper* result = child->HitTestSyncInternal(x, y);
    if (result) {
      return result;
    }
  }
  return this;
}

gfx::NativeViewAccessible TestAXNodeWrapper::HitTestSync(int x, int y) {
  TestAXNodeWrapper* wrapper = HitTestSyncInternal(x, y);
  return wrapper ? wrapper->ax_platform_node()->GetNativeViewAccessible()
                 : nullptr;
}

gfx::NativeViewAccessible TestAXNodeWrapper::GetFocus() {
  auto focused = g_focused_node_in_tree.find(tree_);
  if (focused != g_focused_node_in_tree.end() &&
      focused->second->IsDescendantOf(node_)) {
    return GetOrCreate(tree_, focused->second)
        ->ax_platform_node()
        ->GetNativeViewAccessible();
  }
  return nullptr;
}

// Walk the AXTree and ensure that all wrappers are created
void TestAXNodeWrapper::BuildAllWrappers(AXTree* tree, AXNode* node) {
  for (int i = 0; i < node->child_count(); i++) {
    auto* child = node->children()[i];
    TestAXNodeWrapper::GetOrCreate(tree, child);

    BuildAllWrappers(tree, child);
  }
}

AXPlatformNode* TestAXNodeWrapper::GetFromNodeID(int32_t id) {
  // Force creating all of the wrappers for this tree.
  BuildAllWrappers(tree_, node_);

  for (auto it = g_node_to_wrapper_map.begin();
       it != g_node_to_wrapper_map.end(); ++it) {
    AXNode* node = it->first;
    if (node->id() == id) {
      TestAXNodeWrapper* wrapper = it->second;
      return wrapper->ax_platform_node();
    }
  }
  return nullptr;
}

int TestAXNodeWrapper::GetIndexInParent() const {
  return node_ ? node_->index_in_parent() : -1;
}

void TestAXNodeWrapper::ReplaceIntAttribute(int32_t node_id,
                                            ax::mojom::IntAttribute attribute,
                                            int32_t value) {
  if (!tree_)
    return;

  AXNode* node = tree_->GetFromId(node_id);
  if (!node)
    return;

  AXNodeData new_data = node->data();
  std::vector<std::pair<ax::mojom::IntAttribute, int32_t>>& attributes =
      new_data.int_attributes;

  base::EraseIf(attributes, [attribute](auto& pair) {
    return pair.first == attribute;
  });

  new_data.AddIntAttribute(attribute, value);
  node->SetData(new_data);
}

void TestAXNodeWrapper::ReplaceBoolAttribute(ax::mojom::BoolAttribute attribute,
                                             bool value) {
  AXNodeData new_data = GetData();
  std::vector<std::pair<ax::mojom::BoolAttribute, bool>>& attributes =
      new_data.bool_attributes;

  base::EraseIf(attributes,
                [attribute](auto& pair) { return pair.first == attribute; });

  new_data.AddBoolAttribute(attribute, value);
  node_->SetData(new_data);
}

bool TestAXNodeWrapper::IsTable() const {
  return node_->IsTable();
}

int TestAXNodeWrapper::GetTableRowCount() const {
  return node_->GetTableRowCount();
}

int TestAXNodeWrapper::GetTableColCount() const {
  return node_->GetTableColCount();
}

base::Optional<int32_t> TestAXNodeWrapper::GetTableAriaRowCount() const {
  return node_->GetTableAriaRowCount();
}

base::Optional<int32_t> TestAXNodeWrapper::GetTableAriaColCount() const {
  return node_->GetTableAriaColCount();
}

int TestAXNodeWrapper::GetTableCellCount() const {
  return node_->GetTableCellCount();
}

const std::vector<int32_t> TestAXNodeWrapper::GetColHeaderNodeIds() const {
  std::vector<int32_t> header_ids;
  node_->GetTableCellColHeaderNodeIds(&header_ids);
  return header_ids;
}

const std::vector<int32_t> TestAXNodeWrapper::GetColHeaderNodeIds(
    int32_t col_index) const {
  std::vector<int32_t> header_ids;
  node_->GetTableColHeaderNodeIds(col_index, &header_ids);
  return header_ids;
}

const std::vector<int32_t> TestAXNodeWrapper::GetRowHeaderNodeIds() const {
  std::vector<int32_t> header_ids;
  node_->GetTableCellRowHeaderNodeIds(&header_ids);
  return header_ids;
}

const std::vector<int32_t> TestAXNodeWrapper::GetRowHeaderNodeIds(
    int32_t row_index) const {
  std::vector<int32_t> header_ids;
  node_->GetTableRowHeaderNodeIds(row_index, &header_ids);
  return header_ids;
}

bool TestAXNodeWrapper::IsTableRow() const {
  return node_->IsTableRow();
}

int TestAXNodeWrapper::GetTableRowRowIndex() const {
  return node_->GetTableRowRowIndex();
}

bool TestAXNodeWrapper::IsTableCellOrHeader() const {
  return node_->IsTableCellOrHeader();
}

int TestAXNodeWrapper::GetTableCellIndex() const {
  return node_->GetTableCellIndex();
}

int TestAXNodeWrapper::GetTableCellColIndex() const {
  return node_->GetTableCellColIndex();
}

int TestAXNodeWrapper::GetTableCellRowIndex() const {
  return node_->GetTableCellRowIndex();
}

int TestAXNodeWrapper::GetTableCellColSpan() const {
  return node_->GetTableCellColSpan();
}

int TestAXNodeWrapper::GetTableCellRowSpan() const {
  return node_->GetTableCellRowSpan();
}

int TestAXNodeWrapper::GetTableCellAriaColIndex() const {
  return node_->GetTableCellAriaColIndex();
}

int TestAXNodeWrapper::GetTableCellAriaRowIndex() const {
  return node_->GetTableCellAriaRowIndex();
}

int32_t TestAXNodeWrapper::GetCellId(int32_t row_index,
                                     int32_t col_index) const {
  ui::AXNode* cell = node_->GetTableCellFromCoords(row_index, col_index);
  if (cell)
    return cell->id();

  return -1;
}

gfx::AcceleratedWidget
TestAXNodeWrapper::GetTargetForNativeAccessibilityEvent() {
#if defined(OS_WIN)
  return gfx::kMockAcceleratedWidget;
#else
  return AXPlatformNodeDelegateBase::GetTargetForNativeAccessibilityEvent();
#endif
}

int32_t TestAXNodeWrapper::CellIndexToId(int32_t cell_index) const {
  ui::AXNode* cell = node_->GetTableCellFromIndex(cell_index);
  if (cell)
    return cell->id();
  return -1;
}

bool TestAXNodeWrapper::AccessibilityPerformAction(
    const ui::AXActionData& data) {
  switch (data.action) {
    case ax::mojom::Action::kScrollToPoint:
      g_offset = gfx::Vector2d(data.target_point.x(), data.target_point.y());
      return true;

    case ax::mojom::Action::kScrollToMakeVisible: {
      auto offset = node_->data().relative_bounds.bounds.OffsetFromOrigin();
      g_offset = gfx::Vector2d(-offset.x(), -offset.y());
      return true;
    }

    case ax::mojom::Action::kDoDefault:
      if (GetData().role == ax::mojom::Role::kListBoxOption ||
          GetData().role == ax::mojom::Role::kCell) {
        bool current_value =
            GetData().GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
        ReplaceBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                             !current_value);
      }
      return true;

    case ax::mojom::Action::kSetSelection:
      ReplaceIntAttribute(data.anchor_node_id,
                          ax::mojom::IntAttribute::kTextSelStart,
                          data.anchor_offset);
      ReplaceIntAttribute(data.anchor_node_id,
                          ax::mojom::IntAttribute::kTextSelEnd,
                          data.focus_offset);
      return true;

    case ax::mojom::Action::kFocus:
      g_focused_node_in_tree[tree_] = node_;
      return true;

    case ax::mojom::Action::kShowContextMenu:
      g_node_from_last_show_context_menu = node_;
      return true;

    default:
      return true;
  }
}

base::string16 TestAXNodeWrapper::GetLocalizedRoleDescriptionForUnlabeledImage()
    const {
  return base::ASCIIToUTF16("Unlabeled image");
}

base::string16 TestAXNodeWrapper::GetLocalizedStringForImageAnnotationStatus(
    ax::mojom::ImageAnnotationStatus status) const {
  switch (status) {
    case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
      return base::ASCIIToUTF16(
          "To get missing image descriptions, open the context menu.");
    case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
      return base::ASCIIToUTF16("Getting description...");
    case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
      return base::ASCIIToUTF16(
          "Appears to contain adult content. No description available.");
    case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
    case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
    case ax::mojom::ImageAnnotationStatus::kNone:
    case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
      return base::string16();
  }

  NOTREACHED();
  return base::string16();
}

bool TestAXNodeWrapper::ShouldIgnoreHoveredStateForTesting() {
  return true;
}

std::set<AXPlatformNode*> TestAXNodeWrapper::GetReverseRelations(
    ax::mojom::IntAttribute attr) {
  DCHECK(IsNodeIdIntAttribute(attr));
  return GetNodesForNodeIds(tree_->GetReverseRelations(attr, GetData().id));
}

std::set<AXPlatformNode*> TestAXNodeWrapper::GetReverseRelations(
    ax::mojom::IntListAttribute attr) {
  DCHECK(IsNodeIdIntListAttribute(attr));
  return GetNodesForNodeIds(tree_->GetReverseRelations(attr, GetData().id));
}

const ui::AXUniqueId& TestAXNodeWrapper::GetUniqueId() const {
  return unique_id_;
}

TestAXNodeWrapper::TestAXNodeWrapper(AXTree* tree, AXNode* node)
    : tree_(tree),
      node_(node),
      platform_node_(AXPlatformNode::Create(this)) {
}

bool TestAXNodeWrapper::IsOrderedSetItem() const {
  return node_->IsOrderedSetItem();
}

bool TestAXNodeWrapper::IsOrderedSet() const {
  return node_->IsOrderedSet();
}

int32_t TestAXNodeWrapper::GetPosInSet() const {
  return node_->GetPosInSet();
}

int32_t TestAXNodeWrapper::GetSetSize() const {
  return node_->GetSetSize();
}

}  // namespace ui

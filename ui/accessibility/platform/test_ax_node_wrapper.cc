// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/hash_tables.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/platform/test_ax_node_wrapper.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace ui {

namespace {

// A global map from AXNodes to TestAXNodeWrappers.
base::hash_map<AXNode*, TestAXNodeWrapper*> g_node_to_wrapper_map;

// A global coordinate offset.
gfx::Vector2d g_offset;

// A simple implementation of AXTreeDelegate to catch when AXNodes are
// deleted so we can delete their wrappers.
class TestAXTreeDelegate : public AXTreeDelegate {
  void OnNodeDataWillChange(AXTree* tree,
                            const AXNodeData& old_node_data,
                            const AXNodeData& new_node_data) override {}
  void OnTreeDataChanged(AXTree* tree,
                         const AXTreeData& old_data,
                         const AXTreeData& new_data) override {}
  void OnNodeWillBeDeleted(AXTree* tree, AXNode* node) override {
    auto iter = g_node_to_wrapper_map.find(node);
    if (iter != g_node_to_wrapper_map.end()) {
      TestAXNodeWrapper* wrapper = iter->second;
      delete wrapper;
      g_node_to_wrapper_map.erase(iter->first);
    }
  }
  void OnSubtreeWillBeDeleted(AXTree* tree, AXNode* node) override {}
  void OnNodeWillBeReparented(AXTree* tree, AXNode* node) override {}
  void OnSubtreeWillBeReparented(AXTree* tree, AXNode* node) override {}
  void OnNodeCreated(AXTree* tree, AXNode* node) override {}
  void OnNodeReparented(AXTree* tree, AXNode* node) override {}
  void OnNodeChanged(AXTree* tree, AXNode* node) override {}
  void OnAtomicUpdateFinished(AXTree* tree,
                              bool root_changed,
                              const std::vector<Change>& changes) override {}
};

TestAXTreeDelegate g_ax_tree_delegate;

}  // namespace

// static
TestAXNodeWrapper* TestAXNodeWrapper::GetOrCreate(AXTree* tree, AXNode* node) {
  if (!tree || !node)
    return nullptr;

  tree->SetDelegate(&g_ax_tree_delegate);
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

TestAXNodeWrapper::~TestAXNodeWrapper() {
  platform_node_->Destroy();
}

const AXNodeData& TestAXNodeWrapper::GetData() const {
  return node_->data();
}

const AXTreeData& TestAXNodeWrapper::GetTreeData() const {
  return tree_->data();
}

gfx::NativeWindow TestAXNodeWrapper::GetTopLevelWidget() {
  return nullptr;
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

gfx::Rect TestAXNodeWrapper::GetScreenBoundsRect() const {
  gfx::RectF bounds = GetData().location;
  bounds.Offset(g_offset);
  return gfx::ToEnclosingRect(bounds);
}

TestAXNodeWrapper* TestAXNodeWrapper::HitTestSyncInternal(int x, int y) {
  // Here we find the deepest child whose bounding box contains the given point.
  // The assuptions are that there are no overlapping bounding rects and that
  // all children have smaller bounding rects than their parents.
  if (!GetScreenBoundsRect().Contains(gfx::Rect(x, y)))
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
  return -1;
}

gfx::AcceleratedWidget
TestAXNodeWrapper::GetTargetForNativeAccessibilityEvent() {
  return gfx::kNullAcceleratedWidget;
}

void TestAXNodeWrapper::ReplaceIntAttribute(int32_t node_id,
                                            AXIntAttribute attribute,
                                            int32_t value) {
  if (!tree_)
    return;

  AXNode* node = tree_->GetFromId(node_id);
  if (!node)
    return;

  AXNodeData new_data = node->data();
  std::vector<std::pair<AXIntAttribute, int32_t>>& attributes =
      new_data.int_attributes;

  auto deleted = std::remove_if(
      attributes.begin(), attributes.end(),
      [attribute](auto& pair) { return pair.first == attribute; });
  attributes.erase(deleted, attributes.end());

  new_data.AddIntAttribute(attribute, value);
  node->SetData(new_data);
}

bool TestAXNodeWrapper::AccessibilityPerformAction(
    const ui::AXActionData& data) {
  if (data.action == ui::AX_ACTION_SCROLL_TO_POINT) {
    g_offset = gfx::Vector2d(data.target_point.x(), data.target_point.x());
    return true;
  }

  if (data.action == ui::AX_ACTION_SCROLL_TO_MAKE_VISIBLE) {
    g_offset = gfx::Vector2d(data.target_rect.x(), data.target_rect.x());
    return true;
  }

  if (data.action == ui::AX_ACTION_SET_SELECTION) {
    ReplaceIntAttribute(data.anchor_node_id, AX_ATTR_TEXT_SEL_START,
                        data.anchor_offset);
    ReplaceIntAttribute(data.anchor_node_id, AX_ATTR_TEXT_SEL_END,
                        data.focus_offset);
    return true;
  }

  return true;
}

bool TestAXNodeWrapper::ShouldIgnoreHoveredStateForTesting() {
  return true;
}

bool TestAXNodeWrapper::IsOffscreen() const {
  return false;
}

TestAXNodeWrapper::TestAXNodeWrapper(AXTree* tree, AXNode* node)
    : tree_(tree),
      node_(node),
      platform_node_(AXPlatformNode::Create(this)) {
}

}  // namespace ui

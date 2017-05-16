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
  void OnTreeDataChanged(AXTree* tree) override {}
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

gfx::NativeViewAccessible TestAXNodeWrapper::HitTestSync(int x, int y) {
  return nullptr;
}

gfx::NativeViewAccessible TestAXNodeWrapper::GetFocus() {
  return nullptr;
}

gfx::AcceleratedWidget
TestAXNodeWrapper::GetTargetForNativeAccessibilityEvent() {
  return gfx::kNullAcceleratedWidget;
}

bool TestAXNodeWrapper::AccessibilityPerformAction(
    const ui::AXActionData& data) {
  return true;
}

TestAXNodeWrapper::TestAXNodeWrapper(AXTree* tree, AXNode* node)
    : tree_(tree),
      node_(node),
      platform_node_(AXPlatformNode::Create(this)) {
}

}  // namespace ui

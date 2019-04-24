// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_aura_window_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_source_checker.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_tree_source_views.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

bool HasNodeWithName(ui::AXNode* node, const std::string& name) {
  if (node->GetStringAttribute(ax::mojom::StringAttribute::kName) == name)
    return true;
  for (auto* child : node->children()) {
    if (HasNodeWithName(child, name))
      return true;
  }
  return false;
}

bool HasNodeWithName(ui::AXTree* tree, const std::string& name) {
  return HasNodeWithName(tree->root(), name);
}

// Subclass of AXAuraWindowUtils that skips any aura::Window named ParentWindow.
class TestAXAuraWindowUtils : public AXAuraWindowUtils {
 public:
  TestAXAuraWindowUtils() {}
  ~TestAXAuraWindowUtils() override {}

 private:
  aura::Window* GetParent(aura::Window* window) override {
    aura::Window* parent = window->parent();
    if (parent && parent->GetTitle() == base::ASCIIToUTF16("ParentWindow"))
      return parent->parent();
    return parent;
  }

  aura::Window::Windows GetChildren(aura::Window* window) override {
    auto children = window->children();
    if (children.size() == 1 &&
        children[0]->GetTitle() == base::ASCIIToUTF16("ParentWindow"))
      return children[0]->children();
    return children;
  }
};

using AuraAXTreeSerializer = ui::
    AXTreeSerializer<views::AXAuraObjWrapper*, ui::AXNodeData, ui::AXTreeData>;

using AuraAXTreeSourceChecker =
    ui::AXTreeSourceChecker<views::AXAuraObjWrapper*,
                            ui::AXNodeData,
                            ui::AXTreeData>;

class AXAuraWindowUtilsTest : public ViewsTestBase {
 public:
  AXAuraWindowUtilsTest() = default;
  ~AXAuraWindowUtilsTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    parent_widget_ = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    parent_widget_->Init(params);
    parent_widget_->GetNativeWindow()->SetTitle(
        base::ASCIIToUTF16("ParentWindow"));
    parent_widget_->Show();

    child_widget_ = new Widget();  // Owned by parent_widget_.
    params = CreateParams(Widget::InitParams::TYPE_BUBBLE);
    params.parent = parent_widget_->GetNativeWindow();
    params.child = true;
    params.bounds = gfx::Rect(100, 100, 200, 200);
    params.ownership = views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
    child_widget_->Init(params);
    auto* button = new LabelButton(nullptr, base::ASCIIToUTF16("ChildButton"));
    button->SetSize(gfx::Size(20, 20));
    child_widget_->GetContentsView()->AddChildView(button);
    child_widget_->GetNativeWindow()->SetTitle(
        base::ASCIIToUTF16("ChildWindow"));
    child_widget_->Show();
  }

  void TearDown() override {
    // Close and reset the parent widget. The child widget will be cleaned
    // up automatically as it's owned by the parent widget.
    if (!parent_widget_->IsClosed())
      parent_widget_->CloseNow();
    parent_widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<ui::AXTree> GetAccessibilityTreeFromWindow(
      aura::Window* window) {
    ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
    AXAuraObjCache* cache = AXAuraObjCache::GetInstance();
    AXTreeSourceViews tree_source(cache->GetOrCreate(window), tree_id);
    AuraAXTreeSerializer serializer(&tree_source);
    ui::AXTreeUpdate serialized_tree;
    serializer.SerializeChanges(tree_source.GetRoot(), &serialized_tree);

    AuraAXTreeSourceChecker checker(&tree_source);
    if (!checker.Check())
      return std::make_unique<ui::AXTree>();

    // For debugging, run with --v=1.
    VLOG(1) << serialized_tree.ToString();

    return std::make_unique<ui::AXTree>(serialized_tree);
  }

  std::unique_ptr<Widget> parent_widget_;
  Widget* child_widget_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(AXAuraWindowUtilsTest);
};

TEST_F(AXAuraWindowUtilsTest, GetHierarchy) {
  aura::Window* window = child_widget_->GetNativeWindow();
  aura::Window* root = window->GetRootWindow();
  std::unique_ptr<ui::AXTree> ax_tree = GetAccessibilityTreeFromWindow(root);

  // For debugging, run with --v=1.
  VLOG(1) << ax_tree->ToString();

  EXPECT_TRUE(HasNodeWithName(ax_tree.get(), "ParentWindow"));
  EXPECT_TRUE(HasNodeWithName(ax_tree.get(), "ChildWindow"));
  EXPECT_TRUE(HasNodeWithName(ax_tree.get(), "ChildButton"));
}

TEST_F(AXAuraWindowUtilsTest, GetHierarchyWithTestAuraWindowUtils) {
  // TestAXAuraWindowUtils will skip over an aura::Window
  // with a title of "ParentWindow".
  AXAuraWindowUtils::Set(std::make_unique<TestAXAuraWindowUtils>());

  aura::Window* window = child_widget_->GetNativeWindow();
  aura::Window* root = window->GetRootWindow();
  std::unique_ptr<ui::AXTree> ax_tree = GetAccessibilityTreeFromWindow(root);

  // For debugging, run with --v=1.
  VLOG(1) << ax_tree->ToString();

  EXPECT_FALSE(HasNodeWithName(ax_tree.get(), "ParentWindow"));
  EXPECT_TRUE(HasNodeWithName(ax_tree.get(), "ChildWindow"));
  EXPECT_TRUE(HasNodeWithName(ax_tree.get(), "ChildButton"));
}

}  // namespace
}  // namespace views

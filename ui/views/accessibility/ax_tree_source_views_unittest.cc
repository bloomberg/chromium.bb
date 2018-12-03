// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_tree_source_views.h"

#include <stddef.h>

#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"
#include "ui/views/accessibility/ax_root_obj_wrapper.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

using AuraAXTreeSerializer = ui::
    AXTreeSerializer<views::AXAuraObjWrapper*, ui::AXNodeData, ui::AXTreeData>;

// Helper to count the number of nodes in a tree.
size_t GetSize(AXAuraObjWrapper* tree) {
  size_t count = 1;

  std::vector<AXAuraObjWrapper*> out_children;
  tree->GetChildren(&out_children);

  for (size_t i = 0; i < out_children.size(); ++i)
    count += GetSize(out_children[i]);

  return count;
}

// TestAXTreeSourceViews provides a root with a default tree ID.
class TestAXTreeSourceViews : public AXTreeSourceViews {
 public:
  TestAXTreeSourceViews(AXAuraObjWrapper* root)
      : AXTreeSourceViews(root, ui::AXTreeID::FromString("123")) {}

  ~TestAXTreeSourceViews() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAXTreeSourceViews);
};

class AXTreeSourceViewsTest : public ViewsTestBase {
 public:
  AXTreeSourceViewsTest() = default;
  ~AXTreeSourceViewsTest() override = default;

  // testing::Test:
  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(11, 22, 333, 444);
    params.context = GetContext();
    widget_->Init(params);
    widget_->SetContentsView(new View());

    label1_ = new Label(base::ASCIIToUTF16("Label 1"));
    label1_->SetBounds(1, 1, 111, 111);
    widget_->GetContentsView()->AddChildView(label1_);

    label2_ = new Label(base::ASCIIToUTF16("Label 2"));
    label2_->SetBounds(2, 2, 222, 222);
    widget_->GetContentsView()->AddChildView(label2_);

    textfield_ = new Textfield();
    textfield_->SetBounds(222, 2, 20, 200);
    widget_->GetContentsView()->AddChildView(textfield_);
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  std::unique_ptr<Widget> widget_;
  Label* label1_ = nullptr;         // Owned by views hierarchy.
  Label* label2_ = nullptr;         // Owned by views hierarchy.
  Textfield* textfield_ = nullptr;  // Owned by views hierarchy.

 private:
  DISALLOW_COPY_AND_ASSIGN(AXTreeSourceViewsTest);
};

TEST_F(AXTreeSourceViewsTest, Basics) {
  AXAuraObjCache* cache = AXAuraObjCache::GetInstance();

  // Start the tree at the Widget's contents view.
  AXAuraObjWrapper* root = cache->GetOrCreate(widget_->GetContentsView());
  TestAXTreeSourceViews tree(root);
  EXPECT_EQ(root, tree.GetRoot());

  // The root has no parent.
  EXPECT_FALSE(tree.GetParent(root));

  // The root has the right children.
  std::vector<AXAuraObjWrapper*> children;
  tree.GetChildren(root, &children);
  ASSERT_EQ(3u, children.size());

  // The labels are the children.
  AXAuraObjWrapper* label1 = children[0];
  AXAuraObjWrapper* label2 = children[1];
  AXAuraObjWrapper* textfield = children[2];
  EXPECT_EQ(label1, cache->GetOrCreate(label1_));
  EXPECT_EQ(label2, cache->GetOrCreate(label2_));
  EXPECT_EQ(textfield, cache->GetOrCreate(textfield_));

  // The parents is correct.
  EXPECT_EQ(root, tree.GetParent(label1));
  EXPECT_EQ(root, tree.GetParent(label2));
  EXPECT_EQ(root, tree.GetParent(textfield));

  // IDs match the ones in the cache.
  EXPECT_EQ(root->GetUniqueId(), tree.GetId(root));
  EXPECT_EQ(label1->GetUniqueId(), tree.GetId(label1));
  EXPECT_EQ(label2->GetUniqueId(), tree.GetId(label2));
  EXPECT_EQ(textfield->GetUniqueId(), tree.GetId(textfield));

  // Reverse ID lookups work.
  EXPECT_EQ(root, tree.GetFromId(root->GetUniqueId()));
  EXPECT_EQ(label1, tree.GetFromId(label1->GetUniqueId()));
  EXPECT_EQ(label2, tree.GetFromId(label2->GetUniqueId()));
  EXPECT_EQ(textfield, tree.GetFromId(textfield->GetUniqueId()));

  // Validity.
  EXPECT_TRUE(tree.IsValid(root));
  EXPECT_FALSE(tree.IsValid(nullptr));

  // Comparisons.
  EXPECT_TRUE(tree.IsEqual(label1, label1));
  EXPECT_FALSE(tree.IsEqual(label1, label2));
  EXPECT_FALSE(tree.IsEqual(label1, nullptr));
  EXPECT_FALSE(tree.IsEqual(nullptr, label1));

  // Null pointers is the null value.
  EXPECT_EQ(nullptr, tree.GetNull());
}

TEST_F(AXTreeSourceViewsTest, GetTreeDataWithFocus) {
  AXAuraObjCache* cache = AXAuraObjCache::GetInstance();
  TestAXTreeSourceViews tree(cache->GetOrCreate(widget_.get()));
  textfield_->RequestFocus();

  ui::AXTreeData tree_data;
  tree.GetTreeData(&tree_data);
  EXPECT_TRUE(tree_data.loaded);
  EXPECT_EQ(cache->GetID(textfield_), tree_data.focus_id);
}

TEST_F(AXTreeSourceViewsTest, IgnoredView) {
  View* ignored_view = new View();
  ignored_view->GetViewAccessibility().OverrideIsIgnored(true);
  widget_->GetContentsView()->AddChildView(ignored_view);

  AXAuraObjCache* cache = AXAuraObjCache::GetInstance();
  TestAXTreeSourceViews tree(cache->GetOrCreate(widget_.get()));
  EXPECT_FALSE(tree.IsValid(cache->GetOrCreate(ignored_view)));
}

// Tests integration of AXTreeSourceViews with AXRootObjWrapper, similar to how
// AX trees are serialized by Chrome.
class AXTreeSourceViewsRootTest : public ViewsTestBase {
 public:
  AXTreeSourceViewsRootTest() = default;
  ~AXTreeSourceViewsRootTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = new Widget();
    Widget::InitParams init_params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    init_params.context = GetContext();
    widget_->Init(init_params);

    content_ = new View();
    widget_->SetContentsView(content_);

    textfield_ = new Textfield();
    textfield_->SetText(base::ASCIIToUTF16("Value"));
    content_->AddChildView(textfield_);
    widget_->Show();
  }

  void TearDown() override {
    // ViewsTestBase requires all Widgets to be closed before shutdown.
    widget_->CloseNow();
    ViewsTestBase::TearDown();
  }

 protected:
  Widget* widget_;
  View* content_;
  Textfield* textfield_;
  // A simulated desktop root with no delegate.
  AXRootObjWrapper root_wrapper_{nullptr};

 private:
  DISALLOW_COPY_AND_ASSIGN(AXTreeSourceViewsRootTest);
};

TEST_F(AXTreeSourceViewsRootTest, Accessors) {
  // Focus the textfield so the cursor does not disappear.
  textfield_->RequestFocus();

  AXTreeSourceViews ax_tree(&root_wrapper_, ui::DesktopAXTreeID());
  ASSERT_TRUE(ax_tree.GetRoot());

  // ID's should be > 0.
  ASSERT_GE(ax_tree.GetRoot()->GetUniqueId(), 1);

  // Grab the content view directly from cache to avoid walking down the tree.
  AXAuraObjWrapper* content =
      AXAuraObjCache::GetInstance()->GetOrCreate(content_);
  std::vector<AXAuraObjWrapper*> content_children;
  ax_tree.GetChildren(content, &content_children);
  ASSERT_EQ(1U, content_children.size());

  // Walk down to the text field and assert it is what we expect.
  AXAuraObjWrapper* textfield = content_children[0];
  AXAuraObjWrapper* cached_textfield =
      AXAuraObjCache::GetInstance()->GetOrCreate(textfield_);
  ASSERT_EQ(cached_textfield, textfield);
  std::vector<AXAuraObjWrapper*> textfield_children;
  ax_tree.GetChildren(textfield, &textfield_children);
  // The textfield has an extra child in Harmony, the focus ring.
  const size_t expected_children = 2;
  ASSERT_EQ(expected_children, textfield_children.size());

  ASSERT_EQ(content, textfield->GetParent());

  ASSERT_NE(textfield->GetUniqueId(), ax_tree.GetRoot()->GetUniqueId());

  // Try walking up the tree to the root.
  AXAuraObjWrapper* test_root = NULL;
  for (AXAuraObjWrapper* root_finder = ax_tree.GetParent(content); root_finder;
       root_finder = ax_tree.GetParent(root_finder))
    test_root = root_finder;
  ASSERT_EQ(ax_tree.GetRoot(), test_root);
}

TEST_F(AXTreeSourceViewsRootTest, DoDefault) {
  AXTreeSourceViews ax_tree(&root_wrapper_, ui::DesktopAXTreeID());

  // Grab a wrapper to |DoDefault| (click).
  AXAuraObjWrapper* textfield_wrapper =
      AXAuraObjCache::GetInstance()->GetOrCreate(textfield_);

  // Click and verify focus.
  ASSERT_FALSE(textfield_->HasFocus());
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kDoDefault;
  action_data.target_node_id = textfield_wrapper->GetUniqueId();
  textfield_wrapper->HandleAccessibleAction(action_data);
  ASSERT_TRUE(textfield_->HasFocus());
}

TEST_F(AXTreeSourceViewsRootTest, Focus) {
  AXTreeSourceViews ax_tree(&root_wrapper_, ui::DesktopAXTreeID());

  // Grab a wrapper to focus.
  AXAuraObjWrapper* textfield_wrapper =
      AXAuraObjCache::GetInstance()->GetOrCreate(textfield_);

  // Focus and verify.
  ASSERT_FALSE(textfield_->HasFocus());
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kFocus;
  action_data.target_node_id = textfield_wrapper->GetUniqueId();
  textfield_wrapper->HandleAccessibleAction(action_data);
  ASSERT_TRUE(textfield_->HasFocus());
}

TEST_F(AXTreeSourceViewsRootTest, Serialize) {
  AXTreeSourceViews ax_tree(&root_wrapper_, ui::DesktopAXTreeID());
  AuraAXTreeSerializer ax_serializer(&ax_tree);
  ui::AXTreeUpdate out_update;

  // This is the initial serialization.
  ax_serializer.SerializeChanges(ax_tree.GetRoot(), &out_update);

  // The update should just be the desktop node and the fake alert window we use
  // to handle posting text alerts.
  ASSERT_EQ(2U, out_update.nodes.size());

  // Try removing some child views and re-adding which should fire some events.
  content_->RemoveAllChildViews(false /* delete_children */);
  content_->AddChildView(textfield_);

  // Grab the textfield since serialization only walks up the tree (not down
  // from root).
  AXAuraObjWrapper* textfield_wrapper =
      AXAuraObjCache::GetInstance()->GetOrCreate(textfield_);

  // Now, re-serialize.
  ui::AXTreeUpdate out_update2;
  ax_serializer.SerializeChanges(textfield_wrapper, &out_update2);

  size_t node_count = out_update2.nodes.size();

  // We should have far more updates this time around.
  ASSERT_GE(node_count, 8U);

  int text_field_update_index = -1;
  for (size_t i = 0; i < node_count; ++i) {
    if (textfield_wrapper->GetUniqueId() == out_update2.nodes[i].id)
      text_field_update_index = i;
  }

  ASSERT_NE(-1, text_field_update_index);
  ASSERT_EQ(ax::mojom::Role::kTextField,
            out_update2.nodes[text_field_update_index].role);
}

TEST_F(AXTreeSourceViewsRootTest, SerializeWindowSetsClipsChildren) {
  AXTreeSourceViews ax_tree(&root_wrapper_, ui::DesktopAXTreeID());
  AuraAXTreeSerializer ax_serializer(&ax_tree);
  AXAuraObjWrapper* widget_wrapper =
      AXAuraObjCache::GetInstance()->GetOrCreate(widget_);
  ui::AXNodeData node_data;
  ax_tree.SerializeNode(widget_wrapper, &node_data);
  EXPECT_EQ(ax::mojom::Role::kWindow, node_data.role);
  bool clips_children = false;
  EXPECT_TRUE(node_data.GetBoolAttribute(
      ax::mojom::BoolAttribute::kClipsChildren, &clips_children));
  EXPECT_TRUE(clips_children);
}

}  // namespace
}  // namespace views

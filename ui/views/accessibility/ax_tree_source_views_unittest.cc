// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/views/accessibility/ax_tree_source_views.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

class AXTreeSourceViewsTest : public ViewsTestBase {
 public:
  AXTreeSourceViewsTest() {}
  virtual ~AXTreeSourceViewsTest() {}

  virtual void SetUp() OVERRIDE {
    ViewsTestBase::SetUp();
    widget_.reset(new Widget());
    Widget::InitParams init_params =
        CreateParams(Widget::InitParams::TYPE_POPUP);
    init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_->Init(init_params);

    content_.reset(new View());
    widget_->SetContentsView(content_.get());

    textfield_.reset(new Textfield());
    textfield_->SetText(base::ASCIIToUTF16("Value"));
    content_->AddChildView(textfield_.get());
  }

 protected:
  scoped_ptr<Widget> widget_;
  scoped_ptr<View> content_;
  scoped_ptr<Textfield> textfield_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AXTreeSourceViewsTest);
};

TEST_F(AXTreeSourceViewsTest, SimpleAccessers) {
  AXTreeSourceViews ax_tree(widget_.get());
  ASSERT_EQ(widget_->GetRootView(), ax_tree.GetRoot());
  std::vector<View*> content_children;
  ax_tree.GetChildren(content_.get(), &content_children);
  ASSERT_EQ(1U, content_children.size());
  std::vector<View*> textfield_children;
  ax_tree.GetChildren(textfield_.get(), &textfield_children);
  ASSERT_EQ(0U, textfield_children.size());

  int root_id = ax_tree.GetId(widget_->GetRootView());
  int content_id = ax_tree.GetId(content_.get());
  int textfield_id = ax_tree.GetId(textfield_.get());

  ASSERT_EQ(1, root_id);
  ASSERT_EQ(2, content_id);
  ASSERT_EQ(3, textfield_id);
}

TEST_F(AXTreeSourceViewsTest, SimpleSerialization) {
  AXTreeSourceViews ax_tree(widget_.get());
  ui::AXTreeSource<View*>* ax_source =
      static_cast<ui::AXTreeSource<View*>* >(&ax_tree);
  ui::AXTreeSerializer<View*> ax_serializer(ax_source);
  ui::AXTreeUpdate out_update;
  ax_serializer.SerializeChanges(ax_tree.GetRoot(), &out_update);
  ASSERT_EQ(3U, out_update.nodes.size());
  ASSERT_EQ("id=1 root_web_area (0, 0)-(0, 0) name= value= child_ids=2",
            out_update.nodes[0].ToString());
  ASSERT_EQ("id=2 client (0, 0)-(0, 0) name= value= child_ids=3",
            out_update.nodes[1].ToString());
  ASSERT_EQ("id=3 text_field (0, 0)-(0, 0) name= value=Value",
            out_update.nodes[2].ToString());
}

} // namespace views

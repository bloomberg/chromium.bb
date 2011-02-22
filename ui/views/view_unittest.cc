// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ui {

class ViewTest : public testing::Test {
 public:
  ViewTest() {}
  virtual ~ViewTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewTest);
};

TEST_F(ViewTest, SizeAndDisposition) {
  View v;
  EXPECT_TRUE(v.bounds().IsEmpty());
  EXPECT_TRUE(v.visible());

  v.SetBounds(gfx::Rect(10, 10, 200, 200));
  EXPECT_EQ(200, v.width());

  EXPECT_TRUE(v.GetPreferredSize().IsEmpty());
}

TEST_F(ViewTest, TreeOperations) {
  View v;
  EXPECT_EQ(NULL, v.GetWidget());
  EXPECT_TRUE(v.children_empty());

  View child1;
  v.AddChildView(&child1);
  EXPECT_EQ(1, v.children_size());
  EXPECT_EQ(&v, child1.parent());

  View child2;
  v.AddChildViewAt(&child2, 0);
  EXPECT_EQ(2, v.children_size());
  EXPECT_EQ(child1.parent(), child2.parent());

  v.RemoveChildView(&child2, false);
  EXPECT_EQ(1, v.children_size());
  EXPECT_EQ(NULL, child2.parent());

  //v.RemoveAllChildViews(false);
  //EXPECT_TRUE(v.children_empty());
}

class ObserverView : public View {
 public:
  ObserverView()
      : view_added_(false),
        view_removed_(false),
        subject_view_(NULL) {}
  virtual ~ObserverView() {}

  void ResetTestState() {
    view_added_ = false;
    view_removed_ = false;
    subject_view_ = NULL;
  }

  // Overridden from View:
  virtual void OnViewAdded(View* parent, View* child) {
    subject_view_ = child;
    view_added_ = true;
    view_removed_ = false;
  }
  virtual void OnViewRemoved(View* parent, View* child) {
    subject_view_ = child;
    view_removed_ = true;
    view_added_ = false;
  }

  bool view_added() const { return view_added_; }
  bool view_removed() const { return view_removed_; }
  View* subject_view() const { return subject_view_; }

 private:
  bool view_added_;
  bool view_removed_;
  View* subject_view_;

  DISALLOW_COPY_AND_ASSIGN(ObserverView);
};

class WidgetObserverView : public View {
 public:
  WidgetObserverView() : in_widget_(false) {}
  virtual ~WidgetObserverView() {}

  // Overridden from View:
  virtual void OnViewAddedToWidget() {
    in_widget_ = true;
  }
  virtual void OnViewRemovedFromWidget() {
    in_widget_ = false;
  }

  bool in_widget() const { return in_widget_; }

 private:
  bool in_widget_;

  DISALLOW_COPY_AND_ASSIGN(WidgetObserverView);
};

/*
TEST_F(ViewTest, HierarchyObserver) {
  ObserverView ov;
  Widget widget(&ov);
  widget.InitWithNativeViewParent(NULL, gfx::Rect(20, 20, 400, 400));

  // |ov|'s addition to the RootView's hierarchy should have caused these values
  // to be set.
  EXPECT_TRUE(ov.view_added());
  EXPECT_FALSE(ov.view_removed());
  EXPECT_EQ(&ov, ov.subject_view());

  ov.ResetTestState();

  // Direct descendants.
  View v2;
  ov.AddChildView(&v2);
  EXPECT_TRUE(ov.view_added());
  EXPECT_FALSE(ov.view_removed());
  EXPECT_EQ(&v2, ov.subject_view());

  ov.ResetTestState();

  // Nested Views and Widget addition.
  WidgetObserverView v3;
  EXPECT_FALSE(v3.in_widget());
  v2.AddChildView(&v3);
  EXPECT_TRUE(v3.in_widget());
  EXPECT_EQ(&widget, v3.GetWidget());
  EXPECT_TRUE(ov.view_added());
  EXPECT_FALSE(ov.view_removed());
  EXPECT_EQ(&v3, ov.subject_view());

  ov.ResetTestState();

  // Removal and Widget removal.
  ov.RemoveChildView(&v2);
  EXPECT_FALSE(ov.view_added());
  EXPECT_TRUE(ov.view_removed());
  EXPECT_EQ(&v2, ov.subject_view());

  EXPECT_FALSE(v3.in_widget());
  EXPECT_EQ(NULL, v3.GetWidget());
}
*/

TEST_F(ViewTest, IDs) {
  const int kV1ID = 1;
  const int kV2ID = 2;
  const int kV3ID = 3;
  const int kV4ID = 4;
  const int kV5ID = 5;
  const int kGroupID = 1;
  View v1;
  v1.set_id(kV1ID);
  View v2;
  v2.set_id(kV2ID);
  View v3;
  v3.set_id(kV3ID);
  v3.set_group(kGroupID);
  View v4;
  v4.set_id(kV4ID);
  v4.set_group(kGroupID);
  v1.AddChildView(&v2);
  v2.AddChildView(&v3);
  v2.AddChildView(&v4);

  EXPECT_EQ(&v4, v1.GetViewByID(kV4ID));
  EXPECT_EQ(&v1, v1.GetViewByID(kV1ID));
  EXPECT_EQ(NULL, v1.GetViewByID(kV5ID)); // No V5 exists.

  View::Views views;
  v1.GetViewsInGroup(kGroupID, &views);
  EXPECT_EQ(2, views.size());
  View::Views::const_iterator it = std::find(views.begin(), views.end(), &v3);
  EXPECT_NE(views.end(), it);
  it = std::find(views.begin(), views.end(), &v4);
  EXPECT_NE(views.end(), it);
}

TEST_F(ViewTest, EventHandlers) {

}

TEST_F(ViewTest, Painting) {

}

}  // namespace ui

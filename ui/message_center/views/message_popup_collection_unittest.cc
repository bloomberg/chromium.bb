// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_popup_collection.h"

#include <list>

#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace message_center {
namespace test {

class MessagePopupCollectionTest : public views::ViewsTestBase {
 public:
  virtual void SetUp() OVERRIDE {
    views::ViewsTestBase::SetUp();
    collection_.reset(new MessagePopupCollection(NULL, &message_center_));
    collection_->SetWorkAreaForTest(gfx::Rect(0, 0, 200, 200));
  }

  virtual void TearDown() OVERRIDE {
    collection_->CloseAllWidgets();
    views::ViewsTestBase::TearDown();
  }

 protected:
  void CreateWidgets(const std::vector<gfx::Rect>& rects) {
    for (std::vector<gfx::Rect>::const_iterator iter = rects.begin();
         iter != rects.end(); ++iter) {
      views::Widget* widget = new views::Widget();
      views::Widget::InitParams params = CreateParams(
          views::Widget::InitParams::TYPE_POPUP);
      params.keep_on_top = true;
      params.top_level = true;
      params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
      widget->Init(params);
      widget->SetBounds(*iter);
      collection_->widgets_.push_back(widget);
    }
  }

  void RepositionWidgets() {
    collection_->RepositionWidgets();
  }

  void RepositionWidgetsWithTarget(const gfx::Rect& target_rect) {
    collection_->RepositionWidgetsWithTarget(target_rect);
  }

  gfx::Rect GetWidgetRectAt(size_t index) {
    size_t i = 0;
    for (std::list<views::Widget*>::const_iterator iter =
             collection_->widgets_.begin();
         iter != collection_->widgets_.end(); ++iter) {
      if (i++ == index)
        return (*iter)->GetWindowBoundsInScreen();
    }
    return gfx::Rect();
  }

 private:
  FakeMessageCenter message_center_;
  scoped_ptr<MessagePopupCollection> collection_;
};

TEST_F(MessagePopupCollectionTest, RepositionWidgets) {
  std::vector<gfx::Rect> rects;
  std::list<views::Widget*> widgets;
  rects.push_back(gfx::Rect(0, 0, 10, 10));
  rects.push_back(gfx::Rect(0, 0, 10, 20));
  rects.push_back(gfx::Rect(0, 0, 10, 30));
  rects.push_back(gfx::Rect(0, 0, 10, 40));
  CreateWidgets(rects);
  RepositionWidgets();

  EXPECT_EQ("0,180 10x10", GetWidgetRectAt(0).ToString());
  EXPECT_EQ("0,150 10x20", GetWidgetRectAt(1).ToString());
  EXPECT_EQ("0,110 10x30", GetWidgetRectAt(2).ToString());
  EXPECT_EQ("0,60 10x40", GetWidgetRectAt(3).ToString());
}

TEST_F(MessagePopupCollectionTest, RepositionWidgetsWithTargetDown) {
  std::vector<gfx::Rect> rects;
  std::list<views::Widget*> widgets;
  rects.push_back(gfx::Rect(0, 180, 10, 10));
  rects.push_back(gfx::Rect(0, 150, 10, 20));
  rects.push_back(gfx::Rect(0, 60, 10, 40));
  CreateWidgets(rects);
  RepositionWidgetsWithTarget(gfx::Rect(0, 110, 10, 30));

  EXPECT_EQ("0,180 10x10", GetWidgetRectAt(0).ToString());
  EXPECT_EQ("0,150 10x20", GetWidgetRectAt(1).ToString());
  EXPECT_EQ("0,110 10x40", GetWidgetRectAt(2).ToString());
}

TEST_F(MessagePopupCollectionTest, RepositionWidgetsWithTargetDownAll) {
  std::vector<gfx::Rect> rects;
  std::list<views::Widget*> widgets;
  rects.push_back(gfx::Rect(0, 150, 10, 20));
  rects.push_back(gfx::Rect(0, 110, 10, 30));
  rects.push_back(gfx::Rect(0, 60, 10, 40));
  CreateWidgets(rects);
  RepositionWidgetsWithTarget(gfx::Rect(0, 180, 10, 10));

  EXPECT_EQ("0,180 10x20", GetWidgetRectAt(0).ToString());
  EXPECT_EQ("0,140 10x30", GetWidgetRectAt(1).ToString());
  EXPECT_EQ("0,90 10x40", GetWidgetRectAt(2).ToString());
}

TEST_F(MessagePopupCollectionTest, RepositionWidgetsWithTargetUp) {
  std::vector<gfx::Rect> rects;
  std::list<views::Widget*> widgets;
  rects.push_back(gfx::Rect(0, 180, 10, 10));
  rects.push_back(gfx::Rect(0, 150, 10, 20));
  rects.push_back(gfx::Rect(0, 110, 10, 30));
  CreateWidgets(rects);
  RepositionWidgetsWithTarget(gfx::Rect(0, 60, 10, 40));

  EXPECT_EQ("0,130 10x10", GetWidgetRectAt(0).ToString());
  EXPECT_EQ("0,100 10x20", GetWidgetRectAt(1).ToString());
  EXPECT_EQ("0,60 10x30", GetWidgetRectAt(2).ToString());
}

}  // namespace test
}  // namespace message_center

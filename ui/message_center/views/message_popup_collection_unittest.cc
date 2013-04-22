// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_popup_collection.h"

#include <list>

#include "base/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
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
    MessageCenter::Initialize();
    collection_.reset(
        new MessagePopupCollection(GetContext(), MessageCenter::Get()));
    collection_->SetWorkAreaForTest(gfx::Rect(0, 0, 1280, 1024));
    id_ = 0;
  }

  virtual void TearDown() OVERRIDE {
    collection_->CloseAllWidgets();
    collection_.reset();
    MessageCenter::Shutdown();
    views::ViewsTestBase::TearDown();
  }

 protected:
  size_t GetToastCounts() {
    return collection_->toasts_.size();
  }

  bool IsToastShown(const std::string& id) {
    views::Widget* widget = collection_->GetWidgetForId(id);
    return widget && widget->IsVisible();
  }

  std::string AddNotification() {
    std::string id = base::IntToString(id_++);
    MessageCenter::Get()->AddNotification(
        NOTIFICATION_TYPE_BASE_FORMAT, id, UTF8ToUTF16("test title"),
        UTF8ToUTF16("test message"), string16() /* display_source */,
        "" /* extension_id */, NULL);
    return id;
  }

 private:
  scoped_ptr<MessagePopupCollection> collection_;
  int id_;
};

class MessagePopupCollectionWidgetsTest : public views::ViewsTestBase {
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

  void SetRepositionTarget(const gfx::Rect& target_rect) {
    collection_->reposition_target_ = target_rect;
  }

  void RepositionWidgetsWithTarget(const gfx::Rect& target_rect) {
    SetRepositionTarget(target_rect);
    collection_->RepositionWidgetsWithTarget();
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

TEST_F(MessagePopupCollectionTest, DismissOnClick) {
  std::string id1 = AddNotification();
  std::string id2 = AddNotification();
  EXPECT_EQ(2u, GetToastCounts());
  EXPECT_TRUE(IsToastShown(id1));
  EXPECT_TRUE(IsToastShown(id2));

  MessageCenter::Get()->ClickOnNotification(id2);
  RunPendingMessages();
  EXPECT_EQ(1u, GetToastCounts());
  EXPECT_TRUE(IsToastShown(id1));
  EXPECT_FALSE(IsToastShown(id2));

  MessageCenter::Get()->ClickOnNotificationButton(id1, 0);
  RunPendingMessages();
  EXPECT_EQ(0u, GetToastCounts());
  EXPECT_FALSE(IsToastShown(id1));
  EXPECT_FALSE(IsToastShown(id2));
}

TEST_F(MessagePopupCollectionWidgetsTest, RepositionWidgets) {
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

TEST_F(MessagePopupCollectionWidgetsTest, RepositionWidgetsWithTargetDown) {
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

TEST_F(MessagePopupCollectionWidgetsTest, RepositionWidgetsWithTargetDownAll) {
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

TEST_F(MessagePopupCollectionWidgetsTest, RepositionWidgetsWithTargetUp) {
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

TEST_F(MessagePopupCollectionWidgetsTest, RepositionAfterSettingTarget) {
  std::vector<gfx::Rect> rects;
  std::list<views::Widget*> widgets;
  rects.push_back(gfx::Rect(0, 180, 10, 10));
  rects.push_back(gfx::Rect(0, 150, 10, 20));
  rects.push_back(gfx::Rect(0, 60, 10, 40));
  CreateWidgets(rects);
  SetRepositionTarget(gfx::Rect(0, 110, 10, 30));

  RepositionWidgets();
  EXPECT_EQ("0,180 10x10", GetWidgetRectAt(0).ToString());
  EXPECT_EQ("0,150 10x20", GetWidgetRectAt(1).ToString());
  EXPECT_EQ("0,110 10x40", GetWidgetRectAt(2).ToString());

  RepositionWidgets();
  EXPECT_EQ("0,180 10x10", GetWidgetRectAt(0).ToString());
  EXPECT_EQ("0,150 10x20", GetWidgetRectAt(1).ToString());
  EXPECT_EQ("0,110 10x40", GetWidgetRectAt(2).ToString());

  // Clear the target.
  SetRepositionTarget(gfx::Rect());
  RepositionWidgets();
  EXPECT_EQ("0,180 10x10", GetWidgetRectAt(0).ToString());
  EXPECT_EQ("0,150 10x20", GetWidgetRectAt(1).ToString());
  EXPECT_EQ("0,100 10x40", GetWidgetRectAt(2).ToString());

}

}  // namespace test
}  // namespace message_center

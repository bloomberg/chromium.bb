// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/layout/box_layout.h"
#include "views/view.h"

class StaticSizedView : public views::View {
 public:
  explicit StaticSizedView(const gfx::Size& size)
    : size_(size) { }

  virtual gfx::Size GetPreferredSize() {
    return size_;
  }

 private:
  gfx::Size size_;
};

class BoxLayoutTest : public testing::Test {
 public:
  virtual void SetUp() {
    host_.reset(new views::View);
  }

  scoped_ptr<views::View> host_;
  scoped_ptr<views::BoxLayout> layout_;
};

TEST_F(BoxLayoutTest, Empty) {
  layout_.reset(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 10, 10, 20));
  EXPECT_EQ(gfx::Size(20, 20), layout_->GetPreferredSize(host_.get()));
}

TEST_F(BoxLayoutTest, AlignmentHorizontal) {
  layout_.reset(new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
  views::View* v1 = new StaticSizedView(gfx::Size(10, 20));
  host_->AddChildView(v1);
  views::View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(20, 20), layout_->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 20, 20);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 20), v1->bounds());
  EXPECT_EQ(gfx::Rect(10, 0, 10, 20), v2->bounds());
}

TEST_F(BoxLayoutTest, AlignmentVertical) {
  layout_.reset(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  views::View* v1 = new StaticSizedView(gfx::Size(20, 10));
  host_->AddChildView(v1);
  views::View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(20, 20), layout_->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 20, 20);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(0, 0, 20, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 10, 20, 10), v2->bounds());
}

TEST_F(BoxLayoutTest, Spacing) {
  layout_.reset(new views::BoxLayout(views::BoxLayout::kHorizontal, 7, 7, 8));
  views::View* v1 = new StaticSizedView(gfx::Size(10, 20));
  host_->AddChildView(v1);
  views::View* v2 = new StaticSizedView(gfx::Size(10, 20));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(42, 34), layout_->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 100, 100);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(7, 7, 10, 86), v1->bounds());
  EXPECT_EQ(gfx::Rect(25, 7, 10, 86), v2->bounds());
}

TEST_F(BoxLayoutTest, Overflow) {
  layout_.reset(new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
  views::View* v1 = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(v1);
  views::View* v2 = new StaticSizedView(gfx::Size(10, 20));
  host_->AddChildView(v2);
  host_->SetBounds(0, 0, 10, 10);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(0, 0, 10, 10), v1->bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), v2->bounds());
}

TEST_F(BoxLayoutTest, NoSpace) {
  layout_.reset(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 10, 10, 10));
  views::View* childView = new StaticSizedView(gfx::Size(20, 20));
  host_->AddChildView(childView);
  host_->SetBounds(0, 0, 10, 10);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), childView->bounds());
}

TEST_F(BoxLayoutTest, InvisibleChild) {
  layout_.reset(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 10, 10, 10));
  views::View* v1 = new StaticSizedView(gfx::Size(20, 20));
  v1->SetVisible(false);
  host_->AddChildView(v1);
  views::View* v2 = new StaticSizedView(gfx::Size(10, 10));
  host_->AddChildView(v2);
  EXPECT_EQ(gfx::Size(30, 30), layout_->GetPreferredSize(host_.get()));
  host_->SetBounds(0, 0, 30, 30);
  layout_->Layout(host_.get());
  EXPECT_EQ(gfx::Rect(10, 10, 10, 10), v2->bounds());
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/slide_out_controller.h"

#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace message_center {

class SlideOutControllerDelegate : public SlideOutController::Delegate {
 public:
  explicit SlideOutControllerDelegate(views::View* target) : target_(target) {}
  virtual ~SlideOutControllerDelegate() = default;

  ui::Layer* GetSlideOutLayer() override { return target_->layer(); }

  void OnSlideStarted() override { ++slide_started_count_; }

  void OnSlideChanged(bool in_progress) override {
    slide_changed_last_value_ = in_progress;
    ++slide_changed_count_;
  }

  void OnSlideOut() override { ++slide_out_count_; }

  base::Optional<bool> slide_changed_last_value_;
  int slide_started_count_ = 0;
  int slide_changed_count_ = 0;
  int slide_out_count_ = 0;

 private:
  views::View* const target_;
};

class SlideOutControllerTest : public views::ViewsTestBase {
 public:
  SlideOutControllerTest() = default;
  ~SlideOutControllerTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    widget_ = std::make_unique<views::Widget>();

    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_POPUP);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(50, 50, 650, 650);
    widget_->Init(params);
    views::View* root = widget_->GetRootView();

    views::View* target_ = new views::View();
    target_->SetPaintToLayer(ui::LAYER_TEXTURED);
    target_->SetSize(gfx::Size(50, 50));

    root->AddChildView(target_);
    widget_->Show();

    delegate_ = std::make_unique<SlideOutControllerDelegate>(target_);
    slide_out_controller_ =
        std::make_unique<SlideOutController>(target_, delegate_.get());
  }

  void TearDown() override {
    slide_out_controller_.reset();
    delegate_.reset();
    widget_.reset();

    views::ViewsTestBase::TearDown();
  }

 protected:
  SlideOutController* slide_out_controller() {
    return slide_out_controller_.get();
  }

  SlideOutControllerDelegate* delegate() { return delegate_.get(); }

 private:
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<SlideOutController> slide_out_controller_;
  std::unique_ptr<SlideOutControllerDelegate> delegate_;
};

TEST_F(SlideOutControllerTest, OnGestureEventAndDelegate) {
  ui::GestureEvent scroll_begin(
      0, 0, ui::EF_NONE,
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(1),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));
  slide_out_controller()->OnGestureEvent(&scroll_begin);

  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_EQ(0, delegate()->slide_changed_count_);
  EXPECT_EQ(0, delegate()->slide_out_count_);

  ui::GestureEvent scroll_update(
      0, 0, ui::EF_NONE,
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(2),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE));
  slide_out_controller()->OnGestureEvent(&scroll_update);

  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_EQ(1, delegate()->slide_changed_count_);
  EXPECT_TRUE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);

  ui::GestureEvent scroll_end(
      0, 0, ui::EF_NONE,
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(3),
      ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));
  slide_out_controller()->OnGestureEvent(&scroll_end);

  EXPECT_EQ(1, delegate()->slide_started_count_);
  EXPECT_EQ(2, delegate()->slide_changed_count_);
  EXPECT_FALSE(delegate()->slide_changed_last_value_.value());
  EXPECT_EQ(0, delegate()->slide_out_count_);
}

}  // namespace message_center

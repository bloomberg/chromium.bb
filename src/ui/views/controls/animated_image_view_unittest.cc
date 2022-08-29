// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/animated_image_view.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/test/skia_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/lottie/animation.h"
#include "ui/views/paint_info.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

using ::testing::FloatEq;
using ::testing::NotNull;

template <typename T>
const T* FindPaintOp(const cc::PaintOpBuffer& paint_op_buffer,
                     cc::PaintOpType paint_op_type) {
  for (const cc::PaintOp* op : cc::PaintOpBuffer::Iterator(&paint_op_buffer)) {
    if (op->GetType() == paint_op_type)
      return static_cast<const T*>(op);

    if (op->GetType() == cc::PaintOpType::DrawRecord) {
      const T* record_op_result = FindPaintOp<T>(
          *static_cast<const cc::DrawRecordOp*>(op)->record, paint_op_type);
      if (record_op_result)
        return static_cast<const T*>(record_op_result);
    }
  }
  return nullptr;
}

class AnimatedImageViewTest : public ViewsTestBase {
 protected:
  static constexpr int kDefaultWidthAndHeight = 100;
  static constexpr gfx::Size kDefaultSize =
      gfx::Size(kDefaultWidthAndHeight, kDefaultWidthAndHeight);

  void SetUp() override {
    ViewsTestBase::SetUp();

    Widget::InitParams params =
        CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(kDefaultSize);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_.Init(std::move(params));

    auto view = std::make_unique<AnimatedImageView>();
    view->SetUseDefaultFillLayout(true);
    view_ = view.get();
    widget_.SetContentsView(std::move(view));

    widget_.Show();
  }

  void TearDown() override {
    widget_.Close();
    ViewsTestBase::TearDown();
  }

  std::unique_ptr<lottie::Animation> CreateAnimationWithSize(
      const gfx::Size& size) {
    return std::make_unique<lottie::Animation>(
        cc::CreateSkottie(size, /*duration_secs=*/1));
  }

  sk_sp<cc::PaintRecord> Paint(const gfx::Rect& invalidation_rect) {
    auto display_list = base::MakeRefCounted<cc::DisplayItemList>(
        cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer);
    ui::PaintContext paint_context(display_list.get(),
                                   /*device_scale_factor=*/1.f,
                                   invalidation_rect, /*is_pixel_canvas=*/true);
    view_->Paint(PaintInfo::CreateRootPaintInfo(paint_context,
                                                invalidation_rect.size()));
    RunPendingMessages();
    return display_list->ReleaseAsRecord();
  }

  Widget widget_;
  raw_ptr<AnimatedImageView> view_;
};

TEST_F(AnimatedImageViewTest, PaintsWithAdditionalTranslation) {
  view_->SetAnimatedImage(CreateAnimationWithSize(gfx::Size(80, 80)));
  view_->SetVerticalAlignment(ImageViewBase::Alignment::kCenter);
  view_->SetHorizontalAlignment(ImageViewBase::Alignment::kCenter);
  widget_.GetContentsView()->Layout();
  view_->Play();

  static constexpr float kExpectedDefaultOrigin =
      (kDefaultWidthAndHeight - 80) / 2;

  // Default should be no extra translation.
  sk_sp<cc::PaintRecord> paint_op_buffer = Paint(view_->bounds());
  const cc::TranslateOp* translate_op = FindPaintOp<cc::TranslateOp>(
      *paint_op_buffer, cc::PaintOpType::Translate);
  ASSERT_THAT(translate_op, NotNull());
  EXPECT_THAT(translate_op->dx, FloatEq(kExpectedDefaultOrigin));
  EXPECT_THAT(translate_op->dy, FloatEq(kExpectedDefaultOrigin));

  view_->SetAdditionalTranslation(gfx::Vector2d(5, 5));
  paint_op_buffer = Paint(view_->bounds());
  translate_op = FindPaintOp<cc::TranslateOp>(*paint_op_buffer,
                                              cc::PaintOpType::Translate);
  ASSERT_THAT(translate_op, NotNull());
  EXPECT_THAT(translate_op->dx, FloatEq(kExpectedDefaultOrigin + 5));
  EXPECT_THAT(translate_op->dy, FloatEq(kExpectedDefaultOrigin + 5));

  view_->SetAdditionalTranslation(gfx::Vector2d(5, -5));
  paint_op_buffer = Paint(view_->bounds());
  translate_op = FindPaintOp<cc::TranslateOp>(*paint_op_buffer,
                                              cc::PaintOpType::Translate);
  ASSERT_THAT(translate_op, NotNull());
  EXPECT_THAT(translate_op->dx, FloatEq(kExpectedDefaultOrigin + 5));
  EXPECT_THAT(translate_op->dy, FloatEq(kExpectedDefaultOrigin - 5));

  view_->SetAdditionalTranslation(gfx::Vector2d(-5, 5));
  paint_op_buffer = Paint(view_->bounds());
  translate_op = FindPaintOp<cc::TranslateOp>(*paint_op_buffer,
                                              cc::PaintOpType::Translate);
  ASSERT_THAT(translate_op, NotNull());
  EXPECT_THAT(translate_op->dx, FloatEq(kExpectedDefaultOrigin - 5));
  EXPECT_THAT(translate_op->dy, FloatEq(kExpectedDefaultOrigin + 5));

  view_->SetAdditionalTranslation(gfx::Vector2d(-5, -5));
  paint_op_buffer = Paint(view_->bounds());
  translate_op = FindPaintOp<cc::TranslateOp>(*paint_op_buffer,
                                              cc::PaintOpType::Translate);
  ASSERT_THAT(translate_op, NotNull());
  EXPECT_THAT(translate_op->dx, FloatEq(kExpectedDefaultOrigin - 5));
  EXPECT_THAT(translate_op->dy, FloatEq(kExpectedDefaultOrigin - 5));
}

}  // namespace
}  // namespace views

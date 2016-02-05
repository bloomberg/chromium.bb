// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/ime/mode_indicator_view.h"

#include "base/logging.h"
#include "base/macros.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/wm/core/window_animations.h"

namespace ui {
namespace ime {

namespace {

// Minimum size of inner contents in pixel.
// 43 is the designed size including the default margin (6 * 2).
const int kMinSize = 31;

// After this duration in msec, the mode inicator will be fading out.
const int kShowingDuration = 500;

class ModeIndicatorFrameView : public views::BubbleFrameView {
 public:
  explicit ModeIndicatorFrameView(const gfx::Insets& content_margins)
      : views::BubbleFrameView(gfx::Insets(), content_margins) {}
  ~ModeIndicatorFrameView() override {}

 private:
  // views::BubbleFrameView overrides:
  gfx::Rect GetAvailableScreenBounds(const gfx::Rect& rect) const override {
    return gfx::Screen::GetScreen()
        ->GetDisplayNearestPoint(rect.CenterPoint())
        .bounds();
  }

  DISALLOW_COPY_AND_ASSIGN(ModeIndicatorFrameView);
};

}  // namespace


ModeIndicatorView::ModeIndicatorView(gfx::NativeView parent,
                                     const gfx::Rect& cursor_bounds,
                                     const base::string16& label)
    : cursor_bounds_(cursor_bounds),
      label_view_(new views::Label(label)) {
  set_can_activate(false);
  set_accept_events(false);
  set_parent_window(parent);
  set_shadow(views::BubbleBorder::NO_SHADOW);
  set_arrow(views::BubbleBorder::TOP_CENTER);
}

ModeIndicatorView::~ModeIndicatorView() {}

void ModeIndicatorView::ShowAndFadeOut() {
  wm::SetWindowVisibilityAnimationTransition(
      GetWidget()->GetNativeView(),
      wm::ANIMATE_HIDE);
  GetWidget()->Show();
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kShowingDuration),
               GetWidget(),
               &views::Widget::Close);
}

gfx::Size ModeIndicatorView::GetPreferredSize() const {
  gfx::Size size = label_view_->GetPreferredSize();
  size.SetToMax(gfx::Size(kMinSize, kMinSize));
  return size;
}

const char* ModeIndicatorView::GetClassName() const {
  return "ModeIndicatorView";
}

void ModeIndicatorView::Init() {
  SetLayoutManager(new views::FillLayout());
  AddChildView(label_view_);

  SetAnchorRect(cursor_bounds_);
}

views::NonClientFrameView* ModeIndicatorView::CreateNonClientFrameView(
    views::Widget* widget) {
  views::BubbleFrameView* frame = new ModeIndicatorFrameView(margins());
  // arrow adjustment in BubbleDelegateView is unnecessary because arrow
  // of this bubble is always center.
  frame->SetBubbleBorder(scoped_ptr<views::BubbleBorder>(
      new views::BubbleBorder(arrow(), shadow(), color())));
  return frame;
}

}  // namespace ime
}  // namespace ui

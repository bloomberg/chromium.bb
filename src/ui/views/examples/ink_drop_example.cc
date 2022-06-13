// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/ink_drop_example.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace views {
namespace examples {

InkDropExample::InkDropExample() : ExampleBase("FloodFill Ink Drop") {}

InkDropExample::InkDropExample(const char* title) : ExampleBase(title) {}

InkDropExample::~InkDropExample() = default;

void InkDropExample::CreateExampleView(View* container) {
  BoxLayoutView* box_layout_view = nullptr;
  auto get_callback = [this](InkDropState state) {
    return base::BindRepeating(&InkDropExample::SetInkDropState,
                               base::Unretained(this), state);
  };
  Builder<View>(container)
      .SetUseDefaultFillLayout(true)
      .AddChild(
          Builder<BoxLayoutView>()
              .CopyAddressTo(&box_layout_view)
              .SetOrientation(BoxLayout::Orientation::kVertical)
              .SetMainAxisAlignment(BoxLayout::MainAxisAlignment::kEnd)
              .AddChildren(
                  Builder<View>()
                      .CopyAddressTo(&ink_drop_view_)
                      .SetBorder(CreateRoundedRectBorder(1, 4, SK_ColorBLACK))
                      .SetProperty(kMarginsKey, gfx::Insets(10)),
                  Builder<BoxLayoutView>()
                      .SetOrientation(BoxLayout::Orientation::kHorizontal)
                      .SetCrossAxisAlignment(
                          BoxLayout::CrossAxisAlignment::kCenter)
                      .SetPreferredSize(gfx::Size(0, 50))
                      .AddChildren(
                          Builder<MdTextButton>()
                              .SetText(base::ASCIIToUTF16(
                                  ToString(InkDropState::HIDDEN)))
                              .SetCallback(get_callback(InkDropState::HIDDEN))
                              .SetProperty(kMarginsKey, gfx::Insets(0, 5)),
                          Builder<MdTextButton>()
                              .SetText(base::ASCIIToUTF16(
                                  ToString(InkDropState::ACTION_PENDING)))
                              .SetCallback(
                                  get_callback(InkDropState::ACTION_PENDING))
                              .SetProperty(kMarginsKey, gfx::Insets(0, 5)),
                          Builder<MdTextButton>()
                              .SetText(base::ASCIIToUTF16(
                                  ToString(InkDropState::ACTION_TRIGGERED)))
                              .SetCallback(
                                  get_callback(InkDropState::ACTION_TRIGGERED))
                              .SetProperty(kMarginsKey, gfx::Insets(0, 5)),
                          Builder<MdTextButton>()
                              .SetText(base::ASCIIToUTF16(ToString(
                                  InkDropState::ALTERNATE_ACTION_PENDING)))
                              .SetCallback(get_callback(
                                  InkDropState::ALTERNATE_ACTION_PENDING))
                              .SetProperty(kMarginsKey, gfx::Insets(0, 5)),
                          Builder<MdTextButton>()
                              .SetText(base::ASCIIToUTF16(ToString(
                                  InkDropState::ALTERNATE_ACTION_TRIGGERED)))
                              .SetCallback(get_callback(
                                  InkDropState::ALTERNATE_ACTION_TRIGGERED))
                              .SetProperty(kMarginsKey, gfx::Insets(0, 5)),
                          Builder<MdTextButton>()
                              .SetText(base::ASCIIToUTF16(
                                  ToString(InkDropState::ACTIVATED)))
                              .SetCallback(
                                  get_callback(InkDropState::ACTIVATED))
                              .SetProperty(kMarginsKey, gfx::Insets(0, 5)),
                          Builder<MdTextButton>()
                              .SetText(base::ASCIIToUTF16(
                                  ToString(InkDropState::DEACTIVATED)))
                              .SetCallback(
                                  get_callback(InkDropState::DEACTIVATED))
                              .SetProperty(kMarginsKey, gfx::Insets(0, 5)))))
      .BuildChildren();
  box_layout_view->SetFlexForView(ink_drop_view_, 1);
  CreateInkDrop();
}

void InkDropExample::CreateInkDrop() {
  auto ink_drop_host = std::make_unique<InkDropHost>(ink_drop_view_);
  ink_drop_host->SetBaseColor(SK_ColorBLACK);
  ink_drop_host->SetMode(InkDropHost::InkDropMode::ON);
  InkDrop::Install(ink_drop_view_, std::move(ink_drop_host));
}

void InkDropExample::SetInkDropState(InkDropState state) {
  ui::MouseEvent event(
      ui::ET_MOUSE_PRESSED,
      gfx::PointF(ink_drop_view_->GetLocalBounds().CenterPoint()),
      gfx::PointF(ink_drop_view_->origin()), base::TimeTicks(), 0, 0);
  ui::ScopedAnimationDurationScaleMode scale(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
  InkDrop::Get(ink_drop_view_)->AnimateToState(state, &event);
}

}  // namespace examples
}  // namespace views

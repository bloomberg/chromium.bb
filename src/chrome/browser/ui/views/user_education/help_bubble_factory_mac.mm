// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/help_bubble_factory_mac.h"

#include "chrome/browser/ui/user_education/help_bubble_params.h"
#include "chrome/browser/ui/views/user_education/help_bubble_factory_views.h"
#include "chrome/browser/ui/views/user_education/help_bubble_view.h"
#include "ui/base/interaction/element_tracker_mac.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget.h"

DEFINE_FRAMEWORK_SPECIFIC_METADATA(HelpBubbleFactoryMac)

HelpBubbleFactoryMac::HelpBubbleFactoryMac() = default;
HelpBubbleFactoryMac::~HelpBubbleFactoryMac() = default;

std::unique_ptr<HelpBubble> HelpBubbleFactoryMac::CreateBubble(
    ui::TrackedElement* element,
    HelpBubbleParams params) {
  auto* const element_mac = element->AsA<ui::TrackedElementMac>();
  views::Widget* const widget =
      views::ElementTrackerViews::GetInstance()->GetWidgetForContext(
          element_mac->context());
  auto* const anchor_view = widget->GetRootView();
  gfx::Rect anchor_rect = element_mac->screen_bounds();

  // We don't want the bubble to be flush with either the side or top of the
  // Mac native menu, because it looks funny.
  constexpr gfx::Insets kMacMenuInsets(10, -5);
  anchor_rect.Inset(kMacMenuInsets);

  return base::WrapUnique(new HelpBubbleViews(
      new HelpBubbleView(anchor_view, std::move(params), anchor_rect)));
}

bool HelpBubbleFactoryMac::CanBuildBubbleForTrackedElement(
    const ui::TrackedElement* element) const {
  return element->IsA<ui::TrackedElementMac>();
}

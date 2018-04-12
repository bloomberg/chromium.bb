// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/assistant_container_view.h"

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/app_list/views/assistant_bubble_view.h"
#include "ui/app_list/views/contents_view.h"

namespace app_list {

namespace {

constexpr int kPaddingDip = 16;

}  // namespace

AssistantContainerView::AssistantContainerView(ContentsView* contents_view)
    : contents_view_(contents_view),
      assistant_bubble_view_(new AssistantBubbleView(
          contents_view->app_list_view()->assistant_interaction_model())) {
  AddChildView(assistant_bubble_view_);
}

gfx::Size AssistantContainerView::CalculatePreferredSize() const {
  if (!GetWidget()) {
    return gfx::Size();
  }
  return gfx::Size(contents_view_->GetDisplayWidth(),
                   contents_view_->GetDisplayHeight());
}

void AssistantContainerView::ChildPreferredSizeChanged(views::View* child) {
  // TODO(dmblack): Animate layout changes.
  Layout();
  SchedulePaint();
}

void AssistantContainerView::Layout() {
  // Pin bubble view to bottom lefthand corner.
  gfx::Size size = assistant_bubble_view_->GetPreferredSize();
  assistant_bubble_view_->SetBounds(kPaddingDip,
                                    height() - size.height() - kPaddingDip,
                                    size.width(), size.height());
}

bool AssistantContainerView::ShouldShowSearchBox() const {
  return false;
}

}  // namespace app_list

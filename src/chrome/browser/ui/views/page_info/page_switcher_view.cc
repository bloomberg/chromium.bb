// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_info/page_switcher_view.h"

#include "chrome/browser/ui/views/page_info/page_info_view_factory.h"
#include "ui/views/layout/fill_layout.h"

PageSwitcherView::PageSwitcherView(std::unique_ptr<views::View> initial_page) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  current_page_ = AddChildView(std::move(initial_page));
  current_page_->SetID(PageInfoViewFactory::VIEW_ID_PAGE_INFO_CURRENT_VIEW);
}

PageSwitcherView::~PageSwitcherView() = default;

void PageSwitcherView::SwitchToPage(std::unique_ptr<views::View> page) {
  if (current_page_)
    RemoveChildViewT(current_page_.get());
  current_page_ = AddChildView(std::move(page));
  current_page_->SetID(PageInfoViewFactory::VIEW_ID_PAGE_INFO_CURRENT_VIEW);
  PreferredSizeChanged();
}

void PageSwitcherView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

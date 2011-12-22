// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/app_list/app_list_groups_view.h"

#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura_shell/app_list/app_list_item_group_model.h"
#include "ui/aura_shell/app_list/app_list_item_group_view.h"
#include "ui/aura_shell/app_list/app_list_item_view.h"
#include "ui/aura_shell/app_list/app_list_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/layout/box_layout.h"

namespace aura_shell {

namespace {

const SkColor kPageHeaderColor = SkColorSetARGB(0xFF, 0xB2, 0xB2, 0xB2);
const SkColor kSelectedPageHeaderColor = SK_ColorWHITE;

// Creates page headers view that hosts page title buttons.
views::View* CreatePageHeader() {
  views::View* header = new views::View;
  header->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
  return header;
}

// Creates page header button view that shows page title.
views::View* CreatePageHeaderButton(views::ButtonListener* listener,
                                    const std::string& title ) {
  views::TextButton* button = new views::TextButton(listener,
        UTF8ToUTF16(title));

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  button->SetFont(rb.GetFont(ResourceBundle::BaseFont).DeriveFont(
      2, gfx::Font::BOLD));
  button->SetEnabledColor(kPageHeaderColor);
  return button;
}

// Gets preferred bounds of buttons and page.
void GetPageAndHeaderBounds(views::View* parent,
                            views::View* buttons,
                            views::View* page,
                            gfx::Rect* buttons_bounds,
                            gfx::Rect* page_bounds) {
  gfx::Rect content_bounds = parent->GetContentsBounds();

  if (buttons) {
    gfx::Size buttons_size = buttons->GetPreferredSize();
    if (buttons_bounds) {
      buttons_bounds->SetRect(
          (content_bounds.width() - buttons_size.width()) / 2,
          content_bounds.bottom() - buttons_size.height(),
          buttons_size.width(), buttons_size.height());
    }

    content_bounds.set_height(
        std::max(0, content_bounds.height() - buttons_size.height()));
  }

  if (page_bounds) {
    gfx::Size page_size = page->GetPreferredSize();
    *page_bounds = content_bounds.Center(page_size);
  }
}

}  // namespace

AppListGroupsView::AppListGroupsView(AppListModel* model,
                                     AppListItemViewListener* listener)
    : model_(model),
      listener_(listener),
      page_buttons_(NULL),
      current_page_(0) {
  animator_.reset(new views::BoundsAnimator(this));
  model_->AddObserver(this);
  Update();
}

AppListGroupsView::~AppListGroupsView() {
  model_->RemoveObserver(this);
}

views::View* AppListGroupsView::GetFocusedTile() const {
  AppListItemGroupView* page = GetCurrentPageView();
  return page ? page->GetFocusedTile() : NULL;
}

void AppListGroupsView::Update() {
  current_page_ = 0;
  page_buttons_ = NULL;
  RemoveAllChildViews(true);

  int page_count = model_->group_count();
  if (!page_count)
    return;

  if (page_count > 1)
    AddChildView(page_buttons_ = CreatePageHeader());

  for (int i = 0; i < page_count; ++i) {
    AppListItemGroupModel* group = model_->GetGroup(i);
    AddPage(group->title(), new AppListItemGroupView(group, listener_));
  }

  if (!size().IsEmpty())
    Layout();
  SetCurrentPage(0);
}

void AppListGroupsView::AddPage(const std::string& title,
                                AppListItemGroupView* page) {
  pages_.push_back(page);
  AddChildView(page);

  if (page_buttons_)
    page_buttons_->AddChildView(CreatePageHeaderButton(this, title));
}

int AppListGroupsView::GetPreferredTilesPerRow() const {
  return std::max(1, width() / AppListItemView::kTileSize);
}

AppListItemGroupView* AppListGroupsView::GetCurrentPageView() const {
  return static_cast<size_t>(current_page_) < pages_.size() ?
      pages_[current_page_] : NULL;
}

void AppListGroupsView::SetCurrentPage(int page) {
  int previous_page = current_page_;
  current_page_ = page;

 // Updates page buttons.
  if (page_buttons_) {
    for (int i = 0; i < page_buttons_->child_count(); ++i) {
      views::TextButton* button = static_cast<views::TextButton*>(
          page_buttons_->child_at(i));

      button->SetEnabledColor(i == current_page_ ?
          kSelectedPageHeaderColor : kPageHeaderColor);
    }
    page_buttons_->SchedulePaint();
  }

  // Gets sliding animation direction.
  int dir = previous_page < current_page_ ? -1 :
      previous_page > current_page_ ? 1 : 0;

  // Skips animation if no sliding needed or no valid size.
  if (dir == 0 || size().IsEmpty())
    return;

  animator_->Cancel();

  // Makes sure new page has correct layout and focus to its focused tile.
  AppListItemGroupView* current_view = GetCurrentPageView();
  current_view->SetTilesPerRow(GetPreferredTilesPerRow());
  views::View* tile = current_view->GetFocusedTile();
  if (tile)
    tile->RequestFocus();

  // Prepares current page before animation.
  gfx::Rect current_page_bounds;
  GetPageAndHeaderBounds(this, page_buttons_, current_view,
      NULL, &current_page_bounds);
  current_page_bounds.Offset(- dir * width(), 0);
  current_view->SetBoundsRect(current_page_bounds);

  // Schedules animations to slide out previous page and slide in current page.
  AppListItemGroupView* previous_view = pages_[previous_page];
  gfx::Rect previous_page_bounds = previous_view->bounds();
  previous_page_bounds.Offset(dir * width(), 0);
  animator_->AnimateViewTo(previous_view, previous_page_bounds);

  current_page_bounds.Offset(dir * width(), 0);
  animator_->AnimateViewTo(current_view, current_page_bounds);
}

void AppListGroupsView::Layout() {
  AppListItemGroupView* page = GetCurrentPageView();
  if (!page)
    return;

  page->SetTilesPerRow(GetPreferredTilesPerRow());

  gfx::Rect buttons_bounds;
  gfx::Rect page_bounds;
  GetPageAndHeaderBounds(this, page_buttons_, page,
      &buttons_bounds, &page_bounds);

  if (page_buttons_)
    page_buttons_->SetBoundsRect(buttons_bounds);

  page->SetBoundsRect(page_bounds);
}

bool AppListGroupsView::OnKeyPressed(const views::KeyEvent& event) {
  if (event.IsControlDown()) {
    switch (event.key_code()) {
      case ui::VKEY_LEFT:
        if (current_page_ > 0)
          SetCurrentPage(current_page_ - 1);
        return true;
      case ui::VKEY_RIGHT:
        if (static_cast<size_t>(current_page_ + 1) < pages_.size())
          SetCurrentPage(current_page_ + 1);
        return true;
      default:
        break;
    }
  }

  return false;
}

void AppListGroupsView::ButtonPressed(views::Button* sender,
                                      const views::Event& event) {
  DCHECK(page_buttons_);
  for (int i = 0; i < page_buttons_->child_count(); ++i) {
    if (page_buttons_->child_at(i) == sender)
      SetCurrentPage(i);
  }
}

void AppListGroupsView::ListItemsAdded(int start, int count) {
  Update();
}

void AppListGroupsView::ListItemsRemoved(int start, int count) {
  Update();
}

void AppListGroupsView::ListItemsChanged(int start, int count) {
  NOTREACHED();
}

}  // namespace aura_shell

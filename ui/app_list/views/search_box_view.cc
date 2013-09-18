// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_box_view.h"

#include <algorithm>

#include "grit/ui_resources.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/views/app_list_menu_views.h"
#include "ui/app_list/views/search_box_view_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"

namespace app_list {

namespace {

const int kPadding = 14;
const int kIconDimension = 32;
const int kPreferredWidth = 360;
const int kPreferredHeight = 48;
const int kMenuButtonDimension = 29;

const SkColor kHintTextColor = SkColorSetRGB(0xA0, 0xA0, 0xA0);

// Menu offset relative to the bottom-right corner of the menu button.
const int kMenuYOffsetFromButton = -4;
const int kMenuXOffsetFromButton = -7;

}  // namespace

SearchBoxView::SearchBoxView(SearchBoxViewDelegate* delegate,
                             AppListViewDelegate* view_delegate,
                             AppListModel* model)
    : delegate_(delegate),
      view_delegate_(view_delegate),
      model_(model),
      icon_view_(new views::ImageView),
      search_box_(new views::Textfield),
      contents_view_(NULL) {
  DCHECK(model_);
  AddChildView(icon_view_);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

#if !defined(OS_CHROMEOS)
  menu_button_ = new views::MenuButton(NULL, base::string16(), this, false);
  menu_button_->set_border(NULL);
  menu_button_->SetIcon(*rb.GetImageSkiaNamed(IDR_APP_LIST_TOOLS_NORMAL));
  menu_button_->SetHoverIcon(*rb.GetImageSkiaNamed(IDR_APP_LIST_TOOLS_HOVER));
  menu_button_->SetPushedIcon(*rb.GetImageSkiaNamed(
      IDR_APP_LIST_TOOLS_PRESSED));
  AddChildView(menu_button_);
#endif

  search_box_->RemoveBorder();
  search_box_->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
  search_box_->set_placeholder_text_color(kHintTextColor);
  search_box_->SetController(this);
  AddChildView(search_box_);

  model_->search_box()->AddObserver(this);
  IconChanged();
  HintTextChanged();
}

SearchBoxView::~SearchBoxView() {
  model_->search_box()->RemoveObserver(this);
}

bool SearchBoxView::HasSearch() const {
  return !search_box_->text().empty();
}

void SearchBoxView::ClearSearch() {
  search_box_->SetText(base::string16());
  // Updates model and fires query changed manually because SetText() above
  // does not generate ContentsChanged() notification.
  UpdateModel();
  NotifyQueryChanged();
}

void SearchBoxView::InvalidateMenu() {
  menu_.reset();
}

gfx::Size SearchBoxView::GetPreferredSize() {
  return gfx::Size(kPreferredWidth, kPreferredHeight);
}

void SearchBoxView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Rect icon_frame(rect);
  icon_frame.set_width(kIconDimension + 2 * kPadding);
  icon_view_->SetBoundsRect(icon_frame);

  gfx::Rect menu_button_frame(rect);
#if !defined(OS_CHROMEOS)
  menu_button_frame.set_width(kMenuButtonDimension);
  menu_button_frame.set_x(rect.right() - menu_button_frame.width() - kPadding);
  menu_button_frame.ClampToCenteredSize(gfx::Size(menu_button_frame.width(),
                                                  kMenuButtonDimension));
  menu_button_->SetBoundsRect(menu_button_frame);
#else
  menu_button_frame.set_width(0);
#endif

  gfx::Rect edit_frame(rect);
  edit_frame.set_x(icon_frame.right());
  edit_frame.set_width(
      rect.width() - icon_frame.width() - kPadding - menu_button_frame.width());
  edit_frame.ClampToCenteredSize(
      gfx::Size(edit_frame.width(), search_box_->GetPreferredSize().height()));
  search_box_->SetBoundsRect(edit_frame);
}

bool SearchBoxView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  if (contents_view_)
    return contents_view_->OnMouseWheel(event);

  return false;
}

void SearchBoxView::UpdateModel() {
  // Temporarily remove from observer to ignore notifications caused by us.
  model_->search_box()->RemoveObserver(this);
  model_->search_box()->SetText(search_box_->text());
  model_->search_box()->SetSelectionModel(search_box_->GetSelectionModel());
  model_->search_box()->AddObserver(this);
}

void SearchBoxView::NotifyQueryChanged() {
  DCHECK(delegate_);
  delegate_->QueryChanged(this);
}

void SearchBoxView::ContentsChanged(views::Textfield* sender,
                                    const base::string16& new_contents) {
  UpdateModel();
  NotifyQueryChanged();
}

bool SearchBoxView::HandleKeyEvent(views::Textfield* sender,
                                   const ui::KeyEvent& key_event) {
  bool handled = false;
  if (contents_view_ && contents_view_->visible())
    handled = contents_view_->OnKeyPressed(key_event);

  return handled;
}

void SearchBoxView::OnMenuButtonClicked(View* source, const gfx::Point& point) {
  if (!menu_)
    menu_.reset(new AppListMenuViews(view_delegate_, model_));

  const gfx::Point menu_location =
      menu_button_->GetBoundsInScreen().bottom_right() +
      gfx::Vector2d(kMenuXOffsetFromButton, kMenuYOffsetFromButton);
  menu_->RunMenuAt(menu_button_, menu_location);
}

void SearchBoxView::IconChanged() {
  icon_view_->SetImage(model_->search_box()->icon());
}

void SearchBoxView::HintTextChanged() {
  search_box_->set_placeholder_text(model_->search_box()->hint_text());
}

void SearchBoxView::SelectionModelChanged() {
  search_box_->SelectSelectionModel(model_->search_box()->selection_model());
}

void SearchBoxView::TextChanged() {
  search_box_->SetText(model_->search_box()->text());
  NotifyQueryChanged();
}

}  // namespace app_list

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_box_view.h"

#include <algorithm>

#include "ui/app_list/search_box_model.h"
#include "ui/app_list/search_box_view_delegate.h"
#include "ui/base/events/event.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"

namespace app_list {

namespace {

const int kPadding = 14;
const int kIconDimension = 32;
const int kPreferredWidth = 360;
const int kPreferredHeight = 48;
const int kEditHeight = 19;

const SkColor kHintTextColor = SkColorSetRGB(0xA0, 0xA0, 0xA0);

}  // namespace

SearchBoxView::SearchBoxView(SearchBoxViewDelegate* delegate)
    : delegate_(delegate),
      model_(NULL),
      icon_view_(new views::ImageView),
      search_box_(new views::Textfield),
      contents_view_(NULL) {
  AddChildView(icon_view_);

  search_box_->RemoveBorder();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  search_box_->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
  search_box_->set_placeholder_text_color(kHintTextColor);
  search_box_->SetController(this);
  AddChildView(search_box_);
}

SearchBoxView::~SearchBoxView() {
  if (model_)
    model_->RemoveObserver(this);
}

void SearchBoxView::SetModel(SearchBoxModel* model) {
  if (model_ == model)
    return;

  if (model_)
    model_->RemoveObserver(this);

  model_ = model;
  if (model_) {
    model_->AddObserver(this);
    IconChanged();
    HintTextChanged();
  }
}

gfx::Size SearchBoxView::GetPreferredSize() {
  return gfx::Size(kPreferredWidth, kPreferredHeight);
}

void SearchBoxView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Size icon_size(kIconDimension, kIconDimension);
  gfx::Rect icon_frame(rect);
  icon_frame.set_width(icon_size.width() + 2 * kPadding);
  icon_view_->SetBoundsRect(icon_frame);

  gfx::Rect edit_frame(rect);
  edit_frame.set_x(icon_frame.right());
  edit_frame.set_width(rect.width() - icon_frame.width() - kPadding);
  edit_frame.ClampToCenteredSize(gfx::Size(edit_frame.width(), kEditHeight));
  search_box_->SetBoundsRect(edit_frame);
}

bool SearchBoxView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  if (contents_view_)
    return contents_view_->OnMouseWheel(event);

  return false;
}

void SearchBoxView::UpdateModel() {
  // Temporarily remove from observer to ignore notifications caused by us.
  model_->RemoveObserver(this);
  model_->SetText(search_box_->text());
  model_->SetSelectionModel(search_box_->GetSelectionModel());
  model_->AddObserver(this);
}

void SearchBoxView::NotifyQueryChanged() {
  DCHECK(delegate_);
  delegate_->QueryChanged(this);
}

void SearchBoxView::ContentsChanged(views::Textfield* sender,
                                    const string16& new_contents) {
  UpdateModel();
  NotifyQueryChanged();
}

bool SearchBoxView::HandleKeyEvent(views::Textfield* sender,
                                   const ui::KeyEvent& key_event) {
  bool has_query = !search_box_->text().empty();

  // Escape with non-empty query text clears the search box.
  if (has_query && key_event.key_code() == ui::VKEY_ESCAPE) {
    search_box_->SetText(string16());
    // Updates model and fires query changed manually because SetText above
    // does not generate ContentsChanged notification.
    UpdateModel();
    NotifyQueryChanged();
    return true;
  }

  bool handled = false;
  if (contents_view_ && contents_view_->visible())
    handled = contents_view_->OnKeyPressed(key_event);

  return handled;
}

void SearchBoxView::IconChanged() {
  icon_view_->SetImage(model_->icon());
}

void SearchBoxView::HintTextChanged() {
  search_box_->set_placeholder_text(model_->hint_text());
}

void SearchBoxView::SelectionModelChanged() {
  search_box_->SelectSelectionModel(model_->selection_model());
}

void SearchBoxView::TextChanged() {
  search_box_->SetText(model_->text());
}

}  // namespace app_list

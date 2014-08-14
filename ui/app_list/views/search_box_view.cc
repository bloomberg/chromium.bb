// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_box_view.h"

#include <algorithm>

#include "grit/ui_resources.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/speech_ui_model.h"
#include "ui/app_list/views/app_list_menu_views.h"
#include "ui/app_list/views/search_box_view_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"

namespace app_list {

namespace {

const int kPadding = 14;
const int kIconDimension = 32;
const int kPreferredWidth = 360;
const int kPreferredHeight = 48;
#if !defined(OS_CHROMEOS)
const int kMenuButtonDimension = 29;
#endif

const SkColor kHintTextColor = SkColorSetRGB(0xA0, 0xA0, 0xA0);

// Menu offset relative to the bottom-right corner of the menu button.
const int kMenuYOffsetFromButton = -4;
const int kMenuXOffsetFromButton = -7;

}  // namespace

SearchBoxView::SearchBoxView(SearchBoxViewDelegate* delegate,
                             AppListViewDelegate* view_delegate)
    : delegate_(delegate),
      view_delegate_(view_delegate),
      model_(NULL),
      icon_view_(new views::ImageView),
      speech_button_(NULL),
      search_box_(new views::Textfield),
      contents_view_(NULL) {
  AddChildView(icon_view_);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

#if !defined(OS_CHROMEOS)
  menu_button_ = new views::MenuButton(NULL, base::string16(), this, false);
  menu_button_->SetBorder(views::Border::NullBorder());
  menu_button_->SetImage(views::Button::STATE_NORMAL,
                         *rb.GetImageSkiaNamed(IDR_APP_LIST_TOOLS_NORMAL));
  menu_button_->SetImage(views::Button::STATE_HOVERED,
                         *rb.GetImageSkiaNamed(IDR_APP_LIST_TOOLS_HOVER));
  menu_button_->SetImage(views::Button::STATE_PRESSED,
                         *rb.GetImageSkiaNamed(IDR_APP_LIST_TOOLS_PRESSED));
  AddChildView(menu_button_);
#endif

  search_box_->SetBorder(views::Border::NullBorder());
  search_box_->SetFontList(rb.GetFontList(ui::ResourceBundle::MediumFont));
  search_box_->set_placeholder_text_color(kHintTextColor);
  search_box_->set_controller(this);
  AddChildView(search_box_);

  view_delegate_->GetSpeechUI()->AddObserver(this);
  ModelChanged();
}

SearchBoxView::~SearchBoxView() {
  view_delegate_->GetSpeechUI()->RemoveObserver(this);
  model_->search_box()->RemoveObserver(this);
}

void SearchBoxView::ModelChanged() {
  if (model_)
    model_->search_box()->RemoveObserver(this);

  model_ = view_delegate_->GetModel();
  DCHECK(model_);
  model_->search_box()->AddObserver(this);
  IconChanged();
  SpeechRecognitionButtonPropChanged();
  HintTextChanged();
}

bool SearchBoxView::HasSearch() const {
  return !search_box_->text().empty();
}

void SearchBoxView::ClearSearch() {
  search_box_->SetText(base::string16());
  view_delegate_->AutoLaunchCanceled();
  // Updates model and fires query changed manually because SetText() above
  // does not generate ContentsChanged() notification.
  UpdateModel();
  NotifyQueryChanged();
}

void SearchBoxView::InvalidateMenu() {
  menu_.reset();
}

gfx::Size SearchBoxView::GetPreferredSize() const {
  return gfx::Size(kPreferredWidth, kPreferredHeight);
}

void SearchBoxView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Rect icon_frame(rect);
  icon_frame.set_width(kIconDimension + 2 * kPadding);
  icon_view_->SetBoundsRect(icon_frame);

  // Places |speech_button_| if exists. |speech_button_frame| holds its bounds
  // to calculate the search box bounds.
  gfx::Rect speech_button_frame;
  if (speech_button_) {
    speech_button_frame = icon_frame;
    speech_button_frame.set_x(rect.right() - icon_frame.width());
    gfx::Size button_size = speech_button_->GetPreferredSize();
    gfx::Point button_origin = speech_button_frame.CenterPoint();
    button_origin.Offset(-button_size.width() / 2, -button_size.height() / 2);
    speech_button_->SetBoundsRect(gfx::Rect(button_origin, button_size));
  }

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
  int edit_frame_width =
      rect.width() - icon_frame.width() - kPadding - menu_button_frame.width();
  if (!speech_button_frame.IsEmpty())
    edit_frame_width -= speech_button_frame.width() + kPadding;
  edit_frame.set_width(edit_frame_width);
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
  view_delegate_->AutoLaunchCanceled();
  NotifyQueryChanged();
}

bool SearchBoxView::HandleKeyEvent(views::Textfield* sender,
                                   const ui::KeyEvent& key_event) {
  bool handled = false;
  if (contents_view_ && contents_view_->visible())
    handled = contents_view_->OnKeyPressed(key_event);

  return handled;
}

void SearchBoxView::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  DCHECK(speech_button_ && sender == speech_button_);
  view_delegate_->ToggleSpeechRecognition();
}

void SearchBoxView::OnMenuButtonClicked(View* source, const gfx::Point& point) {
  if (!menu_)
    menu_.reset(new AppListMenuViews(view_delegate_));

  const gfx::Point menu_location =
      menu_button_->GetBoundsInScreen().bottom_right() +
      gfx::Vector2d(kMenuXOffsetFromButton, kMenuYOffsetFromButton);
  menu_->RunMenuAt(menu_button_, menu_location);
}

void SearchBoxView::IconChanged() {
  icon_view_->SetImage(model_->search_box()->icon());
}

void SearchBoxView::SpeechRecognitionButtonPropChanged() {
  const SearchBoxModel::SpeechButtonProperty* speech_button_prop =
      model_->search_box()->speech_button();
  if (speech_button_prop) {
    if (!speech_button_) {
      speech_button_ = new views::ImageButton(this);
      AddChildView(speech_button_);
    }

    if (view_delegate_->GetSpeechUI()->state() ==
        SPEECH_RECOGNITION_HOTWORD_LISTENING) {
      speech_button_->SetImage(
          views::Button::STATE_NORMAL, &speech_button_prop->on_icon);
      speech_button_->SetTooltipText(speech_button_prop->on_tooltip);
    } else {
      speech_button_->SetImage(
          views::Button::STATE_NORMAL, &speech_button_prop->off_icon);
      speech_button_->SetTooltipText(speech_button_prop->off_tooltip);
    }
  } else {
    if (speech_button_) {
      // Deleting a view will detach it from its parent.
      delete speech_button_;
      speech_button_ = NULL;
    }
  }
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

void SearchBoxView::OnSpeechRecognitionStateChanged(
    SpeechRecognitionState new_state) {
  SpeechRecognitionButtonPropChanged();
  SchedulePaint();
}

}  // namespace app_list

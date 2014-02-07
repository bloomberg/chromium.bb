// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/folder_header_view.h"

#include "base/strings/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "grit/ui_strings.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/painter.h"

namespace app_list {

namespace {

const int kPreferredWidth = 360;
const int kPreferredHeight = 48;
const int kIconDimension = 24;
const int kPadding = 14;
const int kFolderNameWidth = 150;
const int kFolderNameHeight = 30;
const int kBottomSeparatorWidth = 380;
const int kBottomSeparatorHeight = 1;

const SkColor kHintTextColor = SkColorSetRGB(0xA0, 0xA0, 0xA0);

}  // namespace

class FolderHeaderView::FolderNameView : public views::Textfield {
 public:
  FolderNameView() {
    SetBorder(views::Border::CreateEmptyBorder(1, 1, 1, 1));
    const SkColor kFocusBorderColor = SkColorSetRGB(64, 128, 250);
    SetFocusPainter(views::Painter::CreateSolidFocusPainter(
          kFocusBorderColor,
          gfx::Insets(0, 0, 1, 1)));
  }

  virtual ~FolderNameView() {
  }

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(kFolderNameWidth, kFolderNameHeight);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FolderNameView);
};

FolderHeaderView::FolderHeaderView(FolderHeaderViewDelegate* delegate)
    : folder_item_(NULL),
      back_button_(new views::ImageButton(this)),
      folder_name_view_(new FolderNameView),
      delegate_(delegate),
      folder_name_visible_(true) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  back_button_->SetImage(views::ImageButton::STATE_NORMAL,
      rb.GetImageSkiaNamed(IDR_APP_LIST_FOLDER_BACK_NORMAL));
  back_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
      views::ImageButton::ALIGN_MIDDLE);
  AddChildView(back_button_);

  folder_name_view_->SetFontList(
      rb.GetFontList(ui::ResourceBundle::MediumFont));
  folder_name_view_->set_placeholder_text_color(kHintTextColor);
  folder_name_view_->set_placeholder_text(
      rb.GetLocalizedString(IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER));
  folder_name_view_->SetBorder(views::Border::NullBorder());
  folder_name_view_->SetBackgroundColor(kContentsBackgroundColor);
  folder_name_view_->set_controller(this);
  AddChildView(folder_name_view_);
}

FolderHeaderView::~FolderHeaderView() {
  if (folder_item_)
    folder_item_->RemoveObserver(this);
}

void FolderHeaderView::SetFolderItem(AppListFolderItem* folder_item) {
  if (folder_item_)
    folder_item_->RemoveObserver(this);

  folder_item_ = folder_item;
  if (!folder_item_)
    return;
  folder_item_->AddObserver(this);

  Update();
}

void FolderHeaderView::UpdateFolderNameVisibility(bool visible) {
  folder_name_visible_ = visible;
  Update();
  SchedulePaint();
}

void FolderHeaderView::OnFolderItemRemoved() {
  folder_item_ = NULL;
}

void FolderHeaderView::Update() {
  if (!folder_item_)
    return;

  folder_name_view_->SetVisible(folder_name_visible_);
  if (folder_name_visible_)
    folder_name_view_->SetText(base::UTF8ToUTF16(folder_item_->title()));
}

gfx::Size FolderHeaderView::GetPreferredSize() {
  return gfx::Size(kPreferredWidth, kPreferredHeight);
}

void FolderHeaderView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Rect back_bounds(rect);
  back_bounds.set_width(kIconDimension + 2 * kPadding);
  back_button_->SetBoundsRect(back_bounds);

  gfx::Rect text_bounds(rect);
  int text_width = folder_name_view_->GetPreferredSize().width();
  text_bounds.set_x(back_bounds.x() + (rect.width() - text_width) / 2);
  text_bounds.set_width(text_width);
  text_bounds.ClampToCenteredSize(gfx::Size(text_bounds.width(),
      folder_name_view_->GetPreferredSize().height()));
  folder_name_view_->SetBoundsRect(text_bounds);
}

void FolderHeaderView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty() || !folder_name_visible_)
    return;

  // Draw bottom separator line.
  rect.set_x((rect.width() - kBottomSeparatorWidth) / 2 + rect.x());
  rect.set_y(rect.y() + rect.height() - kBottomSeparatorHeight);
  rect.set_width(kBottomSeparatorWidth);
  rect.set_height(kBottomSeparatorHeight);
  canvas->FillRect(rect, kTopSeparatorColor);
}

void FolderHeaderView::ContentsChanged(views::Textfield* sender,
                                       const base::string16& new_contents) {
  // Temporarily remove from observer to ignore data change caused by us.
  if (!folder_item_)
    return;

  folder_item_->RemoveObserver(this);
  std::string name = base::UTF16ToUTF8(folder_name_view_->text());
  folder_item_->SetTitleAndFullName(name, name);
  folder_item_->AddObserver(this);
}

void FolderHeaderView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  delegate_->NavigateBack(folder_item_, event);
}

void FolderHeaderView::ItemIconChanged() {
}

void FolderHeaderView::ItemTitleChanged() {
  Update();
}

void FolderHeaderView::ItemHighlightedChanged() {
}

void FolderHeaderView::ItemIsInstallingChanged() {
}

void FolderHeaderView::ItemPercentDownloadedChanged() {
}

}  // namespace app_list


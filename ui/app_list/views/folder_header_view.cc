// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/folder_header_view.h"

#include <algorithm>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/resources/grit/app_list_resources.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/painter.h"

namespace app_list {

namespace {

const int kPreferredWidth = 360;
const int kPreferredHeight = 48;
const int kBottomSeparatorHeight = 1;
const int kMaxFolderNameWidth = 300;

}  // namespace

class FolderHeaderView::FolderNameView : public views::Textfield {
 public:
  FolderNameView() {
    SetBorder(views::CreateEmptyBorder(1, 1, 1, 1));
    SetTextColor(kFolderTitleColor);
  }

  ~FolderNameView() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FolderNameView);
};

FolderHeaderView::FolderHeaderView(FolderHeaderViewDelegate* delegate)
    : folder_item_(nullptr),
      folder_name_view_(new FolderNameView),
      folder_name_placeholder_text_(
          ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
              IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER)),
      delegate_(delegate),
      folder_name_visible_(true) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  folder_name_view_->SetFontList(
      rb.GetFontList(ui::ResourceBundle::MediumFont));
  folder_name_view_->set_placeholder_text_color(kFolderTitleHintTextColor);
  folder_name_view_->set_placeholder_text(folder_name_placeholder_text_);
  folder_name_view_->SetBorder(views::NullBorder());
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

  folder_name_view_->SetEnabled(folder_item_->folder_type() !=
                                AppListFolderItem::FOLDER_TYPE_OEM);

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

void FolderHeaderView::SetTextFocus() {
  if (!folder_name_view_->HasFocus()) {
    views::FocusManager* focus_manager = GetFocusManager();
    if (focus_manager)
      focus_manager->SetFocusedView(folder_name_view_);
  }
}

bool FolderHeaderView::HasTextFocus() const {
  return folder_name_view_->HasFocus();
}

void FolderHeaderView::Update() {
  if (!folder_item_)
    return;

  folder_name_view_->SetVisible(folder_name_visible_);
  if (folder_name_visible_) {
    folder_name_view_->SetText(base::UTF8ToUTF16(folder_item_->name()));
    UpdateFolderNameAccessibleName();
  }

  Layout();
}

void FolderHeaderView::UpdateFolderNameAccessibleName() {
  // Sets |folder_name_view_|'s accessible name to the placeholder text if
  // |folder_name_view_| is blank; otherwise, clear the accessible name, the
  // accessible state's value is set to be folder_name_view_->text() by
  // TextField.
  base::string16 accessible_name = folder_name_view_->text().empty()
                                       ? folder_name_placeholder_text_
                                       : base::string16();
  folder_name_view_->SetAccessibleName(accessible_name);
}

const base::string16& FolderHeaderView::GetFolderNameForTest() {
  return folder_name_view_->text();
}

void FolderHeaderView::SetFolderNameForTest(const base::string16& name) {
  folder_name_view_->SetText(name);
}

bool FolderHeaderView::IsFolderNameEnabledForTest() const {
  return folder_name_view_->enabled();
}

gfx::Size FolderHeaderView::GetPreferredSize() const {
  return gfx::Size(kPreferredWidth, kPreferredHeight);
}

void FolderHeaderView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Rect text_bounds(rect);
  base::string16 text = folder_item_ && !folder_item_->name().empty()
                            ? base::UTF8ToUTF16(folder_item_->name())
                            : folder_name_placeholder_text_;
  int text_width =
      gfx::Canvas::GetStringWidth(text, folder_name_view_->GetFontList()) +
      folder_name_view_->GetCaretBounds().width() +
      folder_name_view_->GetInsets().width();
  text_width = std::min(text_width, kMaxFolderNameWidth);
  text_bounds.set_x(rect.x() + (rect.width() - text_width) / 2);
  text_bounds.set_width(text_width);
  text_bounds.ClampToCenteredSize(gfx::Size(text_bounds.width(),
      folder_name_view_->GetPreferredSize().height()));
  folder_name_view_->SetBoundsRect(text_bounds);
}

bool FolderHeaderView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN)
    delegate_->GiveBackFocusToSearchBox();

  return false;
}

void FolderHeaderView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty() || !folder_name_visible_)
    return;

  // Draw bottom separator line.
  rect.Inset(kAppsGridPadding, 0);
  rect.set_y(rect.bottom() - kBottomSeparatorHeight);
  rect.set_height(kBottomSeparatorHeight);
  canvas->FillRect(rect, kTopSeparatorColor);
}

void FolderHeaderView::ContentsChanged(views::Textfield* sender,
                                       const base::string16& new_contents) {
  // Temporarily remove from observer to ignore data change caused by us.
  if (!folder_item_)
    return;

  folder_item_->RemoveObserver(this);
  // Enforce the maximum folder name length in UI.
  std::string name = base::UTF16ToUTF8(
      folder_name_view_->text().substr(0, kMaxFolderNameChars));
  if (name != folder_item_->name())
    delegate_->SetItemName(folder_item_, name);
  folder_item_->AddObserver(this);

  UpdateFolderNameAccessibleName();

  Layout();
}

void FolderHeaderView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  delegate_->NavigateBack(folder_item_, event);
}

void FolderHeaderView::ItemNameChanged() {
  Update();
}

}  // namespace app_list

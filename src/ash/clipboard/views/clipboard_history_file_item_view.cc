// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_file_item_view.h"

#include <array>

#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view_class_properties.h"

namespace {

// The file icon's preferred size.
constexpr gfx::Size kIconSize(20, 20);

// The file icon's margin.
constexpr auto kIconMargin = gfx::Insets::TLBR(0, 0, 0, 12);
}  // namespace

namespace ash {

ClipboardHistoryFileItemView::ClipboardHistoryFileItemView(
    const ClipboardHistoryItem* clipboard_history_item,
    views::MenuItemView* container)
    : ClipboardHistoryTextItemView(clipboard_history_item, container) {}
ClipboardHistoryFileItemView::~ClipboardHistoryFileItemView() = default;

std::unique_ptr<ClipboardHistoryFileItemView::ContentsView>
ClipboardHistoryFileItemView::CreateContentsView() {
  auto contents_view = ClipboardHistoryTextItemView::CreateContentsView();

  // `file_icon` should be `contents_view`'s first child.
  file_icon_ = contents_view->AddChildViewAt(
      std::make_unique<views::ImageView>(), /*index=*/0);
  file_icon_->SetImageSize(kIconSize);
  file_icon_->SetProperty(views::kMarginsKey, kIconMargin);

  return contents_view;
}

const char* ClipboardHistoryFileItemView::GetClassName() const {
  return "ClipboardHistoryFileItemView";
}

void ClipboardHistoryFileItemView::OnThemeChanged() {
  ClipboardHistoryTextItemView::OnThemeChanged();

  // Use the light mode as default because the light mode is the default mode
  // of the native theme which decides the context menu's background color.
  // TODO(andrewxu): remove this line after https://crbug.com/1143009 is fixed.
  ScopedLightModeAsDefault scoped_light_mode_as_default;

  file_icon_->SetImage(ClipboardHistoryUtil::GetIconForFileClipboardItem(
      *clipboard_history_item(), base::UTF16ToUTF8(text())));
}

}  // namespace ash

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_FILE_ITEM_VIEW_H_
#define ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_FILE_ITEM_VIEW_H_

#include "ash/clipboard/views/clipboard_history_text_item_view.h"

namespace views {
class ImageView;
class MenuItemView;
}

namespace ash {

// The menu item showing the copied file.
class ClipboardHistoryFileItemView : public ClipboardHistoryTextItemView {
 public:
  ClipboardHistoryFileItemView(
      const ClipboardHistoryItem* clipboard_history_item,
      views::MenuItemView* container);
  ClipboardHistoryFileItemView(const ClipboardHistoryFileItemView& rhs) =
      delete;
  ClipboardHistoryFileItemView& operator=(
      const ClipboardHistoryFileItemView& rhs) = delete;
  ~ClipboardHistoryFileItemView() override;

 private:
  // ClipboardHistoryTextItemView:
  std::unique_ptr<ContentsView> CreateContentsView() override;
  const char* GetClassName() const override;
  void OnThemeChanged() override;

  views::ImageView* file_icon_ = nullptr;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_FILE_ITEM_VIEW_H_

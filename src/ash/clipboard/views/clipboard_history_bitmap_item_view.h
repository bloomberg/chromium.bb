// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_BITMAP_ITEM_VIEW_H_
#define ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_BITMAP_ITEM_VIEW_H_

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/views/clipboard_history_item_view.h"
#include "ui/base/clipboard/clipboard_data.h"

namespace ash {
class ClipboardHistoryResourceManager;

// The menu item showing a bitmap.
class ClipboardHistoryBitmapItemView : public ClipboardHistoryItemView {
 public:
  ClipboardHistoryBitmapItemView(
      const ClipboardHistoryItem* clipboard_history_item,
      const ClipboardHistoryResourceManager* resource_manager,
      views::MenuItemView* container);
  ClipboardHistoryBitmapItemView(const ClipboardHistoryBitmapItemView& rhs) =
      delete;
  ClipboardHistoryBitmapItemView& operator=(
      const ClipboardHistoryBitmapItemView& rhs) = delete;
  ~ClipboardHistoryBitmapItemView() override;

 private:
  class BitmapContentsView;

  // ClipboardHistoryItemView:
  const char* GetClassName() const override;
  std::unique_ptr<ContentsView> CreateContentsView() override;
  std::u16string GetAccessibleName() const override;

  // Owned by ClipboardHistoryController.
  const ClipboardHistoryResourceManager* const resource_manager_;

  // The format of the associated `ClipboardData`.
  const ui::ClipboardInternalFormat data_format_;
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_BITMAP_ITEM_VIEW_H_

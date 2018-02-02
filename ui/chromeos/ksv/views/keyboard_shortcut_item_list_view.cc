// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/ksv/views/keyboard_shortcut_item_list_view.h"

#include "ui/chromeos/ksv/views/keyboard_shortcut_item_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace keyboard_shortcut_viewer {

KeyboardShortcutItemListView::KeyboardShortcutItemListView()
    : shortcut_item_views_(new views::View) {
  auto layout = std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  shortcut_item_views_->SetLayoutManager(std::move(layout));

  views::ScrollView* const scroller = new views::ScrollView();
  scroller->set_draw_overflow_indicator(false);
  scroller->ClipHeightTo(0, 0);
  scroller->SetContents(shortcut_item_views_);
  AddChildView(scroller);
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

void KeyboardShortcutItemListView::AddItemView(
    KeyboardShortcutItemView* item_view) {
  shortcut_item_views_->AddChildView(item_view);
}

}  // namespace keyboard_shortcut_viewer

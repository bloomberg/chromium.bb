// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/ksv/views/keyboard_shortcut_view.h"

#include "ui/chromeos/ksv/keyboard_shortcut_viewer_metadata.h"
#include "ui/chromeos/ksv/views/keyboard_shortcut_item_list_view.h"
#include "ui/chromeos/ksv/views/keyboard_shortcut_item_view.h"
#include "ui/views/background.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace keyboard_shortcut_viewer {

namespace {

KeyboardShortcutView* g_ksv_view = nullptr;

}  // namespace

KeyboardShortcutView::~KeyboardShortcutView() {
  DCHECK_EQ(g_ksv_view, this);
  g_ksv_view = nullptr;
}

// static
views::Widget* KeyboardShortcutView::Show(gfx::NativeWindow context) {
  if (g_ksv_view) {
    // If there is a KeyboardShortcutView window open already, just activate it.
    g_ksv_view->GetWidget()->Activate();
  } else {
    // TODO(wutao): change the bounds to UX specs when they are available.
    views::Widget::CreateWindowWithContextAndBounds(
        new KeyboardShortcutView(), context, gfx::Rect(0, 0, 660, 560));
    g_ksv_view->GetWidget()->Show();
  }
  return g_ksv_view->GetWidget();
}

KeyboardShortcutView::KeyboardShortcutView() {
  DCHECK_EQ(g_ksv_view, nullptr);
  g_ksv_view = this;

  // TODO(wutao): will be removed and replaced by explicit Layout().
  SetLayoutManager(std::make_unique<views::FillLayout>());
  // Default background is transparent.
  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  InitViews();
}

void KeyboardShortcutView::InitViews() {
  // Init views of KeyboardShortcutItemView.
  for (const auto& item : GetKeyboardShortcutItemList()) {
    for (auto category : item.categories) {
      shortcut_views_by_category_[category].emplace_back(
          new KeyboardShortcutItemView(item));
    }
  }

  // Init views of TabbedPane and KeyboardShortcutItemListView.
  tabbed_pane_ =
      new views::TabbedPane(views::TabbedPane::Orientation::kVertical);
  for (auto category : GetShortcutCategories()) {
    auto iter = shortcut_views_by_category_.find(category);
    // TODO(wutao): add DCKECK(iter != shortcut_views_by_category_.end()).
    if (iter != shortcut_views_by_category_.end()) {
      KeyboardShortcutItemListView* item_list_view =
          new KeyboardShortcutItemListView();
      for (auto* item_view : iter->second)
        item_list_view->AddItemView(item_view);
      tabbed_pane_->AddTab(GetStringForCategory(category), item_list_view);
    }
  }
  AddChildView(tabbed_pane_);
}

bool KeyboardShortcutView::CanMaximize() const {
  return false;
}

bool KeyboardShortcutView::CanMinimize() const {
  return true;
}

bool KeyboardShortcutView::CanResize() const {
  return false;
}

views::ClientView* KeyboardShortcutView::CreateClientView(
    views::Widget* widget) {
  return new views::ClientView(widget, this);
}

KeyboardShortcutView* KeyboardShortcutView::GetInstanceForTests() {
  return g_ksv_view;
}

int KeyboardShortcutView::GetCategoryNumberForTests() const {
  return shortcut_views_by_category_.size();
}

int KeyboardShortcutView::GetTabCountForTests() const {
  return tabbed_pane_->GetTabCount();
}

}  // namespace keyboard_shortcut_viewer

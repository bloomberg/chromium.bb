// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/tabbed_pane/tabbed_pane.h"

#include "views/controls/tabbed_pane/native_tabbed_pane_wrapper.h"

namespace views {

// static
const char TabbedPane::kViewClassName[] = "views/TabbedPane";

TabbedPane::TabbedPane() : native_tabbed_pane_(NULL), listener_(NULL) {
  SetFocusable(true);
}

TabbedPane::~TabbedPane() {
}

void TabbedPane::SetListener(Listener* listener) {
  listener_ = listener;
}

void TabbedPane::AddTab(const std::wstring& title, View* contents) {
  native_tabbed_pane_->AddTab(title, contents);
}

void TabbedPane::AddTabAtIndex(int index,
                               const std::wstring& title,
                               View* contents,
                               bool select_if_first_tab) {
  native_tabbed_pane_->AddTabAtIndex(index, title, contents,
                                     select_if_first_tab);
}

int TabbedPane::GetSelectedTabIndex() {
  return native_tabbed_pane_->GetSelectedTabIndex();
}

View* TabbedPane::GetSelectedTab() {
  return native_tabbed_pane_->GetSelectedTab();
}

View* TabbedPane::RemoveTabAtIndex(int index) {
  return native_tabbed_pane_->RemoveTabAtIndex(index);
}

void TabbedPane::SelectTabAt(int index) {
   native_tabbed_pane_->SelectTabAt(index);
}

int TabbedPane::GetTabCount() {
  return native_tabbed_pane_->GetTabCount();
}

void TabbedPane::CreateWrapper() {
  native_tabbed_pane_ = NativeTabbedPaneWrapper::CreateNativeWrapper(this);
}

// View overrides:
std::string TabbedPane::GetClassName() const {
  return kViewClassName;
}

void TabbedPane::ViewHierarchyChanged(bool is_add, View* parent, View* child) {
  if (is_add && !native_tabbed_pane_ && GetWidget()) {
    CreateWrapper();
    AddChildView(native_tabbed_pane_->GetView());
  }
}

void TabbedPane::Layout() {
  if (native_tabbed_pane_) {
    native_tabbed_pane_->GetView()->SetBounds(0, 0, width(), height());
    native_tabbed_pane_->GetView()->Layout();
  }
}

void TabbedPane::Focus() {
  // Forward the focus to the wrapper.
  if (native_tabbed_pane_)
    native_tabbed_pane_->SetFocus();
  else
    View::Focus();  // Will focus the RootView window (so we still get keyboard
                    // messages).
}

}  // namespace views

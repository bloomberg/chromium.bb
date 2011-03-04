// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/tabbed_pane/tabbed_pane.h"

#include "base/logging.h"
// TODO(avi): remove when not needed
#include "base/utf_string_conversions.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "views/controls/native/native_view_host.h"
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
  PreferredSizeChanged();
}

void TabbedPane::AddTabAtIndex(int index,
                               const std::wstring& title,
                               View* contents,
                               bool select_if_first_tab) {
  native_tabbed_pane_->AddTabAtIndex(index, title, contents,
                                     select_if_first_tab);
  contents->SetAccessibleName(WideToUTF16Hack(title));
  PreferredSizeChanged();
}

int TabbedPane::GetSelectedTabIndex() {
  return native_tabbed_pane_->GetSelectedTabIndex();
}

View* TabbedPane::GetSelectedTab() {
  return native_tabbed_pane_->GetSelectedTab();
}

View* TabbedPane::RemoveTabAtIndex(int index) {
  View* tab = native_tabbed_pane_->RemoveTabAtIndex(index);
  PreferredSizeChanged();
  return tab;
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
  if (is_add && !native_tabbed_pane_) {
    CreateWrapper();
    AddChildView(native_tabbed_pane_->GetView());
    LoadAccelerators();
  }
}

bool TabbedPane::AcceleratorPressed(const views::Accelerator& accelerator) {
  // We only accept Ctrl+Tab keyboard events.
  DCHECK(accelerator.GetKeyCode() ==
      ui::VKEY_TAB && accelerator.IsCtrlDown());

  int tab_count = GetTabCount();
  if (tab_count <= 1)
    return false;
  int selected_tab_index = GetSelectedTabIndex();
  int next_tab_index = accelerator.IsShiftDown() ?
      (selected_tab_index - 1) % tab_count :
      (selected_tab_index + 1) % tab_count;
  // Wrap around.
  if (next_tab_index < 0)
    next_tab_index += tab_count;
  SelectTabAt(next_tab_index);
  return true;
}

void TabbedPane::LoadAccelerators() {
  // Ctrl+Shift+Tab
  AddAccelerator(views::Accelerator(ui::VKEY_TAB, true, true, false));
  // Ctrl+Tab
  AddAccelerator(views::Accelerator(ui::VKEY_TAB, false, true, false));
}

void TabbedPane::Layout() {
  if (native_tabbed_pane_)
    native_tabbed_pane_->GetView()->SetBounds(0, 0, width(), height());
}

void TabbedPane::OnFocus() {
  // Forward the focus to the wrapper.
  if (native_tabbed_pane_) {
    native_tabbed_pane_->SetFocus();

    View* selected_tab = GetSelectedTab();
    if (selected_tab)
       selected_tab->NotifyAccessibilityEvent(AccessibilityTypes::EVENT_FOCUS);
  }
  else
    View::OnFocus();  // Will focus the RootView window (so we still get
                      // keyboard messages).
}

void TabbedPane::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (NativeViewHost::kRenderNativeControlFocus)
    View::OnPaintFocusBorder(canvas);
}

AccessibilityTypes::Role TabbedPane::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_PAGETABLIST;
}

gfx::Size TabbedPane::GetPreferredSize() {
  return native_tabbed_pane_ ?
      native_tabbed_pane_->GetPreferredSize() : gfx::Size();
}

}  // namespace views

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/desktop/desktop_views_delegate.h"

#include "base/logging.h"
#include "ui/views/desktop/desktop_window_view.h"

namespace views {
namespace desktop {

////////////////////////////////////////////////////////////////////////////////
// DesktopViewsDelegate, public:

DesktopViewsDelegate::DesktopViewsDelegate() {
  DCHECK(!views::ViewsDelegate::views_delegate);
  views::ViewsDelegate::views_delegate = this;
}

DesktopViewsDelegate::~DesktopViewsDelegate() {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopViewsDelegate, ViewsDelegate implementation:

ui::Clipboard* DesktopViewsDelegate::GetClipboard() const {
  return NULL;
}

View* DesktopViewsDelegate::GetDefaultParentView() {
  return DesktopWindowView::desktop_window_view;
}

void DesktopViewsDelegate::SaveWindowPlacement(const Widget* widget,
                                               const std::string& window_name,
                                               const gfx::Rect& bounds,
                                               ui::WindowShowState show_state) {
}

bool DesktopViewsDelegate::GetSavedWindowPlacement(
    const std::string& window_name,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  return false;
}

void DesktopViewsDelegate::NotifyAccessibilityEvent(
    views::View* view, ui::AccessibilityTypes::Event event_type) {
}

void DesktopViewsDelegate::NotifyMenuItemFocused(const string16& menu_name,
                                                 const string16& menu_item_name,
                                                 int item_index,
                                                 int item_count,
                                                 bool has_submenu) {
}

#if defined(OS_WIN)
HICON DesktopViewsDelegate::GetDefaultWindowIcon() const {
  return NULL;
}
#endif

void DesktopViewsDelegate::AddRef() {
}

void DesktopViewsDelegate::ReleaseRef() {
}

int DesktopViewsDelegate::GetDispositionForEvent(int event_flags) {
  return 0;
}

}  // namespace desktop
}  // namespace views

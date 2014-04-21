// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/views_delegate.h"

#include "ui/views/touchui/touch_selection_controller_impl.h"

namespace views {

ViewsDelegate::ViewsDelegate()
    : views_tsc_factory_(new views::ViewsTouchSelectionControllerFactory) {
  ui::TouchSelectionControllerFactory::SetInstance(views_tsc_factory_.get());
}

ViewsDelegate::~ViewsDelegate() {
  ui::TouchSelectionControllerFactory::SetInstance(NULL);
}

void ViewsDelegate::SaveWindowPlacement(const Widget* widget,
                                        const std::string& window_name,
                                        const gfx::Rect& bounds,
                                        ui::WindowShowState show_state) {
}

bool ViewsDelegate::GetSavedWindowPlacement(
    const Widget* widget,
    const std::string& window_name,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  return false;
}

void ViewsDelegate::NotifyAccessibilityEvent(View* view,
                                             ui::AXEvent event_type) {
}

void ViewsDelegate::NotifyMenuItemFocused(const base::string16& menu_name,
                                          const base::string16& menu_item_name,
                                          int item_index,
                                          int item_count,
                                          bool has_submenu) {
}

#if defined(OS_WIN)
HICON ViewsDelegate::GetDefaultWindowIcon() const {
  return NULL;
}

bool ViewsDelegate::IsWindowInMetro(gfx::NativeWindow window) const {
  return false;
}
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS)
gfx::ImageSkia* ViewsDelegate::GetDefaultWindowIcon() const {
  return NULL;
}
#endif

NonClientFrameView* ViewsDelegate::CreateDefaultNonClientFrameView(
    Widget* widget) {
  return NULL;
}

void ViewsDelegate::AddRef() {
}

void ViewsDelegate::ReleaseRef() {
}

content::WebContents* ViewsDelegate::CreateWebContents(
    content::BrowserContext* browser_context,
    content::SiteInstance* site_instance) {
  return NULL;
}

base::TimeDelta ViewsDelegate::GetDefaultTextfieldObscuredRevealDuration() {
  return base::TimeDelta();
}

bool ViewsDelegate::WindowManagerProvidesTitleBar(bool maximized) {
  return false;
}

}  // namespace views

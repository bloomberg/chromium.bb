// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_views_delegate.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/views/views_switches.h"

#if defined(USE_AURA) && !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_native_widget_aura.h"
#endif

namespace views {

TestViewsDelegate::TestViewsDelegate()
    : use_transparent_windows_(false) {
  DCHECK(!ViewsDelegate::views_delegate);
  ViewsDelegate::views_delegate = this;
}

TestViewsDelegate::~TestViewsDelegate() {
  ViewsDelegate::views_delegate = NULL;
}

void TestViewsDelegate::SetUseTransparentWindows(bool transparent) {
  use_transparent_windows_ = transparent;
}

void TestViewsDelegate::SaveWindowPlacement(const Widget* window,
                                            const std::string& window_name,
                                            const gfx::Rect& bounds,
                                            ui::WindowShowState show_state) {
}

bool TestViewsDelegate::GetSavedWindowPlacement(
    const std::string& window_name,
    gfx::Rect* bounds,
    ui:: WindowShowState* show_state) const {
  return false;
}

NonClientFrameView* TestViewsDelegate::CreateDefaultNonClientFrameView(
    Widget* widget) {
  return NULL;
}

bool TestViewsDelegate::UseTransparentWindows() const {
  return use_transparent_windows_;
}

int TestViewsDelegate::GetDispositionForEvent(int event_flags) {
  return 0;
}

#if defined(USE_AURA)
views::NativeWidgetHelperAura* TestViewsDelegate::CreateNativeWidgetHelper(
    views::NativeWidgetAura* native_widget) {
  return NULL;
}
#endif

content::WebContents* TestViewsDelegate::CreateWebContents(
    content::BrowserContext* browser_context,
    content::SiteInstance* site_instance) {
  return NULL;
}

NativeWidget* TestViewsDelegate::CreateNativeWidget(
    internal::NativeWidgetDelegate* delegate,
    gfx::NativeView parent) {
#if defined(USE_AURA) && !defined(OS_CHROMEOS)
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDesktopAura))
    return new DesktopNativeWidgetAura(delegate);
#endif
  return NULL;
}

}  // namespace views

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_views_delegate.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/wm/core/wm_state.h"

#if !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/native_widget_aura.h"
#endif

namespace views {

TestViewsDelegate::TestViewsDelegate()
    : use_transparent_windows_(false) {
  DCHECK(!ViewsDelegate::views_delegate);
  ViewsDelegate::views_delegate = this;
  wm_state_.reset(new wm::WMState);
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
    const Widget* window,
    const std::string& window_name,
    gfx::Rect* bounds,
    ui:: WindowShowState* show_state) const {
  return false;
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
gfx::ImageSkia* TestViewsDelegate::GetDefaultWindowIcon() const {
  return NULL;
}
#endif

NonClientFrameView* TestViewsDelegate::CreateDefaultNonClientFrameView(
    Widget* widget) {
  return NULL;
}

content::WebContents* TestViewsDelegate::CreateWebContents(
    content::BrowserContext* browser_context,
    content::SiteInstance* site_instance) {
  return NULL;
}

void TestViewsDelegate::OnBeforeWidgetInit(
    Widget::InitParams* params,
    internal::NativeWidgetDelegate* delegate) {
  if (params->opacity == Widget::InitParams::INFER_OPACITY) {
    if (use_transparent_windows_)
      params->opacity = Widget::InitParams::TRANSLUCENT_WINDOW;
    else
      params->opacity = Widget::InitParams::OPAQUE_WINDOW;
  }
}

base::TimeDelta TestViewsDelegate::GetDefaultTextfieldObscuredRevealDuration() {
  return base::TimeDelta();
}

}  // namespace views

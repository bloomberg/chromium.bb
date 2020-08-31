// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/frame/custom_tab_browser_frame.h"
#endif
#include "chrome/browser/ui/views/frame/native_browser_frame_factory.h"
#include "chrome/grit/chromium_strings.h"
#include "components/safe_browsing/content/password_protection/metrics_util.h"
#if defined(USE_AURA)
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#endif
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

// static
BrowserWindow* BrowserWindow::CreateBrowserWindow(
    std::unique_ptr<Browser> browser,
    bool user_gesture,
    bool in_tab_dragging) {
  // Create the view and the frame. The frame will attach itself via the view
  // so we don't need to do anything with the pointer.
  BrowserView* view = new BrowserView(std::move(browser));
  BrowserFrame* browser_frame = nullptr;
#if defined(OS_CHROMEOS)
  if (view->browser()->is_type_custom_tab())
    browser_frame = new CustomTabBrowserFrame(view);
#endif
  if (!browser_frame)
    browser_frame = new BrowserFrame(view);
  if (in_tab_dragging)
    browser_frame->SetTabDragKind(TabDragKind::kAllTabs);
  browser_frame->InitBrowserFrame();

  view->GetWidget()->non_client_view()->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));

#if defined(USE_AURA)
  // For now, all browser windows are true. This only works when USE_AURA
  // because it requires gfx::NativeWindow to be an aura::Window*.
  view->GetWidget()->GetNativeWindow()->SetProperty(
      aura::client::kCreatedByUserGesture, user_gesture);
#endif
  return view;
}

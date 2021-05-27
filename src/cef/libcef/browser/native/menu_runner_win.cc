// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/native/menu_runner_win.h"

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/native/menu_2.h"

#include "base/task/current_thread.h"
#include "ui/gfx/geometry/point.h"

CefMenuRunnerWin::CefMenuRunnerWin() {}

bool CefMenuRunnerWin::RunContextMenu(
    AlloyBrowserHostImpl* browser,
    CefMenuModelImpl* model,
    const content::ContextMenuParams& params) {
  // Create a menu based on the model.
  menu_.reset(new views::CefNativeMenuWin(model->model(), nullptr));
  menu_->Rebuild(nullptr);

  // Make sure events can be pumped while the menu is up.
  base::CurrentThread::ScopedNestableTaskAllower allow;

  const gfx::Point& screen_point =
      browser->GetScreenPoint(gfx::Point(params.x, params.y));

  // Show the menu. Blocks until the menu is dismissed.
  menu_->RunMenuAt(screen_point, views::Menu2::ALIGN_TOPLEFT);

  return true;
}

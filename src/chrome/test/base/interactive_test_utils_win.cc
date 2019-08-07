// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/interactive_test_utils.h"

#include <Psapi.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/test/base/interactive_test_utils_aura.h"
#include "chrome/test/base/process_lineage_win.h"
#include "chrome/test/base/save_desktop_snapshot_win.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/win/foreground_helper.h"
#include "ui/views/focus/focus_manager.h"

namespace ui_test_utils {

void HideNativeWindow(gfx::NativeWindow window) {
#if defined(OS_CHROMEOS)
  HideNativeWindowAura(window);
#else
  HWND hwnd = window->GetHost()->GetAcceleratedWidget();
  ::ShowWindow(hwnd, SW_HIDE);
#endif  // OS_CHROMEOS
}

bool ShowAndFocusNativeWindow(gfx::NativeWindow window) {
#if defined(OS_CHROMEOS)
  ShowAndFocusNativeWindowAura(window);
#endif  // OS_CHROMEOS
  window->Show();
  // Always make sure the window hosting ash is visible and focused.
  HWND hwnd = window->GetHost()->GetAcceleratedWidget();

  ::ShowWindow(hwnd, SW_SHOW);

  if (GetForegroundWindow() != hwnd) {
    VLOG(1) << "Forcefully refocusing front window";
    ui::ForegroundHelper::SetForeground(hwnd);
  }

  // ShowWindow does not necessarily activate the window. In particular if a
  // window from another app is the foreground window then the request to
  // activate the window fails. See SetForegroundWindow for details.
  HWND foreground_window = GetForegroundWindow();
  if (foreground_window == hwnd)
    return true;

  wchar_t window_title[256];
  GetWindowText(foreground_window, window_title, base::size(window_title));

  base::string16 lineage_str;
  if (foreground_window) {
    DWORD process_id = 0;
    GetWindowThreadProcessId(foreground_window, &process_id);
    ProcessLineage lineage = ProcessLineage::Create(process_id);
    if (!lineage.IsEmpty()) {
      lineage_str = STRING16_LITERAL(", process lineage: ");
      lineage_str.append(lineage.ToString());
    }
  }
  LOG(ERROR) << "ShowAndFocusNativeWindow failed. foreground window: "
             << foreground_window << ", title: " << window_title << lineage_str;

  const base::FilePath output_dir =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kSnapshotOutputDir);
  if (!output_dir.empty()) {
    base::FilePath snapshot_file = SaveDesktopSnapshot(output_dir);
    if (!snapshot_file.empty()) {
      LOG(ERROR) << "Screenshot saved to file: \"" << snapshot_file.value()
                 << "\"";
    }
  }

  return false;
}

}  // namespace ui_test_utils

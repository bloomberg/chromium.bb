// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/win/host_configurer_window.h"

#include <atlbase.h>
#include <atlstr.h>
#include <atlwin.h>
#include <windows.h>

#include "base/run_loop.h"
#include "base/string16.h"
#include "remoting/host/setup/win/load_string_from_resource.h"
#include "remoting/host/setup/win/start_host_window.h"

namespace remoting {

HostConfigurerWindow::HostConfigurerWindow(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::Closure on_destroy)
    : url_request_context_getter_(url_request_context_getter),
      ui_task_runner_(ui_task_runner),
      on_destroy_(on_destroy) {
}

void HostConfigurerWindow::OnCancel(UINT code, int id, HWND control) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  Finish();
}

void HostConfigurerWindow::OnClose() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  Finish();
}

LRESULT HostConfigurerWindow::OnInitDialog(HWND wparam, LPARAM lparam) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Failure of any of these calls is acceptable.
  SetWindowText(LoadStringFromResource(IDS_TITLE));
  GetDlgItem(IDC_CHANGE_PIN).EnableWindow(FALSE);
  GetDlgItem(IDC_STOP_HOST).EnableWindow(FALSE);
  PositionWindow();
  return TRUE;
}

void HostConfigurerWindow::OnOk(UINT code, int id, HWND control) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  Finish();
}

void HostConfigurerWindow::OnStartHost(UINT code, int id, HWND control) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  StartHostWindow start_host_window(url_request_context_getter_);
  start_host_window.DoModal();
}

void HostConfigurerWindow::PositionWindow() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // Get the window dimensions.
  RECT rect;
  if (!GetWindowRect(&rect)) {
    return;
  }

  // Position against the owner window unless it is minimized or invisible.
  HWND owner = ::GetWindow(m_hWnd, GW_OWNER);
  if (owner != NULL) {
    DWORD style = ::GetWindowLong(owner, GWL_STYLE);
    if ((style & WS_MINIMIZE) != 0 || (style & WS_VISIBLE) == 0) {
      owner = NULL;
    }
  }

  // Make sure that the window will not end up split by a monitor's boundary.
  RECT area_rect;
  if (!::SystemParametersInfo(SPI_GETWORKAREA, NULL, &area_rect, NULL)) {
    return;
  }

  // On a multi-monitor system use the monitor where the owner window is shown.
  RECT owner_rect = area_rect;
  if (owner != NULL && ::GetWindowRect(owner, &owner_rect)) {
    HMONITOR monitor = ::MonitorFromRect(&owner_rect, MONITOR_DEFAULTTONEAREST);
    if (monitor != NULL) {
      MONITORINFO monitor_info = {0};
      monitor_info.cbSize = sizeof(monitor_info);
      if (::GetMonitorInfo(monitor, &monitor_info)) {
        area_rect = monitor_info.rcWork;
      }
    }
  }

  LONG width = rect.right - rect.left;
  LONG height = rect.bottom - rect.top;
  // Avoid the center of the owner rectangle, because the host controller will
  // put its confirmation window there.
  LONG x = (3 * owner_rect.left + owner_rect.right) / 4 - (width / 2);
  LONG y = (3 * owner_rect.top + owner_rect.bottom) / 4 - (height / 2);

  x = std::max(x, area_rect.left);
  x = std::min(x, area_rect.right - width);
  y = std::max(y, area_rect.top);
  y = std::min(y, area_rect.bottom - width);

  SetWindowPos(NULL, x, y, -1, -1, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void HostConfigurerWindow::Finish() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  DestroyWindow();
  on_destroy_.Run();
}

}  // namespace remoting

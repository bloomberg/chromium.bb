// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/disconnect_window.h"

#include <windows.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/host_ui_resource.h"
#include "remoting/host/ui_strings.h"

// TODO(garykac): Lots of duplicated code in this file and
// continue_window_win.cc. If we need to expand this then we should
// create a class with the shared code.

// HMODULE from DllMain/WinMain. This is needed to find our dialog resource.
// This is defined in:
//   Plugin: host_plugin.cc
//   SimpleHost: simple_host_process.cc
extern HMODULE g_hModule;

const int DISCONNECT_HOTKEY_ID = 1000;
const int kWindowBorderRadius = 14;

namespace remoting {

class DisconnectWindowWin : public DisconnectWindow {
 public:
  DisconnectWindowWin();
  virtual ~DisconnectWindowWin();

  virtual void Show(ChromotingHost* host,
                    const DisconnectCallback& disconnect_callback,
                    const std::string& username) OVERRIDE;
  virtual void Hide() OVERRIDE;

private:
  static BOOL CALLBACK DialogProc(HWND hwmd, UINT msg, WPARAM wParam,
                                  LPARAM lParam);

  BOOL OnDialogMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  void ShutdownHost();
  void EndDialog();
  void SetStrings(const UiStrings& strings, const std::string& username);

  DisconnectCallback disconnect_callback_;
  HWND hwnd_;
  bool has_hotkey_;
  base::win::ScopedGDIObject<HPEN> border_pen_;

  DISALLOW_COPY_AND_ASSIGN(DisconnectWindowWin);
};

DisconnectWindowWin::DisconnectWindowWin()
    : hwnd_(NULL),
      has_hotkey_(false),
      border_pen_(CreatePen(PS_SOLID, 5,
                            RGB(0.13 * 255, 0.69 * 255, 0.11 * 255))) {
}

DisconnectWindowWin::~DisconnectWindowWin() {
  EndDialog();
}

BOOL CALLBACK DisconnectWindowWin::DialogProc(HWND hwnd, UINT msg,
                                              WPARAM wParam, LPARAM lParam) {
  DisconnectWindowWin* win = NULL;
  if (msg == WM_INITDIALOG) {
    win = reinterpret_cast<DisconnectWindowWin*>(lParam);
    CHECK(win);
    SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)win);
  } else {
    LONG_PTR lp = GetWindowLongPtr(hwnd, DWLP_USER);
    win = reinterpret_cast<DisconnectWindowWin*>(lp);
  }
  if (win == NULL)
    return FALSE;
  return win->OnDialogMessage(hwnd, msg, wParam, lParam);
}

BOOL DisconnectWindowWin::OnDialogMessage(HWND hwnd, UINT msg,
                                          WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    // Ignore close messages.
    case WM_CLOSE:
      return TRUE;

    // Handle the Disconnect button.
    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case IDC_DISCONNECT:
          EndDialog();
          ShutdownHost();
          return TRUE;
      }
      return FALSE;

    // Ensure we don't try to use the HWND anymore.
    case WM_DESTROY:
      hwnd_ = NULL;
      return TRUE;

    // Handle the disconnect hot-key.
    case WM_HOTKEY:
      EndDialog();
      ShutdownHost();
      return TRUE;

    // Let the window be draggable by its client area by responding
    // that the entire window is the title bar.
    case WM_NCHITTEST:
      SetWindowLong(hwnd, DWL_MSGRESULT, HTCAPTION);
      return TRUE;

    case WM_PAINT:
      {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd_, &ps);
        RECT rect;
        GetClientRect(hwnd_, &rect);
        {
          base::win::ScopedSelectObject border(hdc, border_pen_);
          base::win::ScopedSelectObject brush(hdc, GetStockObject(NULL_BRUSH));
          RoundRect(hdc, rect.left, rect.top, rect.right - 1, rect.bottom - 1,
                    kWindowBorderRadius, kWindowBorderRadius);
        }
        EndPaint(hwnd_, &ps);
        return TRUE;
      }
  }
  return FALSE;
}

void DisconnectWindowWin::Show(ChromotingHost* host,
                               const DisconnectCallback& disconnect_callback,
                               const std::string& username) {
  CHECK(!hwnd_);
  disconnect_callback_ = disconnect_callback;

  // Load the dialog resource so that we can modify the RTL flags if necessary.
  // This is taken from chrome/default_plugin/install_dialog.cc
  HRSRC dialog_resource =
      FindResource(g_hModule, MAKEINTRESOURCE(IDD_DISCONNECT), RT_DIALOG);
  CHECK(dialog_resource);
  HGLOBAL dialog_template = LoadResource(g_hModule, dialog_resource);
  CHECK(dialog_template);
  DLGTEMPLATE* dialog_pointer =
      reinterpret_cast<DLGTEMPLATE*>(LockResource(dialog_template));
  CHECK(dialog_pointer);

  // The actual resource type is DLGTEMPLATEEX, but this is not defined in any
  // standard headers, so we treat it as a generic pointer and manipulate the
  // correct offsets explicitly.
  scoped_ptr<unsigned char> rtl_dialog_template;
  if (host->ui_strings().direction == UiStrings::RTL) {
    unsigned long dialog_template_size =
        SizeofResource(g_hModule, dialog_resource);
    rtl_dialog_template.reset(new unsigned char[dialog_template_size]);
    memcpy(rtl_dialog_template.get(), dialog_pointer, dialog_template_size);
    DWORD* rtl_dwords = reinterpret_cast<DWORD*>(rtl_dialog_template.get());
    rtl_dwords[2] |= (WS_EX_LAYOUTRTL | WS_EX_RTLREADING);
    dialog_pointer = reinterpret_cast<DLGTEMPLATE*>(rtl_dwords);
  }

  hwnd_ = CreateDialogIndirectParam(g_hModule, dialog_pointer, NULL,
                                    (DLGPROC)DialogProc, (LPARAM)this);
  CHECK(hwnd_);

  // Set up handler for Ctrl-Alt-Esc shortcut.
  if (!has_hotkey_ && RegisterHotKey(hwnd_, DISCONNECT_HOTKEY_ID,
                                     MOD_ALT | MOD_CONTROL, VK_ESCAPE)) {
    has_hotkey_ = true;
  }

  SetStrings(host->ui_strings(), username);

  // Try to center the window above the task-bar. If that fails, use the
  // primary monitor. If that fails (very unlikely), use the default position.
  HWND taskbar = FindWindow(L"Shell_TrayWnd", NULL);
  HMONITOR monitor = MonitorFromWindow(taskbar, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO monitor_info = {sizeof(monitor_info)};
  RECT window_rect;
  if (GetMonitorInfo(monitor, &monitor_info) &&
      GetWindowRect(hwnd_, &window_rect)) {
    int window_width = window_rect.right - window_rect.left;
    int window_height = window_rect.bottom - window_rect.top;
    int top = monitor_info.rcWork.bottom - window_height;
    int left = (monitor_info.rcWork.right + monitor_info.rcWork.left -
        window_width) / 2;
    SetWindowPos(hwnd_, NULL, left, top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
  }
  ShowWindow(hwnd_, SW_SHOW);
}

void DisconnectWindowWin::ShutdownHost() {
  CHECK(!disconnect_callback_.is_null());
  disconnect_callback_.Run();
}

static int GetControlTextWidth(HWND control) {
  RECT rect = {0, 0, 0, 0};
  WCHAR text[256];
  int result = GetWindowText(control, text, arraysize(text));
  if (result) {
    base::win::ScopedGetDC dc(control);
    base::win::ScopedSelectObject font(
        dc, (HFONT)SendMessage(control, WM_GETFONT, 0, 0));
    DrawText(dc, text, -1, &rect, DT_CALCRECT|DT_SINGLELINE);
  }
  return rect.right;
}

void DisconnectWindowWin::SetStrings(const UiStrings& strings,
                                     const std::string& username) {
  SetWindowText(hwnd_, strings.product_name.c_str());

  HWND hwndButton = GetDlgItem(hwnd_, IDC_DISCONNECT);
  CHECK(hwndButton);
  const WCHAR* label =
    has_hotkey_ ? strings.disconnect_button_text_plus_shortcut.c_str()
                : strings.disconnect_button_text.c_str();
  int button_old_required_width = GetControlTextWidth(hwndButton);
  SetWindowText(hwndButton, label);
  int button_new_required_width = GetControlTextWidth(hwndButton);

  HWND hwndSharingWith = GetDlgItem(hwnd_, IDC_DISCONNECT_SHARINGWITH);
  CHECK(hwndSharingWith);
  string16 text = ReplaceStringPlaceholders(
      strings.disconnect_message, UTF8ToUTF16(username), NULL);
  int label_old_required_width = GetControlTextWidth(hwndSharingWith);
  SetWindowText(hwndSharingWith, text.c_str());
  int label_new_required_width = GetControlTextWidth(hwndSharingWith);

  int label_width_delta = label_new_required_width - label_old_required_width;
  int button_width_delta =
      button_new_required_width - button_old_required_width;

  // Reposition the controls such that the label lies to the left of the
  // disconnect button (assuming LTR layout). The dialog template determines
  // the controls' spacing; update their size to fit the localized content.
  RECT label_rect;
  GetClientRect(hwndSharingWith, &label_rect);
  SetWindowPos(hwndSharingWith, NULL, 0, 0,
               label_rect.right + label_width_delta, label_rect.bottom,
               SWP_NOMOVE|SWP_NOZORDER);

  RECT button_rect;
  GetWindowRect(hwndButton, &button_rect);
  int button_width = button_rect.right - button_rect.left;
  int button_height = button_rect.bottom - button_rect.top;
  MapWindowPoints(NULL, hwnd_, reinterpret_cast<LPPOINT>(&button_rect), 2);
  SetWindowPos(hwndButton, NULL,
               button_rect.left + label_width_delta, button_rect.top,
               button_width + button_width_delta, button_height, SWP_NOZORDER);

  RECT window_rect;
  GetWindowRect(hwnd_, &window_rect);
  int width = (window_rect.right - window_rect.left) +
      button_width_delta + label_width_delta;
  int height = window_rect.bottom - window_rect.top;
  SetWindowPos(hwnd_, NULL, 0, 0, width, height, SWP_NOMOVE|SWP_NOZORDER);
  HRGN rgn = CreateRoundRectRgn(0, 0, width, height, kWindowBorderRadius,
                                kWindowBorderRadius);
  SetWindowRgn(hwnd_, rgn, TRUE);
}

void DisconnectWindowWin::Hide() {
  EndDialog();
}

void DisconnectWindowWin::EndDialog() {
  if (has_hotkey_) {
    UnregisterHotKey(hwnd_, DISCONNECT_HOTKEY_ID);
    has_hotkey_ = false;
  }

  if (hwnd_) {
    ::DestroyWindow(hwnd_);
    hwnd_ = NULL;
  }
}

scoped_ptr<DisconnectWindow> DisconnectWindow::Create() {
  return scoped_ptr<DisconnectWindow>(new DisconnectWindowWin());
}

}  // namespace remoting

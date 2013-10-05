// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_win.h"

#include <winuser.h>

#include "base/debug/stack_trace.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"

namespace views {
namespace corewm {

TooltipWin::TooltipWin(HWND parent)
    : parent_hwnd_(parent),
      tooltip_hwnd_(NULL),
      showing_(false) {
  memset(&toolinfo_, 0, sizeof(toolinfo_));
  toolinfo_.cbSize = sizeof(toolinfo_);
  toolinfo_.uFlags = TTF_IDISHWND | TTF_TRACK | TTF_ABSOLUTE;
  toolinfo_.uId = reinterpret_cast<UINT_PTR>(parent_hwnd_);
  toolinfo_.hwnd = parent_hwnd_;
  toolinfo_.lpszText = NULL;
  toolinfo_.lpReserved = NULL;
  SetRectEmpty(&toolinfo_.rect);
}

TooltipWin::~TooltipWin() {
  if (tooltip_hwnd_)
    DestroyWindow(tooltip_hwnd_);
}

bool TooltipWin::HandleNotify(int w_param, NMHDR* l_param, LRESULT* l_result) {
  if (tooltip_hwnd_ == NULL)
    return false;

  switch (l_param->code) {
    case TTN_POP:
      showing_ = false;
      return true;
    case TTN_SHOW:
      *l_result = TRUE;
      PositionTooltip();
      showing_ = true;
      return true;
    default:
      break;
  }
  return false;
}

bool TooltipWin::EnsureTooltipWindow() {
  if (tooltip_hwnd_)
    return true;

  tooltip_hwnd_ = CreateWindowEx(
      WS_EX_TRANSPARENT | l10n_util::GetExtendedTooltipStyles() | WS_EX_TOPMOST,
      TOOLTIPS_CLASS, NULL, TTS_NOPREFIX | WS_POPUP, 0, 0, 0, 0,
      parent_hwnd_, NULL, NULL, NULL);
  if (!tooltip_hwnd_) {
    LOG_GETLASTERROR(WARNING) << "tooltip creation failed, disabling tooltips";
    return false;
  }

  l10n_util::AdjustUIFontForWindow(tooltip_hwnd_);

  SendMessage(tooltip_hwnd_, TTM_ADDTOOL, 0,
              reinterpret_cast<LPARAM>(&toolinfo_));
  return true;
}

void TooltipWin::PositionTooltip() {
  // This code only runs for non-metro, so GetNativeScreen() is fine.
  gfx::Display display(
      gfx::Screen::GetNativeScreen()->GetDisplayNearestPoint(location_));

  DWORD tooltip_size = SendMessage(tooltip_hwnd_, TTM_GETBUBBLESIZE, 0,
                                   reinterpret_cast<LPARAM>(&toolinfo_));
  // 20 accounts for visible cursor size. I tried using SM_CYCURSOR but that's
  // way too big (32 on win7 default).
  // TODO(sky): figure out the right way to determine offset.
  const int initial_y = location_.y();
  gfx::Rect tooltip_bounds(location_.x(), initial_y + 20,
                           LOWORD(tooltip_size), HIWORD(tooltip_size));
  tooltip_bounds.AdjustToFit(display.work_area());
  if (tooltip_bounds.y() < initial_y)
    tooltip_bounds.set_y(initial_y - tooltip_bounds.height() - 2);
  SetWindowPos(tooltip_hwnd_, NULL, tooltip_bounds.x(), tooltip_bounds.y(), 0,
               0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void TooltipWin::SetText(aura::Window* window,
                         const base::string16& tooltip_text,
                         const gfx::Point& location) {
  if (!EnsureTooltipWindow())
    return;

  // See comment in header for details on why |location_| is needed.
  location_ = location;

  base::string16 adjusted_text(tooltip_text);
  base::i18n::AdjustStringForLocaleDirection(&adjusted_text);
  toolinfo_.lpszText = const_cast<WCHAR*>(adjusted_text.c_str());
  SendMessage(tooltip_hwnd_, TTM_SETTOOLINFO, 0,
              reinterpret_cast<LPARAM>(&toolinfo_));

  // This code only runs for non-metro, so GetNativeScreen() is fine.
  gfx::Display display(
      gfx::Screen::GetNativeScreen()->GetDisplayNearestPoint(location_));
  const gfx::Rect monitor_bounds = display.bounds();
  int max_width = (monitor_bounds.width() + 1) / 2;
  SendMessage(tooltip_hwnd_, TTM_SETMAXTIPWIDTH, 0, max_width);
}

void TooltipWin::Show() {
  if (!EnsureTooltipWindow())
    return;

  SendMessage(tooltip_hwnd_, TTM_TRACKACTIVATE,
              TRUE, reinterpret_cast<LPARAM>(&toolinfo_));
}

void TooltipWin::Hide() {
  if (!EnsureTooltipWindow())
    return;

  SendMessage(tooltip_hwnd_, TTM_TRACKACTIVATE, FALSE,
              reinterpret_cast<LPARAM>(&toolinfo_));
}

bool TooltipWin::IsVisible() {
  return showing_;
}

}  // namespace corewm
}  // namespace views

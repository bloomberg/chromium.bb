// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/tooltip_manager_win.h"

#include <windowsx.h>

#include <limits>
#include <vector>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_select_object.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/win/dpi.h"
#include "ui/gfx/win/hwnd_util.h"
#include "ui/gfx/win/scoped_set_map_mode.h"
#include "ui/views/view.h"
#include "ui/views/widget/monitor_win.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

static int tooltip_height_ = 0;

// Maximum number of lines we allow in the tooltip.
const size_t kMaxLines = 6;

// Trims the tooltip to fit, setting |text| to the clipped result, |max_width|
// to the width (in pixels) of the clipped text and |line_count| to the number
// of lines of text in the tooltip. |available_width| gives the space available
// for the tooltip.
void TrimTooltipToFit(const gfx::FontList& font_list,
                      int available_width,
                      base::string16* text,
                      int* max_width,
                      int* line_count) {
  *max_width = 0;
  *line_count = 0;

  TooltipManager::TrimTooltipText(text);

  // Split the string into at most kMaxLines lines.
  std::vector<base::string16> lines;
  base::SplitString(*text, '\n', &lines);
  if (lines.size() > kMaxLines)
    lines.resize(kMaxLines);
  *line_count = static_cast<int>(lines.size());

  // Format each line to fit.
  base::string16 result;
  for (std::vector<base::string16>::iterator i = lines.begin();
       i != lines.end(); ++i) {
    base::string16 elided_text =
        gfx::ElideText(*i, font_list, available_width, gfx::ELIDE_AT_END);
    *max_width = std::max(*max_width,
                          gfx::GetStringWidth(elided_text, font_list));
    if (!result.empty())
      result.push_back('\n');
    result.append(elided_text);
  }
  *text = result;
}

}  // namespace

// static
int TooltipManager::GetTooltipHeight() {
  DCHECK_GT(tooltip_height_, 0);
  return tooltip_height_;
}

static gfx::Font DetermineDefaultFont() {
  HWND window = CreateWindowEx(
      WS_EX_TRANSPARENT | l10n_util::GetExtendedTooltipStyles(),
      TOOLTIPS_CLASS, NULL, 0 , 0, 0, 0, 0, NULL, NULL, NULL, NULL);
  if (!window)
    return gfx::Font();
  HFONT hfont = reinterpret_cast<HFONT>(SendMessage(window, WM_GETFONT, 0, 0));
  gfx::Font font = hfont ? gfx::Font(hfont) : gfx::Font();
  DestroyWindow(window);
  return font;
}

TooltipManagerWin::TooltipManagerWin(Widget* widget)
    : widget_(widget),
      tooltip_hwnd_(NULL),
      last_mouse_pos_(-1, -1),
      tooltip_showing_(false),
      last_tooltip_view_(NULL),
      last_view_out_of_sync_(false),
      tooltip_width_(0) {
  DCHECK(widget);
  DCHECK(widget->GetNativeView());
}

TooltipManagerWin::~TooltipManagerWin() {
  if (tooltip_hwnd_)
    DestroyWindow(tooltip_hwnd_);
}

bool TooltipManagerWin::Init() {
  DCHECK(!tooltip_hwnd_);
  // Create the tooltip control.
  tooltip_hwnd_ = CreateWindowEx(
      WS_EX_TRANSPARENT | l10n_util::GetExtendedTooltipStyles(),
      TOOLTIPS_CLASS, NULL, TTS_NOPREFIX, 0, 0, 0, 0,
      GetParent(), NULL, NULL, NULL);
  if (!tooltip_hwnd_)
    return false;

  l10n_util::AdjustUIFontForWindow(tooltip_hwnd_);

  // This effectively turns off clipping of tooltips. We need this otherwise
  // multi-line text (\r\n) won't work right. The size doesn't really matter
  // (just as long as its bigger than the monitor's width) as we clip to the
  // screen size before rendering.
  SendMessage(tooltip_hwnd_, TTM_SETMAXTIPWIDTH, 0,
              std::numeric_limits<int16>::max());

  // Add one tool that is used for all tooltips.
  toolinfo_.cbSize = sizeof(toolinfo_);
  toolinfo_.uFlags = TTF_TRANSPARENT | TTF_IDISHWND;
  toolinfo_.hwnd = GetParent();
  toolinfo_.uId = reinterpret_cast<UINT_PTR>(GetParent());
  // Setting this tells windows to call GetParent() back (using a WM_NOTIFY
  // message) for the actual tooltip contents.
  toolinfo_.lpszText = LPSTR_TEXTCALLBACK;
  toolinfo_.lpReserved = NULL;
  SetRectEmpty(&toolinfo_.rect);
  SendMessage(tooltip_hwnd_, TTM_ADDTOOL, 0, (LPARAM)&toolinfo_);
  return true;
}

gfx::NativeView TooltipManagerWin::GetParent() {
  return widget_->GetNativeView();
}

const gfx::FontList& TooltipManagerWin::GetFontList() const {
  static gfx::FontList* font_list = NULL;
  if (!font_list)
    font_list = new gfx::FontList(DetermineDefaultFont());
  return *font_list;
}

void TooltipManagerWin::UpdateTooltip() {
  // Set last_view_out_of_sync_ to indicate the view is currently out of sync.
  // This doesn't update the view under the mouse immediately as it may cause
  // timing problems.
  last_view_out_of_sync_ = true;
  last_tooltip_view_ = NULL;
  // Hide the tooltip.
  SendMessage(tooltip_hwnd_, TTM_POP, 0, 0);
}

void TooltipManagerWin::TooltipTextChanged(View* view) {
  if (view == last_tooltip_view_)
    UpdateTooltip(last_mouse_pos_);
}

LRESULT TooltipManagerWin::OnNotify(int w_param,
                                    NMHDR* l_param,
                                    bool* handled) {
  *handled = false;
  if (l_param->hwndFrom != tooltip_hwnd_)
    return 0;

  switch (l_param->code) {
    case TTN_GETDISPINFO: {
      if (last_view_out_of_sync_) {
        // View under the mouse is out of sync, determine it now.
        View* root_view = widget_->GetRootView();
        last_tooltip_view_ =
            root_view->GetTooltipHandlerForPoint(last_mouse_pos_);
        last_view_out_of_sync_ = false;
      }
      // Tooltip control is asking for the tooltip to display.
      NMTTDISPINFOW* tooltip_info =
          reinterpret_cast<NMTTDISPINFOW*>(l_param);
      // Initialize the string, if we have a valid tooltip the string will
      // get reset below.
      tooltip_info->szText[0] = TEXT('\0');
      tooltip_text_.clear();
      tooltip_info->lpszText = NULL;
      clipped_text_.clear();
      if (last_tooltip_view_ != NULL) {
        tooltip_text_.clear();
        // Mouse is over a View, ask the View for its tooltip.
        gfx::Point view_loc = last_mouse_pos_;
        View::ConvertPointToTarget(widget_->GetRootView(),
                                   last_tooltip_view_, &view_loc);
        if (last_tooltip_view_->GetTooltipText(view_loc, &tooltip_text_) &&
            !tooltip_text_.empty()) {
          // View has a valid tip, copy it into TOOLTIPINFO.
          clipped_text_ = tooltip_text_;
          gfx::Point screen_loc = last_mouse_pos_;
          View::ConvertPointToScreen(widget_->GetRootView(), &screen_loc);
          TrimTooltipToFit(
              GetFontList(),
              GetMaxWidth(screen_loc.x(), screen_loc.y(),
                          widget_->GetNativeView()),
              &clipped_text_, &tooltip_width_, &line_count_);
          // Adjust the clipped tooltip text for locale direction.
          base::i18n::AdjustStringForLocaleDirection(&clipped_text_);
          tooltip_info->lpszText = const_cast<WCHAR*>(clipped_text_.c_str());
        } else {
          tooltip_text_.clear();
        }
      }
      *handled = true;
      return 0;
    }
    case TTN_POP:
      tooltip_showing_ = false;
      *handled = true;
      return 0;
    case TTN_SHOW: {
      *handled = true;
      tooltip_showing_ = true;
      // The tooltip is about to show, allow the view to position it
      gfx::Point text_origin;
      if (tooltip_height_ == 0)
        tooltip_height_ = CalcTooltipHeight();
      gfx::Point view_loc = last_mouse_pos_;
      View::ConvertPointToTarget(widget_->GetRootView(),
                                 last_tooltip_view_, &view_loc);
      if (last_tooltip_view_->GetTooltipTextOrigin(view_loc, &text_origin) &&
          SetTooltipPosition(text_origin.x(), text_origin.y())) {
        // Return true, otherwise the rectangle we specified is ignored.
        return TRUE;
      }
      return 0;
    }
    default:
      // Fall through.
      break;
  }
  return 0;
}

bool TooltipManagerWin::SetTooltipPosition(int text_x, int text_y) {
  // NOTE: this really only tests that the y location fits on screen, but that
  // is good enough for our usage.

  // Calculate the bounds the tooltip will get.
  gfx::Point view_loc;
  View::ConvertPointToScreen(last_tooltip_view_, &view_loc);
  view_loc = gfx::win::DIPToScreenPoint(view_loc);
  RECT bounds = { view_loc.x() + text_x,
                  view_loc.y() + text_y,
                  view_loc.x() + text_x + tooltip_width_,
                  view_loc.y() + line_count_ * GetTooltipHeight() };
  SendMessage(tooltip_hwnd_, TTM_ADJUSTRECT, TRUE, (LPARAM)&bounds);

  // Make sure the rectangle completely fits on the current monitor. If it
  // doesn't, return false so that windows positions the tooltip at the
  // default location.
  gfx::Rect monitor_bounds =
      views::GetMonitorBoundsForRect(gfx::Rect(bounds.left, bounds.right,
                                                  0, 0));
  if (!monitor_bounds.Contains(gfx::Rect(bounds))) {
    return false;
  }

  ::SetWindowPos(tooltip_hwnd_, NULL, bounds.left, bounds.top, 0, 0,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE);
  return true;
}

int TooltipManagerWin::CalcTooltipHeight() {
  // Ask the tooltip for its font.
  int height;
  HFONT hfont = reinterpret_cast<HFONT>(
      SendMessage(tooltip_hwnd_, WM_GETFONT, 0, 0));
  if (hfont != NULL) {
    base::win::ScopedGetDC dc(tooltip_hwnd_);
    base::win::ScopedSelectObject font(dc, hfont);
    gfx::ScopedSetMapMode mode(dc, MM_TEXT);
    TEXTMETRIC font_metrics;
    GetTextMetrics(dc, &font_metrics);
    height = font_metrics.tmHeight;
  } else {
    // Tooltip is using the system font. Use gfx::Font, which should pick
    // up the system font.
    height = gfx::Font().GetHeight();
  }
  // Get the margins from the tooltip
  RECT tooltip_margin;
  SendMessage(tooltip_hwnd_, TTM_GETMARGIN, 0, (LPARAM)&tooltip_margin);
  return height + tooltip_margin.top + tooltip_margin.bottom;
}

void TooltipManagerWin::UpdateTooltip(const gfx::Point& mouse_pos) {
  View* root_view = widget_->GetRootView();
  View* view = root_view->GetTooltipHandlerForPoint(mouse_pos);
  if (view != last_tooltip_view_) {
    // NOTE: This *must* be sent regardless of the visibility of the tooltip.
    // It triggers Windows to ask for the tooltip again.
    SendMessage(tooltip_hwnd_, TTM_POP, 0, 0);
    last_tooltip_view_ = view;
  } else if (last_tooltip_view_ != NULL) {
    // Tooltip is showing, and mouse is over the same view. See if the tooltip
    // text has changed.
    gfx::Point view_point = mouse_pos;
    View::ConvertPointToTarget(root_view, last_tooltip_view_, &view_point);
    base::string16 new_tooltip_text;
    bool has_tooltip_text =
        last_tooltip_view_->GetTooltipText(view_point, &new_tooltip_text);
    if (!has_tooltip_text || (new_tooltip_text != tooltip_text_)) {
      // The text has changed, hide the popup.
      SendMessage(tooltip_hwnd_, TTM_POP, 0, 0);
      if (has_tooltip_text && !new_tooltip_text.empty() && tooltip_showing_) {
        // New text is valid, show the popup.
        SendMessage(tooltip_hwnd_, TTM_POPUP, 0, 0);
      }
    }
  }
}

void TooltipManagerWin::OnMouse(UINT u_msg, WPARAM w_param, LPARAM l_param) {
  gfx::Point mouse_pos_in_pixels(l_param);
  gfx::Point mouse_pos = gfx::win::ScreenToDIPPoint(mouse_pos_in_pixels);

  if (u_msg >= WM_NCMOUSEMOVE && u_msg <= WM_NCXBUTTONDBLCLK) {
    // NC message coordinates are in screen coordinates.
    POINT temp = mouse_pos_in_pixels.ToPOINT();
    ::MapWindowPoints(HWND_DESKTOP, GetParent(), &temp, 1);
    mouse_pos_in_pixels.SetPoint(temp.x, temp.y);
    mouse_pos = gfx::win::ScreenToDIPPoint(mouse_pos_in_pixels);
  }

  if (u_msg != WM_MOUSEMOVE || last_mouse_pos_ != mouse_pos) {
    last_mouse_pos_ = mouse_pos;
    UpdateTooltip(mouse_pos);
  }
  // Forward the message onto the tooltip.
  MSG msg;
  msg.hwnd = GetParent();
  msg.message = u_msg;
  msg.wParam = w_param;
  msg.lParam = l_param;
  SendMessage(tooltip_hwnd_, TTM_RELAYEVENT, 0, (LPARAM)&msg);
}

}  // namespace views

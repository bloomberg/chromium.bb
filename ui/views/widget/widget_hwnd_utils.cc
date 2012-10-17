// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_hwnd_utils.h"

#include "base/win/windows_version.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/win/hwnd_message_handler.h"

namespace views {

namespace {

void CalculateWindowStylesFromInitParams(
    const Widget::InitParams& params,
    WidgetDelegate* widget_delegate,
    internal::NativeWidgetDelegate* native_widget_delegate,
    DWORD* style,
    DWORD* ex_style,
    DWORD* class_style) {
  *style = WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
  *ex_style = 0;
  *class_style = CS_DBLCLKS;

  // Set type-independent style attributes.
  if (params.child)
    *style |= WS_CHILD;
  if (params.show_state == ui::SHOW_STATE_MAXIMIZED)
    *style |= WS_MAXIMIZE;
  if (params.show_state == ui::SHOW_STATE_MINIMIZED)
    *style |= WS_MINIMIZE;
  if (!params.accept_events)
    *ex_style |= WS_EX_TRANSPARENT;
  if (!params.can_activate)
    *ex_style |= WS_EX_NOACTIVATE;
  if (params.keep_on_top)
    *ex_style |= WS_EX_TOPMOST;
  if (params.mirror_origin_in_rtl)
    *ex_style |= l10n_util::GetExtendedTooltipStyles();
#if !defined(USE_AURA)
  // TODO(beng): layered windows with aura. http://crbug.com/154069
  if (params.transparent)
    *ex_style |= WS_EX_LAYERED;
#endif
  if (params.has_dropshadow) {
    *class_style |= (base::win::GetVersion() < base::win::VERSION_XP) ?
        0 : CS_DROPSHADOW;
  }

  // Set type-dependent style attributes.
  switch (params.type) {
    case Widget::InitParams::TYPE_PANEL:
      *ex_style |= WS_EX_TOPMOST;
      if (params.remove_standard_frame) {
        *style |= WS_POPUP;
        break;
      }
      // Else, no break. Fall through to TYPE_WINDOW.
    case Widget::InitParams::TYPE_WINDOW: {
      *style |= WS_SYSMENU | WS_CAPTION;
      bool can_resize = widget_delegate->CanResize();
      bool can_maximize = widget_delegate->CanMaximize();
      if (can_maximize) {
        *style |= WS_OVERLAPPEDWINDOW;
      } else if (can_resize || params.remove_standard_frame) {
        *style |= WS_OVERLAPPED | WS_THICKFRAME;
      }
      if (native_widget_delegate->IsDialogBox()) {
        *style |= DS_MODALFRAME;
        // NOTE: Turning this off means we lose the close button, which is bad.
        // Turning it on though means the user can maximize or size the window
        // from the system menu, which is worse. We may need to provide our own
        // menu to get the close button to appear properly.
        // style &= ~WS_SYSMENU;

        // Set the WS_POPUP style for modal dialogs. This ensures that the owner
        // window is activated on destruction. This style should not be set for
        // non-modal non-top-level dialogs like constrained windows.
        *style |= native_widget_delegate->IsModal() ? WS_POPUP : 0;
      }
      *ex_style |=
          native_widget_delegate->IsDialogBox() ? WS_EX_DLGMODALFRAME : 0;
      break;
    }
    case Widget::InitParams::TYPE_CONTROL:
      *style |= WS_VISIBLE;
      break;
    case Widget::InitParams::TYPE_WINDOW_FRAMELESS:
      *style |= WS_POPUP;
      break;
    case Widget::InitParams::TYPE_BUBBLE:
      *style |= WS_POPUP;
      *style |= WS_CLIPCHILDREN;
      break;
    case Widget::InitParams::TYPE_POPUP:
      *style |= WS_POPUP;
      *ex_style |= WS_EX_TOOLWINDOW;
      break;
    case Widget::InitParams::TYPE_MENU:
      *style |= WS_POPUP;
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace

bool DidClientAreaSizeChange(const WINDOWPOS* window_pos) {
  return !(window_pos->flags & SWP_NOSIZE) ||
         window_pos->flags & SWP_FRAMECHANGED;
}

void ConfigureWindowStyles(
    HWNDMessageHandler* handler,
    const Widget::InitParams& params,
    WidgetDelegate* widget_delegate,
    internal::NativeWidgetDelegate* native_widget_delegate) {
  // Configure the HWNDMessageHandler with the appropriate
  DWORD style = 0;
  DWORD ex_style = 0;
  DWORD class_style = 0;
  CalculateWindowStylesFromInitParams(params, widget_delegate,
                                      native_widget_delegate, &style, &ex_style,
                                      &class_style);
  handler->set_initial_class_style(class_style);
  handler->set_window_style(handler->window_style() | style);
  handler->set_window_ex_style(handler->window_ex_style() | ex_style);
}

}  // namespace views

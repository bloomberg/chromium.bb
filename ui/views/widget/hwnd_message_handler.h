// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_HWND_MESSAGE_HANDLER_H_
#define UI_VIEWS_WIDGET_HWND_MESSAGE_HANDLER_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>
#include <windows.h>

#include "base/basictypes.h"
#include "ui/views/views_export.h"

namespace views {

class HWNDMessageHandlerDelegate;

// An object that handles messages for a HWND that implements the views
// "Custom Frame" look. The purpose of this class is to isolate the windows-
// specific message handling from the code that wraps it. It is intended to be
// used by both a views::NativeWidget and an aura::RootWindowHost
// implementation.
// TODO(beng): This object should eventually *become* the WindowImpl.
class VIEWS_EXPORT HWNDMessageHandler {
 public:
  explicit HWNDMessageHandler(HWNDMessageHandlerDelegate* delegate);
  ~HWNDMessageHandler();

  // Message Handlers.
  void OnActivate(UINT action, BOOL minimized, HWND window);
  // TODO(beng): Once this object becomes the WindowImpl, these methods can
  //             be made private.
  void OnActivateApp(BOOL active, DWORD thread_id);
  // TODO(beng): return BOOL is temporary until this object becomes a
  //             WindowImpl.
  BOOL OnAppCommand(HWND window, short command, WORD device, int keystate);
  void OnCancelMode();
  void OnCaptureChanged(HWND window);
  void OnClose();
  void OnCommand(UINT notification_code, int command, HWND window);
  void OnDestroy();
  void OnDisplayChange(UINT bits_per_pixel, const CSize& screen_size);
  void OnDwmCompositionChanged(UINT msg, WPARAM w_param, LPARAM l_param);
  void OnEndSession(BOOL ending, UINT logoff);
  void OnEnterSizeMove();
  LRESULT OnEraseBkgnd(HDC dc);
  void OnExitMenuLoop(BOOL is_track_popup_menu);
  void OnExitSizeMove();
  LRESULT OnImeMessages(UINT message, WPARAM w_param, LPARAM l_param);
  void OnInputLangChange(DWORD character_set, HKL input_language_id);
  void OnMove(const CPoint& point);
  void OnMoving(UINT param, const RECT* new_bounds);
  LRESULT OnNCUAHDrawCaption(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnNCUAHDrawFrame(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnPowerBroadcast(DWORD power_event, DWORD data);
  void OnThemeChanged();
  void OnVScroll(int scroll_type, short position, HWND scrollbar);

  // TODO(beng): Can be removed once this object becomes the WindowImpl.
  bool remove_standard_frame() const { return remove_standard_frame_; }
  void set_remove_standard_frame(bool remove_standard_frame) {
    remove_standard_frame_ = remove_standard_frame;
  }

 private:
  // TODO(beng): This won't be a style violation once this object becomes the
  //             WindowImpl.
  HWND hwnd();

  // TODO(beng): Remove once this class becomes the WindowImpl.
  LRESULT DefWindowProcWithRedrawLock(UINT message,
                                      WPARAM w_param,
                                      LPARAM l_param);

  // TODO(beng): Remove once this class becomes the WindowImpl.
  void SetMsgHandled(BOOL handled);

  HWNDMessageHandlerDelegate* delegate_;

  bool remove_standard_frame_;

  DISALLOW_COPY_AND_ASSIGN(HWNDMessageHandler);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_HWND_MESSAGE_HANDLER_H_

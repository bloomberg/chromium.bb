// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_H_
#define UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_H_

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

  bool IsActive() const;
  bool IsVisible() const;

  // Tells the HWND its client area has changed.
  void SendFrameChanged();

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
  LRESULT OnCreate(CREATESTRUCT* create_struct);
  void OnDestroy();
  void OnDisplayChange(UINT bits_per_pixel, const CSize& screen_size);
  LRESULT OnDwmCompositionChanged(UINT msg, WPARAM w_param, LPARAM l_param);
  void OnEndSession(BOOL ending, UINT logoff);
  void OnEnterSizeMove();
  LRESULT OnEraseBkgnd(HDC dc);
  void OnExitMenuLoop(BOOL is_track_popup_menu);
  void OnExitSizeMove();
  void OnHScroll(int scroll_type, short position, HWND scrollbar);
  void OnGetMinMaxInfo(MINMAXINFO* minmax_info);
  LRESULT OnGetObject(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnImeMessages(UINT message, WPARAM w_param, LPARAM l_param);
  void OnInitMenu(HMENU menu);
  void OnInitMenuPopup();
  void OnInputLangChange(DWORD character_set, HKL input_language_id);
  LRESULT OnKeyEvent(UINT message, WPARAM w_param, LPARAM l_param);
  void OnKillFocus(HWND focused_window);
  LRESULT OnMouseActivate(UINT message, WPARAM w_param, LPARAM l_param);
  void OnMove(const CPoint& point);
  void OnMoving(UINT param, const RECT* new_bounds);
  LRESULT OnNCActivate(BOOL active);
  LRESULT OnNCHitTest(const CPoint& point);
  LRESULT OnNCUAHDrawCaption(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnNCUAHDrawFrame(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnNotify(int w_param, NMHDR* l_param);
  LRESULT OnPowerBroadcast(DWORD power_event, DWORD data);
  LRESULT OnReflectedMessage(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnSetCursor(UINT message, WPARAM w_param, LPARAM l_param);
  void OnSetFocus(HWND last_focused_window);
  LRESULT OnSetIcon(UINT size_type, HICON new_icon);
  LRESULT OnSetText(const wchar_t* text);
  void OnSettingChange(UINT flags, const wchar_t* section);
  void OnSize(UINT param, const CSize& size);
  void OnThemeChanged();
  void OnVScroll(int scroll_type, short position, HWND scrollbar);

  // TODO(beng): Can be removed once this object becomes the WindowImpl.
  bool remove_standard_frame() const { return remove_standard_frame_; }
  void set_remove_standard_frame(bool remove_standard_frame) {
    remove_standard_frame_ = remove_standard_frame;
  }

  // Resets the window region for the current widget bounds if necessary.
  // If |force| is true, the window region is reset to NULL even for native
  // frame windows.
  void ResetWindowRegion(bool force);

 private:
  // TODO(beng): This won't be a style violation once this object becomes the
  //             WindowImpl.
  HWND hwnd();
  HWND hwnd() const;

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

#endif  // UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_H_

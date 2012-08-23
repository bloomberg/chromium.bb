// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_H_
#define UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_H_

#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>
#include <windows.h>

#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/ime/input_method_delegate.h"
#include "ui/views/views_export.h"

namespace views {

class HWNDMessageHandlerDelegate;
class InputMethod;

VIEWS_EXPORT bool IsAeroGlassEnabled();

// An object that handles messages for a HWND that implements the views
// "Custom Frame" look. The purpose of this class is to isolate the windows-
// specific message handling from the code that wraps it. It is intended to be
// used by both a views::NativeWidget and an aura::RootWindowHost
// implementation.
// TODO(beng): This object should eventually *become* the WindowImpl.
class VIEWS_EXPORT HWNDMessageHandler : public internal::InputMethodDelegate {
 public:
  explicit HWNDMessageHandler(HWNDMessageHandlerDelegate* delegate);
  ~HWNDMessageHandler();

  bool IsVisible() const;
  bool IsActive() const;
  bool IsMinimized() const;
  bool IsMaximized() const;

  // Tells the HWND its client area has changed.
  void SendFrameChanged();

  void SetCapture();
  void ReleaseCapture();
  bool HasCapture() const;

  InputMethod* CreateInputMethod();

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
  LRESULT OnMouseRange(UINT message, WPARAM w_param, LPARAM l_param);
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
  LRESULT OnTouchEvent(UINT message, WPARAM w_param, LPARAM l_param);
  void OnVScroll(int scroll_type, short position, HWND scrollbar);
  void OnWindowPosChanged(WINDOWPOS* window_pos);

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
  typedef std::set<DWORD> TouchIDs;

  // TODO(beng): remove once this is the WindowImpl.
  friend class NativeWidgetWin;

  // Overridden from internal::InputMethodDelegate
  virtual void DispatchKeyEventPostIME(const ui::KeyEvent& key) OVERRIDE;

  // Start tracking all mouse events so that this window gets sent mouse leave
  // messages too.
  void TrackMouseEvents(DWORD mouse_tracking_flags);

  // Responds to the client area changing size, either at window creation time
  // or subsequently.
  void ClientAreaSizeChanged();

  // Calls DefWindowProc, safely wrapping the call in a ScopedRedrawLock to
  // prevent frame flicker. DefWindowProc handling can otherwise render the
  // classic-look window title bar directly.
  LRESULT DefWindowProcWithRedrawLock(UINT message,
                                      WPARAM w_param,
                                      LPARAM l_param);

  // Lock or unlock the window from being able to redraw itself in response to
  // updates to its invalid region.
  class ScopedRedrawLock;
  void LockUpdates(bool force);
  void UnlockUpdates(bool force);

  // TODO(beng): This won't be a style violation once this object becomes the
  //             WindowImpl.
  HWND hwnd();
  HWND hwnd() const;

  // TODO(beng): Remove once this class becomes the WindowImpl.
  void SetMsgHandled(BOOL handled);

  HWNDMessageHandlerDelegate* delegate_;

  bool remove_standard_frame_;

  // The flags currently being used with TrackMouseEvent to track mouse
  // messages. 0 if there is no active tracking. The value of this member is
  // used when tracking is canceled.
  DWORD active_mouse_tracking_flags_;

  // Set to true when the user presses the right mouse button on the caption
  // area. We need this so we can correctly show the context menu on mouse-up.
  bool is_right_mouse_pressed_on_caption_;

  // The set of touch devices currently down.
  TouchIDs touch_ids_;

  // Represents the number of ScopedRedrawLocks active against this widget.
  // If this is greater than zero, the widget should be locked against updates.
  int lock_updates_count_;

  // This flag can be initialized and checked after certain operations (such as
  // DefWindowProc) to avoid stack-controlled functions (such as unlocking the
  // Window with a ScopedRedrawLock) after destruction.
  bool* destroyed_;

  DISALLOW_COPY_AND_ASSIGN(HWNDMessageHandler);
};

}  // namespace views

#endif  // UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_H_

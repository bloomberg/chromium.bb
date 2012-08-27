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
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/accessibility/accessibility_types.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/rect.h"
#include "ui/views/ime/input_method_delegate.h"
#include "ui/views/views_export.h"

namespace gfx {
class Canvas;
class Insets;
}

namespace views {

class FullscreenHandler;
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

  void Init(const gfx::Rect& bounds);
  void InitModalType(ui::ModalType modal_type);

  void CloseNow();

  gfx::Rect GetWindowBoundsInScreen() const;
  gfx::Rect GetClientAreaBoundsInScreen() const;
  gfx::Rect GetRestoredBounds() const;
  void GetWindowPlacement(gfx::Rect* bounds,
                          ui::WindowShowState* show_state) const;
  gfx::Rect GetWorkAreaBoundsInScreen() const;

  void SetBounds(const gfx::Rect& bounds);
  void SetSize(const gfx::Size& size);
  void CenterWindow(const gfx::Size& size);

  void SetRegion(HRGN rgn);

  void StackAbove(HWND other_hwnd);
  void StackAtTop();

  void ShowMaximizedWithBounds(const gfx::Rect& bounds);
  void Hide();

  void Maximize();
  void Minimize();
  void Restore();

  void Activate();
  void Deactivate();

  void SetAlwaysOnTop(bool on_top);

  bool IsVisible() const;
  bool IsActive() const;
  bool IsMinimized() const;
  bool IsMaximized() const;

  bool RunMoveLoop(const gfx::Point& drag_offset);

  // Tells the HWND its client area has changed.
  void SendFrameChanged();

  void FlashFrame(bool flash);

  void ClearNativeFocus();
  void FocusHWND(HWND hwnd);

  void SetCapture();
  void ReleaseCapture();
  bool HasCapture() const;

  FullscreenHandler* fullscreen_handler() { return fullscreen_handler_.get(); }

  void SetVisibilityChangedAnimationsEnabled(bool enabled);

  InputMethod* CreateInputMethod();

  void SendNativeAccessibilityEvent(int id,
                                    ui::AccessibilityTypes::Event event_type);

  void SetCursor(HCURSOR cursor);

  void FrameTypeChanged();

  // Disable Layered Window updates by setting to false.
  void set_can_update_layered_window(bool can_update_layered_window) {
    can_update_layered_window_ = can_update_layered_window;
  }
  void SchedulePaintInRect(const gfx::Rect& rect);
  void SetOpacity(BYTE opacity);

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
  LRESULT OnNCCalcSize(BOOL mode, LPARAM l_param);
  LRESULT OnNCHitTest(const CPoint& point);
  void OnNCPaint(HRGN rgn);
  LRESULT OnNCUAHDrawCaption(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnNCUAHDrawFrame(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnNotify(int w_param, NMHDR* l_param);
  void OnPaint(HDC dc);
  LRESULT OnPowerBroadcast(DWORD power_event, DWORD data);
  LRESULT OnReflectedMessage(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnSetCursor(UINT message, WPARAM w_param, LPARAM l_param);
  void OnSetFocus(HWND last_focused_window);
  LRESULT OnSetIcon(UINT size_type, HICON new_icon);
  LRESULT OnSetText(const wchar_t* text);
  void OnSettingChange(UINT flags, const wchar_t* section);
  void OnSize(UINT param, const CSize& size);
  void OnSysCommand(UINT notification_code, const CPoint& point);
  void OnThemeChanged();
  LRESULT OnTouchEvent(UINT message, WPARAM w_param, LPARAM l_param);
  void OnVScroll(int scroll_type, short position, HWND scrollbar);
  void OnWindowPosChanging(WINDOWPOS* window_pos);
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

  // TODO(beng): This won't be a style violation once this object becomes the
  //             WindowImpl.
  HWND hwnd();
  HWND hwnd() const;

 private:
  typedef std::set<DWORD> TouchIDs;

  // TODO(beng): remove once this is the WindowImpl.
  friend class NativeWidgetWin;

  // Overridden from internal::InputMethodDelegate
  virtual void DispatchKeyEventPostIME(const ui::KeyEvent& key) OVERRIDE;

  // Executes the specified SC_command.
  void ExecuteSystemMenuCommand(int command);

  // Start tracking all mouse events so that this window gets sent mouse leave
  // messages too.
  void TrackMouseEvents(DWORD mouse_tracking_flags);

  // Responds to the client area changing size, either at window creation time
  // or subsequently.
  void ClientAreaSizeChanged();

  // Returns the insets of the client area relative to the non-client area of
  // the window.
  gfx::Insets GetClientAreaInsets() const;

  // Calls DefWindowProc, safely wrapping the call in a ScopedRedrawLock to
  // prevent frame flicker. DefWindowProc handling can otherwise render the
  // classic-look window title bar directly.
  LRESULT DefWindowProcWithRedrawLock(UINT message,
                                      WPARAM w_param,
                                      LPARAM l_param);

  // Notifies any owned windows that we're closing.
  void NotifyOwnedWindowsParentClosing();

  // Lock or unlock the window from being able to redraw itself in response to
  // updates to its invalid region.
  class ScopedRedrawLock;
  void LockUpdates(bool force);
  void UnlockUpdates(bool force);

  // Stops ignoring SetWindowPos() requests (see below).
  void StopIgnoringPosChanges() { ignore_window_pos_changes_ = false; }

  // Synchronously paints the invalid contents of the Widget.
  void RedrawInvalidRect();

  // Synchronously updates the invalid contents of the Widget. Valid for
  // layered windows only.
  void RedrawLayeredWindowContents();

  // TODO(beng): Remove once this class becomes the WindowImpl.
  void SetMsgHandled(BOOL handled);

  HWNDMessageHandlerDelegate* delegate_;

  scoped_ptr<FullscreenHandler> fullscreen_handler_;

  bool remove_standard_frame_;

  // The last cursor that was active before the current one was selected. Saved
  // so that we can restore it.
  HCURSOR previous_cursor_;

  // Event handling ------------------------------------------------------------

  // The flags currently being used with TrackMouseEvent to track mouse
  // messages. 0 if there is no active tracking. The value of this member is
  // used when tracking is canceled.
  DWORD active_mouse_tracking_flags_;

  // Set to true when the user presses the right mouse button on the caption
  // area. We need this so we can correctly show the context menu on mouse-up.
  bool is_right_mouse_pressed_on_caption_;

  // The set of touch devices currently down.
  TouchIDs touch_ids_;

  // ScopedRedrawLock ----------------------------------------------------------

  // Represents the number of ScopedRedrawLocks active against this widget.
  // If this is greater than zero, the widget should be locked against updates.
  int lock_updates_count_;

  // This flag can be initialized and checked after certain operations (such as
  // DefWindowProc) to avoid stack-controlled functions (such as unlocking the
  // Window with a ScopedRedrawLock) after destruction.
  bool* destroyed_;

  // Window resizing -----------------------------------------------------------

  // When true, this flag makes us discard incoming SetWindowPos() requests that
  // only change our position/size.  (We still allow changes to Z-order,
  // activation, etc.)
  bool ignore_window_pos_changes_;

  // The following factory is used to ignore SetWindowPos() calls for short time
  // periods.
  base::WeakPtrFactory<HWNDMessageHandler> ignore_pos_changes_factory_;

  // The last-seen monitor containing us, and its rect and work area.  These are
  // used to catch updates to the rect and work area and react accordingly.
  HMONITOR last_monitor_;
  gfx::Rect last_monitor_rect_, last_work_area_;

  // Layered windows -----------------------------------------------------------

  // Should we keep an off-screen buffer? This is false by default, set to true
  // when WS_EX_LAYERED is specified before the native window is created.
  //
  // NOTE: this is intended to be used with a layered window (a window with an
  // extended window style of WS_EX_LAYERED). If you are using a layered window
  // and NOT changing the layered alpha or anything else, then leave this value
  // alone. OTOH if you are invoking SetLayeredWindowAttributes then you'll
  // most likely want to set this to false, or after changing the alpha toggle
  // the extended style bit to false than back to true. See MSDN for more
  // details.
  bool use_layered_buffer_;

  // The default alpha to be applied to the layered window.
  BYTE layered_alpha_;

  // A canvas that contains the window contents in the case of a layered
  // window.
  scoped_ptr<gfx::Canvas> layered_window_contents_;

  // We must track the invalid rect ourselves, for two reasons:
  // For layered windows, Windows will not do this properly with
  // InvalidateRect()/GetUpdateRect(). (In fact, it'll return misleading
  // information from GetUpdateRect()).
  // We also need to keep track of the invalid rectangle for the RootView should
  // we need to paint the non-client area. The data supplied to WM_NCPAINT seems
  // to be insufficient.
  gfx::Rect invalid_rect_;

  // A factory that allows us to schedule a redraw for layered windows.
  base::WeakPtrFactory<HWNDMessageHandler> paint_layered_window_factory_;

  // True if we are allowed to update the layered window from the DIB backing
  // store if necessary.
  bool can_update_layered_window_;

  DISALLOW_COPY_AND_ASSIGN(HWNDMessageHandler);
};

}  // namespace views

#endif  // UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_H_

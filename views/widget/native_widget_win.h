// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_NATIVE_WIDGET_WIN_H_
#define VIEWS_WIDGET_NATIVE_WIDGET_WIN_H_

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "ui/base/win/window_impl.h"
#include "views/widget/native_widget.h"

namespace gfx {
class CanvasSkia;
}

namespace ui {
class ViewProp;
}

namespace views {
class WidgetImpl;

namespace internal {

// A Windows message reflected from other windows. This message is sent with the
// following arguments:
//    HWND    - Target window
//    MSG     - kReflectedMessage
//    WPARAM  - Should be 0
//    LPARAM  - Pointer to MSG struct containing the original message.
const int kReflectedMessage = WM_APP + 3;

// These two messages aren't defined in winuser.h, but they are sent to windows
// with captions. They appear to paint the window caption and frame.
// Unfortunately if you override the standard non-client rendering as we do
// with CustomFrameWindow, sometimes Windows (not deterministically
// reproducibly but definitely frequently) will send these messages to the
// window and paint the standard caption/title over the top of the custom one.
// So we need to handle these messages in CustomFrameWindow to prevent this
// from happening.
const int WM_NCUAHDRAWCAPTION = 0xAE;
const int WM_NCUAHDRAWFRAME = 0xAF;

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetWin class
//
//  A NativeWidget implementation that wraps a Win32 HWND.
//
class NativeWidgetWin : public NativeWidget,
                        public ui::WindowImpl,
                        public MessageLoopForUI::Observer {
 public:
  explicit NativeWidgetWin(NativeWidgetListener* listener);
  virtual ~NativeWidgetWin();

 private:
  typedef ScopedVector<ui::ViewProp> ViewProps;

  // Overridden from NativeWidget:
  virtual void InitWithNativeViewParent(gfx::NativeView parent,
                                        const gfx::Rect& bounds);
  virtual void SetNativeWindowProperty(const char* name, void* value);
  virtual void* GetNativeWindowProperty(const char* name) const;
  virtual gfx::Rect GetWindowScreenBounds() const;
  virtual gfx::Rect GetClientAreaScreenBounds() const;
  virtual void SetBounds(const gfx::Rect& bounds);
  virtual void SetShape(const gfx::Path& shape);
  virtual gfx::NativeView GetNativeView() const;
  virtual void Show();
  virtual void Hide();
  virtual void Close();
  virtual void MoveAbove(NativeWidget* other);
  virtual void SetAlwaysOnTop(bool always_on_top);
  virtual bool IsVisible() const;
  virtual bool IsActive() const;
  virtual void SetMouseCapture();
  virtual void ReleaseMouseCapture();
  virtual bool HasMouseCapture() const;
  virtual bool ShouldReleaseCaptureOnMouseReleased() const;
  virtual void Invalidate();
  virtual void InvalidateRect(const gfx::Rect& invalid_rect);
  virtual void Paint();
  virtual void FocusNativeView(gfx::NativeView native_view);
  virtual void RunShellDrag(const ui::OSExchangeData& data, int operation);
  virtual WidgetImpl* GetWidgetImpl();
  virtual const WidgetImpl* GetWidgetImpl() const;

  // Overridden from MessageLoop::Observer:
  void WillProcessMessage(const MSG& msg);
  virtual void DidProcessMessage(const MSG& msg);

  // Message handlers
  BEGIN_MSG_MAP_EX(NativeWidgetWin)
    // Range handlers must go first!
    MESSAGE_RANGE_HANDLER_EX(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseRange)
    MESSAGE_RANGE_HANDLER_EX(WM_NCMOUSEMOVE, WM_NCMBUTTONDBLCLK, OnNCMouseRange)

    // Reflected message handler
    MESSAGE_HANDLER_EX(kReflectedMessage, OnReflectedMessage)

    // CustomFrameWindow hacks
    MESSAGE_HANDLER_EX(WM_NCUAHDRAWCAPTION, OnNCUAHDrawCaption)
    MESSAGE_HANDLER_EX(WM_NCUAHDRAWFRAME, OnNCUAHDrawFrame)

    // Vista and newer
    MESSAGE_HANDLER_EX(WM_DWMCOMPOSITIONCHANGED, OnDwmCompositionChanged)

    // Non-atlcrack.h handlers
    MESSAGE_HANDLER_EX(WM_GETOBJECT, OnGetObject)
    MESSAGE_HANDLER_EX(WM_NCMOUSELEAVE, OnMouseLeave)
    MESSAGE_HANDLER_EX(WM_MOUSELEAVE, OnMouseLeave)

    // Key events.
    MESSAGE_HANDLER_EX(WM_KEYDOWN, OnKeyDown)
    MESSAGE_HANDLER_EX(WM_KEYUP, OnKeyUp)
    MESSAGE_HANDLER_EX(WM_SYSKEYDOWN, OnKeyDown);
    MESSAGE_HANDLER_EX(WM_SYSKEYUP, OnKeyUp);

    // This list is in _ALPHABETICAL_ order! OR I WILL HURT YOU.
    MSG_WM_ACTIVATE(OnActivate)
    MSG_WM_ACTIVATEAPP(OnActivateApp)
    MSG_WM_APPCOMMAND(OnAppCommand)
    MSG_WM_CANCELMODE(OnCancelMode)
    MSG_WM_CAPTURECHANGED(OnCaptureChanged)
    MSG_WM_CLOSE(OnClose)
    MSG_WM_COMMAND(OnCommand)
    MSG_WM_CREATE(OnCreate)
    MSG_WM_DESTROY(OnDestroy)
    MSG_WM_DISPLAYCHANGE(OnDisplayChange)
    MSG_WM_ERASEBKGND(OnEraseBkgnd)
    MSG_WM_ENDSESSION(OnEndSession)
    MSG_WM_ENTERSIZEMOVE(OnEnterSizeMove)
    MSG_WM_EXITMENULOOP(OnExitMenuLoop)
    MSG_WM_EXITSIZEMOVE(OnExitSizeMove)
    MSG_WM_GETMINMAXINFO(OnGetMinMaxInfo)
    MSG_WM_HSCROLL(OnHScroll)
    MSG_WM_INITMENU(OnInitMenu)
    MSG_WM_INITMENUPOPUP(OnInitMenuPopup)
    MSG_WM_KILLFOCUS(OnKillFocus)
    MSG_WM_MOUSEACTIVATE(OnMouseActivate)
    MSG_WM_MOVE(OnMove)
    MSG_WM_MOVING(OnMoving)
    MSG_WM_NCACTIVATE(OnNCActivate)
    MSG_WM_NCCALCSIZE(OnNCCalcSize)
    MESSAGE_HANDLER_EX(WM_NCHITTEST, OnNCHitTest)
    MSG_WM_NCPAINT(OnNCPaint)
    MSG_WM_NOTIFY(OnNotify)
    MSG_WM_PAINT(OnPaint)
    MSG_WM_POWERBROADCAST(OnPowerBroadcast)
    MSG_WM_SETFOCUS(OnSetFocus)
    MSG_WM_SETICON(OnSetIcon)
    MSG_WM_SETTEXT(OnSetText)
    MSG_WM_SETTINGCHANGE(OnSettingChange)
    MSG_WM_SIZE(OnSize)
    MSG_WM_SYSCOMMAND(OnSysCommand)
    MSG_WM_THEMECHANGED(OnThemeChanged)
    MSG_WM_VSCROLL(OnVScroll)
    MSG_WM_WINDOWPOSCHANGING(OnWindowPosChanging)
    MSG_WM_WINDOWPOSCHANGED(OnWindowPosChanged)
  END_MSG_MAP()

  virtual void OnActivate(UINT action, BOOL minimized, HWND window);
  virtual void OnActivateApp(BOOL active, DWORD thread_id);
  virtual LRESULT OnAppCommand(HWND window, short app_command, WORD device,
                               int keystate);
  virtual void OnCancelMode();
  virtual void OnCaptureChanged(HWND hwnd);
  virtual void OnClose();
  virtual void OnCommand(UINT notification_code, int command_id, HWND window);
  virtual LRESULT OnCreate(CREATESTRUCT* create_struct);
  // WARNING: If you override this be sure and invoke super, otherwise we'll
  // leak a few things.
  virtual void OnDestroy();
  virtual void OnDisplayChange(UINT bits_per_pixel, CSize screen_size);
  virtual LRESULT OnDwmCompositionChanged(UINT message,
                                          WPARAM w_param,
                                          LPARAM l_param);
  virtual void OnEndSession(BOOL ending, UINT logoff);
  virtual void OnEnterSizeMove();
  virtual LRESULT OnEraseBkgnd(HDC dc);
  virtual void OnExitMenuLoop(BOOL is_track_popup_menu);
  virtual void OnExitSizeMove();
  virtual LRESULT OnGetObject(UINT message, WPARAM w_param, LPARAM l_param);
  virtual void OnGetMinMaxInfo(MINMAXINFO* minmax_info);
  virtual void OnHScroll(int scroll_type, short position, HWND scrollbar);
  virtual void OnInitMenu(HMENU menu);
  virtual void OnInitMenuPopup(HMENU menu, UINT position, BOOL is_system_menu);
  virtual LRESULT OnKeyDown(UINT message, WPARAM w_param, LPARAM l_param);
  virtual LRESULT OnKeyUp(UINT message, WPARAM w_param, LPARAM l_param);
  virtual void OnKillFocus(HWND focused_window);
  virtual LRESULT OnMouseActivate(HWND window, UINT hittest_code, UINT message);
  virtual LRESULT OnMouseLeave(UINT message, WPARAM w_param, LPARAM l_param);
  virtual void OnMove(const CPoint& point);
  virtual void OnMoving(UINT param, LPRECT new_bounds);
  virtual LRESULT OnMouseRange(UINT message, WPARAM w_param, LPARAM l_param);
  virtual LRESULT OnNCActivate(BOOL active);
  virtual LRESULT OnNCCalcSize(BOOL w_param, LPARAM l_param);
  virtual LRESULT OnNCHitTest(UINT message, WPARAM w_param, LPARAM l_param);
  virtual LRESULT OnNCMouseRange(UINT message, WPARAM w_param, LPARAM l_param);
  virtual void OnNCPaint(HRGN rgn);
  virtual LRESULT OnNCUAHDrawCaption(UINT message,
                                     WPARAM w_param,
                                     LPARAM l_param);
  virtual LRESULT OnNCUAHDrawFrame(UINT message, WPARAM w_param,
                                   LPARAM l_param);
  virtual LRESULT OnNotify(int w_param, NMHDR* l_param);
  virtual void OnPaint(HDC dc);
  virtual LRESULT OnPowerBroadcast(DWORD power_event, DWORD data);
  virtual LRESULT OnReflectedMessage(UINT message, WPARAM w_param,
                                     LPARAM l_param);
  virtual void OnSetFocus(HWND focused_window);
  virtual LRESULT OnSetIcon(UINT size_type, HICON new_icon);
  virtual LRESULT OnSetText(const wchar_t* text);
  virtual void OnSettingChange(UINT flags, const wchar_t* section);
  virtual void OnSize(UINT param, const CSize& size);
  virtual void OnSysCommand(UINT notification_code, CPoint click);
  virtual void OnThemeChanged();
  virtual void OnVScroll(int scroll_type, short position, HWND scrollbar);
  virtual void OnWindowPosChanging(WINDOWPOS* window_pos);
  virtual void OnWindowPosChanged(WINDOWPOS* window_pos);

  // Deletes this window as it is destroyed, override to provide different
  // behavior.
  virtual void OnFinalMessage(HWND window);

  // Overridden from WindowImpl:
  virtual HICON GetDefaultWindowIcon() const;
  virtual LRESULT OnWndProc(UINT message, WPARAM w_param, LPARAM l_param);

  // Start tracking all mouse events so that this window gets sent mouse leave
  // messages too.
  void TrackMouseEvents(DWORD mouse_tracking_flags);

  bool ProcessMouseRange(UINT message, WPARAM w_param, LPARAM l_param,
                         bool non_client);
  void ProcessMouseMoved(const CPoint& point, UINT flags, bool is_nonclient);
  void ProcessMouseExited();

  // Fills out a MSG struct with the supplied values.
  void MakeMSG(MSG* msg, UINT message, WPARAM w_param, LPARAM l_param) const;

  void CloseNow();

  bool IsLayeredWindow() const;

  // A listener implementation that handles events received here.
  NativeWidgetListener* listener_;

  // The flags currently being used with TrackMouseEvent to track mouse
  // messages. 0 if there is no active tracking. The value of this member is
  // used when tracking is canceled.
  DWORD active_mouse_tracking_flags_;

  // True when the HWND has event capture.
  bool has_capture_;

  // A canvas that contains the window contents in the case of a layered
  // window.
  scoped_ptr<gfx::CanvasSkia> window_contents_;

  // Properties associated with this NativeWidget implementation.
  // TODO(beng): move to WidgetImpl.
  ViewProps props_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetWin);
};

}  // namespace internal
}  // namespace views

#endif  // VIEWS_WIDGET_NATIVE_WIDGET_WIN_H_


// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_WINDOW_WIN_H_
#define VIEWS_WINDOW_WINDOW_WIN_H_
#pragma once

#include "views/widget/widget_win.h"
#include "views/window/native_window.h"
#include "views/window/window.h"

namespace gfx {
class Font;
class Point;
class Size;
};

namespace views {
namespace internal {
class NativeWindowDelegate;

// This is exposed only for testing
// Adjusts the value of |child_rect| if necessary to ensure that it is
// completely visible within |parent_rect|.
void EnsureRectIsVisibleInRect(const gfx::Rect& parent_rect,
                               gfx::Rect* child_rect,
                               int padding);

}  // namespace internal

class Client;
class WindowDelegate;

///////////////////////////////////////////////////////////////////////////////
//
// WindowWin
//
//  A WindowWin is a WidgetWin that has a caption and a border. The frame is
//  rendered by the operating system.
//
///////////////////////////////////////////////////////////////////////////////
class WindowWin : public WidgetWin,
                  public NativeWindow,
                  public Window {
 public:
  virtual ~WindowWin();

  // Show the window with the specified show command.
  void Show(int show_state);

  // Accessors and setters for various properties.
  void set_focus_on_creation(bool focus_on_creation) {
    focus_on_creation_ = focus_on_creation;
  }

  // Overridden from Window:
  virtual void SetWindowBounds(const gfx::Rect& bounds,
                               gfx::NativeWindow other_window) OVERRIDE;
  virtual void HideWindow() OVERRIDE;
  virtual void SetNativeWindowProperty(const char* name, void* value) OVERRIDE;
  virtual void* GetNativeWindowProperty(const char* name) OVERRIDE;
  virtual void PushForceHidden() OVERRIDE;
  virtual void PopForceHidden() OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual void Maximize() OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual void Restore() OVERRIDE;
  virtual bool IsActive() const OVERRIDE;
  virtual bool IsVisible() const OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual void SetFullscreen(bool fullscreen) OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual void SetUseDragFrame(bool use_drag_frame) OVERRIDE;
  virtual void SetIsAlwaysOnTop(bool always_on_top) OVERRIDE;
  virtual NonClientFrameView* CreateFrameViewForWindow() OVERRIDE;
  virtual void UpdateFrameAfterFrameChange() OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE;
  virtual bool ShouldUseNativeFrame() const OVERRIDE;
  virtual void FrameTypeChanged() OVERRIDE;

  // Returns the system set window title font.
  static gfx::Font GetWindowTitleFont();

 protected:
  friend Window;

  // Constructs the WindowWin. |window_delegate| cannot be NULL.
  explicit WindowWin(WindowDelegate* window_delegate);

  // Create the Window.
  // If parent is NULL, this WindowWin is top level on the desktop.
  // If |bounds| is empty, the view is queried for its preferred size and
  // centered on screen.
  virtual void Init(gfx::NativeView parent, const gfx::Rect& bounds) OVERRIDE;

  // Returns the insets of the client area relative to the non-client area of
  // the window. Override this function instead of OnNCCalcSize, which is
  // crazily complicated.
  virtual gfx::Insets GetClientAreaInsets() const;

  // Retrieve the show state of the window. This is one of the SW_SHOW* flags
  // passed into Windows' ShowWindow method. For normal windows this defaults
  // to SW_SHOWNORMAL, however windows (e.g. the main window) can override this
  // method to provide different values (e.g. retrieve the user's specified
  // show state from the shortcut starutp info).
  virtual int GetShowState() const;

  // Overridden from WidgetWin:
  virtual void OnActivate(UINT action, BOOL minimized, HWND window) OVERRIDE;
  virtual void OnActivateApp(BOOL active, DWORD thread_id) OVERRIDE;
  virtual LRESULT OnAppCommand(HWND window,
                               short app_command,
                               WORD device,
                               int keystate) OVERRIDE;
  virtual void OnCommand(UINT notification_code,
                         int command_id,
                         HWND window) OVERRIDE;
  virtual void OnDestroy() OVERRIDE;
  virtual LRESULT OnDwmCompositionChanged(UINT msg,
                                          WPARAM w_param,
                                          LPARAM l_param) OVERRIDE;
  virtual void OnFinalMessage(HWND window) OVERRIDE;
  virtual void OnGetMinMaxInfo(MINMAXINFO* minmax_info) OVERRIDE;
  virtual void OnInitMenu(HMENU menu) OVERRIDE;
  virtual LRESULT OnMouseLeave(UINT message,
                               WPARAM w_param,
                               LPARAM l_param) OVERRIDE;
  virtual LRESULT OnMouseRange(UINT message,
                               WPARAM w_param,
                               LPARAM l_param) OVERRIDE;
  virtual LRESULT OnNCActivate(BOOL active) OVERRIDE;
  LRESULT OnNCCalcSize(BOOL mode, LPARAM l_param);  // Don't override.
  virtual LRESULT OnNCHitTest(const CPoint& point) OVERRIDE;
  virtual void OnNCPaint(HRGN rgn) OVERRIDE;
  virtual LRESULT OnNCMouseRange(UINT message,
                                 WPARAM w_param,
                                 LPARAM l_param) OVERRIDE;
  virtual LRESULT OnNCUAHDrawCaption(UINT msg,
                                     WPARAM w_param,
                                     LPARAM l_param) OVERRIDE;
  virtual LRESULT OnNCUAHDrawFrame(UINT msg,
                                   WPARAM w_param,
                                   LPARAM l_param) OVERRIDE;
  virtual LRESULT OnSetIcon(UINT size_type, HICON new_icon) OVERRIDE;
  virtual LRESULT OnSetText(const wchar_t* text) OVERRIDE;
  virtual void OnSettingChange(UINT flags, const wchar_t* section) OVERRIDE;
  virtual void OnSize(UINT size_param, const CSize& new_size) OVERRIDE;
  virtual void OnSysCommand(UINT notification_code, CPoint click) OVERRIDE;
  virtual void OnWindowPosChanging(WINDOWPOS* window_pos) OVERRIDE;
  virtual Window* GetWindow() OVERRIDE { return this; }
  virtual const Window* GetWindow() const OVERRIDE { return this; }

  // Overridden from NativeWindow:
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual void ShowNativeWindow(ShowState state) OVERRIDE;
  virtual void BecomeModal() OVERRIDE;
  virtual void CenterWindow(const gfx::Size& size) OVERRIDE;
  virtual void GetWindowBoundsAndMaximizedState(gfx::Rect* bounds,
                                                bool* maximized) const OVERRIDE;
  virtual void EnableClose(bool enable) OVERRIDE;
  virtual void SetWindowTitle(const std::wstring& title) OVERRIDE;
  virtual void SetWindowIcons(const SkBitmap& window_icon,
                              const SkBitmap& app_icon) OVERRIDE;
  virtual void SetAccessibleName(const std::wstring& name) OVERRIDE;
  virtual void SetAccessibleRole(AccessibilityTypes::Role role) OVERRIDE;
  virtual void SetAccessibleState(AccessibilityTypes::State state) OVERRIDE;
  virtual NativeWidget* AsNativeWidget() OVERRIDE;
  virtual const NativeWidget* AsNativeWidget() const OVERRIDE;

 private:
  // Information saved before going into fullscreen mode, used to restore the
  // window afterwards.
  struct SavedWindowInfo {
    bool maximized;
    LONG style;
    LONG ex_style;
    RECT window_rect;
  };

  // Sets-up the focus manager with the view that should have focus when the
  // window is shown the first time.  If NULL is returned, the focus goes to the
  // button if there is one, otherwise the to the Cancel button.
  void SetInitialFocus();

  // If necessary, enables all ancestors.
  void RestoreEnabledIfNecessary();

  // Calculate the appropriate window styles for this window.
  DWORD CalculateWindowStyle();
  DWORD CalculateWindowExStyle();

  // Lock or unlock the window from being able to redraw itself in response to
  // updates to its invalid region.
  class ScopedRedrawLock;
  void LockUpdates();
  void UnlockUpdates();

  // Stops ignoring SetWindowPos() requests (see below).
  void StopIgnoringPosChanges() { ignore_window_pos_changes_ = false; }

  // Resets the window region for the current window bounds if necessary.
  // If |force| is true, the window region is reset to NULL even for native
  // frame windows.
  void ResetWindowRegion(bool force);

  //  Update accessibility information via our WindowDelegate.
  void UpdateAccessibleName(std::wstring& accessible_name);
  void UpdateAccessibleRole();
  void UpdateAccessibleState();

  // Calls the default WM_NCACTIVATE handler with the specified activation
  // value, safely wrapping the call in a ScopedRedrawLock to prevent frame
  // flicker.
  LRESULT CallDefaultNCActivateHandler(BOOL active);

  // Executes the specified SC_command.
  void ExecuteSystemMenuCommand(int command);

  // Static resource initialization.
  static void InitClass();
  enum ResizeCursor {
    RC_NORMAL = 0, RC_VERTICAL, RC_HORIZONTAL, RC_NESW, RC_NWSE
  };
  static HCURSOR resize_cursors_[6];

  // A delegate implementation that handles events received here.
  internal::NativeWindowDelegate* delegate_;

  // Whether we should SetFocus() on a newly created window after
  // Init(). Defaults to true.
  bool focus_on_creation_;

  // Whether all ancestors have been enabled. This is only used if is_modal_ is
  // true.
  bool restored_enabled_;

  // True if we're in fullscreen mode.
  bool fullscreen_;

  // Saved window information from before entering fullscreen mode.
  SavedWindowInfo saved_window_info_;

  // True if this window is the active top level window.
  bool is_active_;

  // True if updates to this window are currently locked.
  bool lock_updates_;

  // The window styles of the window before updates were locked.
  DWORD saved_window_style_;

  // When true, this flag makes us discard incoming SetWindowPos() requests that
  // only change our position/size.  (We still allow changes to Z-order,
  // activation, etc.)
  bool ignore_window_pos_changes_;

  // The following factory is used to ignore SetWindowPos() calls for short time
  // periods.
  ScopedRunnableMethodFactory<WindowWin> ignore_pos_changes_factory_;

  // If this is greater than zero, we should prevent attempts to make the window
  // visible when we handle WM_WINDOWPOSCHANGING. Some calls like
  // ShowWindow(SW_RESTORE) make the window visible in addition to restoring it,
  // when all we want to do is restore it.
  int force_hidden_count_;

  // Set to true when the user presses the right mouse button on the caption
  // area. We need this so we can correctly show the context menu on mouse-up.
  bool is_right_mouse_pressed_on_caption_;

  // The last-seen monitor containing us, and its rect and work area.  These are
  // used to catch updates to the rect and work area and react accordingly.
  HMONITOR last_monitor_;
  gfx::Rect last_monitor_rect_, last_work_area_;

  // The window styles before we modified them for the drag frame appearance.
  DWORD drag_frame_saved_window_style_;
  DWORD drag_frame_saved_window_ex_style_;

  DISALLOW_COPY_AND_ASSIGN(WindowWin);
};

}  // namespace views

#endif  // VIEWS_WINDOW_WINDOW_WIN_H_

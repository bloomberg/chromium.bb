// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_H_
#define UI_VIEWS_WIDGET_WIDGET_H_

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "ui/gfx/point.h"
#include "ui/views/native_types.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/native_widget_listener.h"

namespace gfx {
class Canvas;
class Path;
class Rect;
class Size;
}

namespace ui {
namespace internal {
class RootView;
}
class FocusManager;
class KeyEvent;
class MouseEvent;
class MouseWheelEvent;
class NativeWidget;
class ThemeProvider;
class View;

////////////////////////////////////////////////////////////////////////////////
// Widget class
//
//  Encapsulates the platform-specific rendering, event receiving and widget
//  management aspects of the UI framework.
//
//  Owns a RootView and thus a View hierarchy. Can contain child Widgets.
//  Widget is a platform-independent type that communicates with a platform or
//  context specific NativeWidget implementation.
//
// TODO(beng): Consider ownership of this object vs. NativeWidget.
class Widget : public internal::NativeWidgetListener {
 public:
  explicit Widget(View* contents_view);
  virtual ~Widget();

  void set_delete_on_destroy(bool delete_on_destroy) {
    delete_on_destroy_ = delete_on_destroy;
  }

  // Initialization.
  void InitWithNativeViewParent(gfx::NativeView parent,
                                const gfx::Rect& bounds);
  void InitWithWidgetParent(Widget* parent, const gfx::Rect& bounds);
  void InitWithViewParent(View* parent, const gfx::Rect& bounds);

  // Returns the topmost Widget in a hierarchy.
  Widget* GetTopLevelWidget() const;

  // Returns the bounding rect of the Widget in screen coordinates.
  gfx::Rect GetWindowScreenBounds() const;

  // Returns the bounding rect of the Widget's client area, in screen
  // coordinates.
  gfx::Rect GetClientAreaScreenBounds() const;

  // Sets the bounding rect of the Widget, in the coordinate system of its
  // parent.
  void SetBounds(const gfx::Rect& bounds);

  void SetShape(const gfx::Path& shape);

  void Show();
  void Hide();

  void Close();

  void MoveAbove(Widget* other);
  void SetAlwaysOnTop(bool always_on_top);

  // Causes the specified rectangle to be added to the invalid rectangle for the
  // Widget.
  void InvalidateRect(const gfx::Rect& invalid_rect);

  // Returns a ThemeProvider that can be used to provide resources when
  // rendering Views associated with this Widget.
  ThemeProvider* GetThemeProvider() const;

  // Returns the FocusManager for this Widget. Only top-level Widgets have
  // FocusManagers.
  FocusManager* GetFocusManager() const;

  FocusTraversable* GetFocusTraversable() const;

  NativeWidget* native_widget() const { return native_widget_.get(); }

 private:
  // Overridden from internal::NativeWidgetListener:
  virtual void OnClose();
  virtual void OnDestroy();
  virtual void OnDisplayChanged();
  virtual bool OnKeyEvent(const KeyEvent& event);
  virtual void OnMouseCaptureLost();
  virtual bool OnMouseEvent(const MouseEvent& event);
  virtual bool OnMouseWheelEvent(const MouseWheelEvent& event);
  virtual void OnNativeWidgetCreated();
  virtual void OnPaint(gfx::Canvas* canvas);
  virtual void OnSizeChanged(const gfx::Size& size);
  virtual void OnNativeFocus(gfx::NativeView focused_view);
  virtual void OnNativeBlur(gfx::NativeView focused_view);
  virtual void OnWorkAreaChanged();
  virtual Widget* GetWidget() const;

  // Causes the Widget to be destroyed immediately.
  void CloseNow();

  // A NativeWidget implementation. This can be changed dynamically to a
  // different implementation during the lifetime of the Widget.
  scoped_ptr<NativeWidget> native_widget_;

  // A RootView that owns the View hierarchy within this Widget.
  scoped_ptr<internal::RootView> root_view_;

  // True when any mouse button is pressed.
  bool is_mouse_button_pressed_;

  // The following are used to detect duplicate mouse move events and not
  // deliver them. Displaying a window may result in the system generating
  // duplicate move events even though the mouse hasn't moved.
  bool last_mouse_event_was_move_;
  gfx::Point last_mouse_event_position_;

  // Handles closing the Widget after a return to the message loop to allow the
  // stack to unwind.
  ScopedRunnableMethodFactory<Widget> close_widget_factory_;

  // True if the Widget should be automatically deleted when it is destroyed.
  bool delete_on_destroy_;

  scoped_ptr<FocusManager> focus_manager_;

  DISALLOW_COPY_AND_ASSIGN(Widget);
};

}  // namespace ui

#endif  // UI_VIEWS_WIDGET_WIDGET_H_


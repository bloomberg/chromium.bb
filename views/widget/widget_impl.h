// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_WIDGET_IMPL_H_
#define VIEWS_WIDGET_WIDGET_IMPL_H_

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "ui/gfx/point.h"
#include "views/native_types.h"
#include "views/focus/focus_manager.h"
#include "views/widget/native_widget_listener.h"
#include "views/widget/widget.h"

namespace gfx {
class Canvas;
class Path;
class Rect;
class Size;
}

namespace ui {
  class ThemeProvider;
}

namespace views {
class FocusManager;
class KeyEvent;
class MouseEvent;
class MouseWheelEvent;
class NativeWidget;
class RootView;
class View;

////////////////////////////////////////////////////////////////////////////////
// WidgetImpl class
//
//  Encapsulates the platform-specific rendering, event receiving and widget
//  management aspects of the UI framework.
//
//  Owns a RootView and thus a View hierarchy. Can contain child WidgetImpls.
//  WidgetImpl is a platform-independent type that communicates with a platform
//  or context specific NativeWidget implementation.
//
// TODO(beng): Consider ownership of this object vs. NativeWidget.
class WidgetImpl : public internal::NativeWidgetListener,
                   public Widget {
 public:
  explicit WidgetImpl(View* contents_view);
  virtual ~WidgetImpl();

  void set_delete_on_destroy(bool delete_on_destroy) {
    delete_on_destroy_ = delete_on_destroy;
  }

  // Initialization.
  void InitWithNativeViewParent(gfx::NativeView parent,
                                const gfx::Rect& bounds);

  // Returns the topmost WidgetImpl in a hierarchy.
  WidgetImpl* GetTopLevelWidgetImpl() const;

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

  void MoveAbove(WidgetImpl* other);
  void SetAlwaysOnTop(bool always_on_top);

  // Causes the specified rectangle to be added to the invalid rectangle for the
  // WidgetImpl.
  void InvalidateRect(const gfx::Rect& invalid_rect);

  // Returns a ThemeProvider that can be used to provide resources when
  // rendering Views associated with this WidgetImpl.
  ThemeProvider* GetThemeProvider() const;

  // Returns the FocusManager for this WidgetImpl. Only top-level WidgetImpls
  // have FocusManagers.
  FocusManager* GetFocusManager();

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
  virtual WidgetImpl* GetWidgetImpl();
  virtual const WidgetImpl* GetWidgetImpl() const;

  // Overridden from Widget:
  // TODO(beng): THIS IS TEMPORARY, and excludes methods duplicated above in
  //             new WidgetImpl API.
  // TODO(beng): Remove/Merge with WidgetImpl API above.
  virtual void Init(gfx::NativeView parent, const gfx::Rect& bounds);
  virtual void InitWithWidget(Widget* parent, const gfx::Rect& bounds);
  virtual WidgetDelegate* GetWidgetDelegate();
  virtual void SetWidgetDelegate(WidgetDelegate* delegate);
  virtual void SetContentsView(View* view);
  virtual void GetBounds(gfx::Rect* out, bool including_frame) const;
  virtual void MoveAbove(Widget* widget);
  virtual void SetShape(gfx::NativeRegion region);
  virtual gfx::NativeView GetNativeView() const;
  virtual void PaintNow(const gfx::Rect& update_rect);
  virtual void SetOpacity(unsigned char opacity);
  virtual RootView* GetRootView();
  virtual Widget* GetRootWidget() const;
  virtual bool IsVisible() const;
  virtual bool IsActive() const;
  virtual bool IsAccessibleWidget() const;
  virtual TooltipManager* GetTooltipManager();
  virtual void GenerateMousePressedForView(View* view,
                                           const gfx::Point& point);
  virtual bool GetAccelerator(int cmd_id, ui::Accelerator* accelerator);
  virtual Window* GetWindow();
  virtual const Window* GetWindow() const;
  virtual void SetNativeWindowProperty(const char* name, void* value);
  virtual void* GetNativeWindowProperty(const char* name);
  virtual ThemeProvider* GetDefaultThemeProvider() const;
  virtual void ViewHierarchyChanged(bool is_add, View *parent,
                                    View *child);
  virtual bool ContainsNativeView(gfx::NativeView native_view);
  virtual void StartDragForViewFromMouseEvent(View* view,
                                              const OSExchangeData& data,
                                              int operation);
  virtual View* GetDraggedView();

  // Causes the Widget to be destroyed immediately.
  void CloseNow();

  // A NativeWidget implementation. This can be changed dynamically to a
  // different implementation during the lifetime of the Widget.
  scoped_ptr<NativeWidget> native_widget_;

  // A RootView that owns the View hierarchy within this Widget.
  scoped_ptr<RootView> root_view_;
  // TODO(beng): Remove once we upgrade RootView.
  View* contents_view_;

  // True when any mouse button is pressed.
  bool is_mouse_button_pressed_;

  // The following are used to detect duplicate mouse move events and not
  // deliver them. Displaying a window may result in the system generating
  // duplicate move events even though the mouse hasn't moved.
  bool last_mouse_event_was_move_;
  gfx::Point last_mouse_event_position_;

  // Handles closing the Widget after a return to the message loop to allow the
  // stack to unwind.
  ScopedRunnableMethodFactory<WidgetImpl> close_widget_factory_;

  // True if the Widget should be automatically deleted when it is destroyed.
  bool delete_on_destroy_;

  scoped_ptr<FocusManager> focus_manager_;

  // Valid for the lifetime of StartDragForViewFromMouseEvent, indicates the
  // view the drag started from. NULL at all other times.
  View* dragged_view_;

  DISALLOW_COPY_AND_ASSIGN(WidgetImpl);
};

}  // namespace views

#endif  // VIEWS_WIDGET_WIDGET_IMPL_H_


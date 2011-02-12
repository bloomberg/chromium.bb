// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_WIDGET_IMPL_H_
#define VIEWS_WIDGET_WIDGET_IMPL_H_
#pragma once

#include "views/widget/native_widget_listener.h"
#include "views/widget/widget.h"

namespace views {

class NativeWidget;

////////////////////////////////////////////////////////////////////////////////
// WidgetImpl class
//
//  An object that represents a native widget that hosts a view hierarchy.
//  A WidgetImpl is owned by a NativeWidget implementation.
//
//  TODO(beng): This class should be renamed Widget after the transition to the
//              V2 API is complete.
//
class WidgetImpl : public Widget,
                   internal::NativeWidgetListener {
 public:
  WidgetImpl();
  virtual ~WidgetImpl();

 private:
  // Overridden from Widget:
  virtual void Init(gfx::NativeView parent, const gfx::Rect& bounds);
  virtual void InitWithWidget(Widget* parent, const gfx::Rect& bounds);
  virtual WidgetDelegate* GetWidgetDelegate();
  virtual void SetWidgetDelegate(WidgetDelegate* delegate);
  virtual void SetContentsView(View* view);
  virtual void GetBounds(gfx::Rect* out, bool including_frame) const;
  virtual void SetBounds(const gfx::Rect& bounds);
  virtual void MoveAbove(Widget* other);
  virtual void SetShape(gfx::NativeRegion region);
  virtual void Close();
  virtual void CloseNow();
  virtual void Show();
  virtual void Hide();
  virtual gfx::NativeView GetNativeView() const;
  virtual void PaintNow(const gfx::Rect& update_rect);
  virtual void SetOpacity(unsigned char opacity);
  virtual void SetAlwaysOnTop(bool on_top);
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
  virtual ThemeProvider* GetThemeProvider() const;
  virtual ThemeProvider* GetDefaultThemeProvider() const;
  virtual FocusManager* GetFocusManager();
  virtual void ViewHierarchyChanged(bool is_add, View *parent,
                                    View *child);
  virtual bool ContainsNativeView(gfx::NativeView native_view);

  // Overridden from NativeWidgetListener:
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
  virtual WidgetImpl* GetWidgetImpl() const;

  // Weak.
  NativeWidget* native_widget_;

  DISALLOW_COPY_AND_ASSIGN(WidgetImpl);
};

}  // namespace views

#endif // VIEWS_WIDGET_WIDGET_H_

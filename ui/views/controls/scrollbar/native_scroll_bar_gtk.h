// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SCROLLBAR_NATIVE_SCROLL_BAR_GTK_H_
#define UI_VIEWS_CONTROLS_SCROLLBAR_NATIVE_SCROLL_BAR_GTK_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/views/controls/native_control_gtk.h"
#include "ui/views/controls/scrollbar/native_scroll_bar_wrapper.h"

namespace views {

/////////////////////////////////////////////////////////////////////////////
//
// NativeScrollBarGtk
//
// A View subclass that wraps a Native gtk scrollbar control.
//
// A scrollbar is either horizontal or vertical.
//
/////////////////////////////////////////////////////////////////////////////
class NativeScrollBarGtk : public NativeControlGtk,
                           public NativeScrollBarWrapper {
 public:
  // Creates new scrollbar, either horizontal or vertical.
  explicit NativeScrollBarGtk(NativeScrollBar* native_scroll_bar);
  virtual ~NativeScrollBarGtk();

 private:
  // Overridden from View for layout purpose.
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from View for keyboard UI purpose.
  virtual bool OnKeyPressed(const KeyEvent& event) OVERRIDE;
  virtual bool OnMouseWheel(const MouseWheelEvent& e) OVERRIDE;

  // Overridden from NativeControlGtk.
  virtual void CreateNativeControl() OVERRIDE;

  // Overridden from NativeScrollBarWrapper.
  virtual int GetPosition() const OVERRIDE;
  virtual View* GetView() OVERRIDE;
  virtual void Update(int viewport_size,
                      int content_size,
                      int current_pos) OVERRIDE;

  // Moves the scrollbar by the given value. Negative value is allowed.
  // (moves upward)
  void MoveBy(int o);

  // Moves the scrollbar by the page (viewport) size.
  void MovePage(bool positive);

  // Moves the scrollbar by predefined step size.
  void MoveStep(bool positive);

  // Moves the scrollbar to the given position. MoveTo(0) moves it to the top.
  void MoveTo(int p);

  // Moves the scrollbar to the end.
  void MoveToBottom();

  // Invoked when the scrollbar's position is changed.
  void ValueChanged();
  static void CallValueChanged(GtkWidget* widget,
                               NativeScrollBarGtk* scroll_bar);

  // The NativeScrollBar we are bound to.
  NativeScrollBar* native_scroll_bar_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeScrollBarGtk);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SCROLLBAR_NATIVE_SCROLL_BAR_GTK_H_

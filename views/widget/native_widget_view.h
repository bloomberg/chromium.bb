// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_NATIVE_WIDGET_VIEW_H_
#define VIEWS_WIDGET_NATIVE_WIDGET_VIEW_H_
#pragma once

#include "views/view.h"
#include "views/widget/native_widget_delegate.h"
#include "views/widget/native_widget_views.h"

namespace ui {
enum TouchStatus;
}

namespace views {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetView
//
// This class represents the View that is the "native view" for a
// NativeWidgetViews. It is the View that is a member of the parent Widget's
// View hierarchy. It is responsible for receiving relevant events from that
// hierarchy and forwarding them to its NativeWidgetViews' delegate's hierarchy.
//
class VIEWS_EXPORT NativeWidgetView : public View {
 public:
  static const char kViewClassName[];

  explicit NativeWidgetView(NativeWidgetViews* native_widget);
  virtual ~NativeWidgetView();

  Widget* GetAssociatedWidget();

  void set_delete_native_widget(bool delete_native_widget) {
    delete_native_widget_ = delete_native_widget;
  }

  void set_cursor(gfx::NativeCursor cursor) { cursor_ = cursor; }

  // Overridden from View:
  virtual void CalculateOffsetToAncestorWithLayer(
      gfx::Point* offset,
      ui::Layer** layer_parent) OVERRIDE;

 private:
  // Overridden from View:
  virtual void ViewHierarchyChanged(bool is_add,
                                    View* parent,
                                    View* child) OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::NativeCursor GetCursor(const MouseEvent& event) OVERRIDE;
  virtual bool OnMousePressed(const MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual void OnMouseMoved(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseEntered(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const MouseEvent& event) OVERRIDE;
  virtual ui::TouchStatus OnTouchEvent(const TouchEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const KeyEvent& event) OVERRIDE;
  virtual bool OnKeyReleased(const KeyEvent& event) OVERRIDE;
  virtual bool OnMouseWheel(const MouseWheelEvent& event) OVERRIDE;
  virtual void VisibilityChanged(View* starting_from, bool is_visible) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;
  virtual void MoveLayerToParent(ui::Layer* parent_layer,
                                 const gfx::Point& point) OVERRIDE;
  virtual void DestroyLayerRecurse() OVERRIDE;
  virtual void UpdateLayerBounds(const gfx::Point& offset) OVERRIDE;
  virtual void CreateLayerIfNecessary() OVERRIDE;

  internal::NativeWidgetDelegate* delegate() {
    return native_widget_->delegate();
  }

  NativeWidgetViews* native_widget_;

  // Have we sent OnNativeWidgetCreated?
  bool sent_create_;

  bool delete_native_widget_;

  // The cursor set for the associated widget.
  gfx::NativeCursor cursor_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetView);
};

}  // namespace internal
}  // namespace views

#endif  // VIEWS_WIDGET_NATIVE_WIDGET_VIEW_H_

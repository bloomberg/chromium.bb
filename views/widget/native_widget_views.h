// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_NATIVE_WIDGET_VIEWS_H_
#define VIEWS_WIDGET_NATIVE_WIDGET_VIEWS_H_
#pragma once

#include "base/message_loop.h"
#include "views/widget/native_widget.h"

namespace views {
namespace internal {
class NativeWidgetView;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetViews
//
//  A NativeWidget implementation that uses another View as its native widget.
//
class NativeWidgetViews : public NativeWidget {
 public:
  NativeWidgetViews(View* host, internal::NativeWidgetDelegate* delegate);
  virtual ~NativeWidgetViews();

  // TODO(beng): remove.
  View* GetView();
  const View* GetView() const;

  internal::NativeWidgetDelegate* delegate() { return delegate_; }

 private:
  // Overridden from NativeWidget:
  virtual void InitNativeWidget(const Widget::InitParams& params) OVERRIDE;
  virtual Widget* GetWidget() OVERRIDE;
  virtual const Widget* GetWidget() const OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE;
  virtual Window* GetContainingWindow() OVERRIDE;
  virtual const Window* GetContainingWindow() const OVERRIDE;
  virtual void ViewRemoved(View* view) OVERRIDE;
  virtual void SetNativeWindowProperty(const char* name, void* value) OVERRIDE;
  virtual void* GetNativeWindowProperty(const char* name) OVERRIDE;
  virtual TooltipManager* GetTooltipManager() const OVERRIDE;
  virtual bool IsScreenReaderActive() const OVERRIDE;
  virtual void SendNativeAccessibilityEvent(
      View* view,
      ui::AccessibilityTypes::Event event_type) OVERRIDE;
  virtual void SetMouseCapture() OVERRIDE;
  virtual void ReleaseMouseCapture() OVERRIDE;
  virtual bool HasMouseCapture() const OVERRIDE;
  virtual bool IsMouseButtonDown() const OVERRIDE;
  virtual InputMethod* GetInputMethodNative() OVERRIDE;
  virtual void ReplaceInputMethod(InputMethod* input_method) OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual gfx::Rect GetWindowScreenBounds() const OVERRIDE;
  virtual gfx::Rect GetClientAreaScreenBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void MoveAbove(gfx::NativeView native_view) OVERRIDE;
  virtual void SetShape(gfx::NativeRegion shape) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void CloseNow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void SetOpacity(unsigned char opacity) OVERRIDE;
  virtual void SetAlwaysOnTop(bool on_top) OVERRIDE;
  virtual bool IsVisible() const OVERRIDE;
  virtual bool IsActive() const OVERRIDE;
  virtual bool IsAccessibleWidget() const OVERRIDE;
  virtual bool ContainsNativeView(gfx::NativeView native_view) const OVERRIDE;
  virtual void RunShellDrag(View* view,
                            const ui::OSExchangeData& data,
                            int operation) OVERRIDE;
  virtual void SchedulePaintInRect(const gfx::Rect& rect) OVERRIDE;
  virtual void SetCursor(gfx::NativeCursor cursor) OVERRIDE;

  NativeWidget* GetParentNativeWidget();
  const NativeWidget* GetParentNativeWidget() const;

  internal::NativeWidgetDelegate* delegate_;

  internal::NativeWidgetView* view_;

  View* host_view_;

  // The following factory is used for calls to close the NativeWidgetViews
  // instance.
  ScopedRunnableMethodFactory<NativeWidgetViews> close_widget_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetViews);
};

}

#endif  // VIEWS_WIDGET_NATIVE_WIDGET_VIEWS_H_

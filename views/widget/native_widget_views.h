// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_NATIVE_WIDGET_VIEWS_H_
#define VIEWS_WIDGET_NATIVE_WIDGET_VIEWS_H_
#pragma once

#include "base/message_loop.h"
#include "views/widget/native_widget_private.h"

namespace views {
namespace internal {
class NativeWidgetView;
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetViews
//
//  A NativeWidget implementation that uses another View as its native widget.
//
class NativeWidgetViews : public internal::NativeWidgetPrivate {
 public:
  NativeWidgetViews(View* host, internal::NativeWidgetDelegate* delegate);
  virtual ~NativeWidgetViews();

  // TODO(beng): remove.
  View* GetView();
  const View* GetView() const;

  void OnActivate(bool active);

  internal::NativeWidgetDelegate* delegate() { return delegate_; }

 protected:
  // Overridden from internal::NativeWidgetPrivate:
  virtual void InitNativeWidget(const Widget::InitParams& params) OVERRIDE;
  virtual NonClientFrameView* CreateNonClientFrameView() OVERRIDE;
  virtual void UpdateFrameAfterFrameChange() OVERRIDE;
  virtual bool ShouldUseNativeFrame() const OVERRIDE;
  virtual void FrameTypeChanged() OVERRIDE;
  virtual Widget* GetWidget() OVERRIDE;
  virtual const Widget* GetWidget() const OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE;
  virtual void ViewRemoved(View* view) OVERRIDE;
  virtual void SetNativeWindowProperty(const char* name, void* value) OVERRIDE;
  virtual void* GetNativeWindowProperty(const char* name) const OVERRIDE;
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
  virtual void CenterWindow(const gfx::Size& size) OVERRIDE;
  virtual void GetWindowBoundsAndMaximizedState(gfx::Rect* bounds,
                                                bool* maximized) const OVERRIDE;
  virtual void SetWindowTitle(const std::wstring& title) OVERRIDE;
  virtual void SetWindowIcons(const SkBitmap& window_icon,
                              const SkBitmap& app_icon) OVERRIDE;
  virtual void SetAccessibleName(const std::wstring& name) OVERRIDE;
  virtual void SetAccessibleRole(ui::AccessibilityTypes::Role role) OVERRIDE;
  virtual void SetAccessibleState(ui::AccessibilityTypes::State state) OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual void BecomeModal() OVERRIDE;
  virtual gfx::Rect GetWindowScreenBounds() const OVERRIDE;
  virtual gfx::Rect GetClientAreaScreenBounds() const OVERRIDE;
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void SetBoundsConstrained(const gfx::Rect& bounds,
                                    Widget* other_widget) OVERRIDE;
  virtual void MoveAbove(gfx::NativeView native_view) OVERRIDE;
  virtual void SetShape(gfx::NativeRegion shape) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void CloseNow() OVERRIDE;
  virtual void EnableClose(bool enable) OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void ShowNativeWidget(ShowState state) OVERRIDE;
  virtual bool IsVisible() const OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual bool IsActive() const OVERRIDE;
  virtual void SetAlwaysOnTop(bool always_on_top) OVERRIDE;
  virtual void Maximize() OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual void Restore() OVERRIDE;
  virtual void SetFullscreen(bool fullscreen) OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual void SetOpacity(unsigned char opacity) OVERRIDE;
  virtual void SetUseDragFrame(bool use_drag_frame) OVERRIDE;
  virtual bool IsAccessibleWidget() const OVERRIDE;
  virtual bool ContainsNativeView(gfx::NativeView native_view) const OVERRIDE;
  virtual void RunShellDrag(View* view,
                            const ui::OSExchangeData& data,
                            int operation) OVERRIDE;
  virtual void SchedulePaintInRect(const gfx::Rect& rect) OVERRIDE;
  virtual void SetCursor(gfx::NativeCursor cursor) OVERRIDE;

 private:
  internal::NativeWidgetPrivate* GetParentNativeWidget();
  const internal::NativeWidgetPrivate* GetParentNativeWidget() const;

  internal::NativeWidgetDelegate* delegate_;

  internal::NativeWidgetView* view_;

  View* host_view_;

  bool active_;

  // The following factory is used for calls to close the NativeWidgetViews
  // instance.
  ScopedRunnableMethodFactory<NativeWidgetViews> close_widget_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetViews);
};

}

#endif  // VIEWS_WIDGET_NATIVE_WIDGET_VIEWS_H_

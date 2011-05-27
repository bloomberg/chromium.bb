// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_NATIVE_WINDOW_VIEWS_H_
#define VIEWS_WINDOW_NATIVE_WINDOW_VIEWS_H_
#pragma once

#include "base/message_loop.h"
#include "views/window/native_window.h"
#include "views/widget/native_widget_views.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeWindowViews
//
//  A NativeWindow implementation that uses another View as its native widget.
//
class NativeWindowViews : public NativeWidgetViews,
                          public NativeWindow {
 public:
  NativeWindowViews(View* host, internal::NativeWindowDelegate* delegate);
  virtual ~NativeWindowViews();

 private:
  // Overridden from NativeWindow:
  virtual Window* GetWindow() OVERRIDE;
  virtual const Window* GetWindow() const OVERRIDE;
  virtual NativeWidget* AsNativeWidget() OVERRIDE;
  virtual const NativeWidget* AsNativeWidget() const OVERRIDE;
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
  virtual void SetAccessibleRole(ui::AccessibilityTypes::Role role) OVERRIDE;
  virtual void SetAccessibleState(ui::AccessibilityTypes::State state) OVERRIDE;
  virtual void SetWindowBounds(const gfx::Rect& bounds,
                               gfx::NativeWindow other_window) OVERRIDE;
  virtual void HideWindow() OVERRIDE;
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
  virtual void SetAlwaysOnTop(bool always_on_top) OVERRIDE;
  virtual void SetUseDragFrame(bool use_drag_frame) OVERRIDE;
  virtual NonClientFrameView* CreateFrameViewForWindow() OVERRIDE;
  virtual void UpdateFrameAfterFrameChange() OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE;
  virtual bool ShouldUseNativeFrame() const OVERRIDE;
  virtual void FrameTypeChanged() OVERRIDE;

  internal::NativeWindowDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(NativeWindowViews);
};

}

#endif  // VIEWS_WINDOW_NATIVE_WINDOW_VIEWS_H_

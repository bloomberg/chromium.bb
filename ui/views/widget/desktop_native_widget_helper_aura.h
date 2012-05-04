// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_NATIVE_WIDGET_HELPER_AURA_H_
#define UI_VIEWS_WIDGET_DESKTOP_NATIVE_WIDGET_HELPER_AURA_H_
#pragma once

#include "ui/aura/root_window_observer.h"
#include "ui/gfx/rect.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/native_widget_helper_aura.h"
#include "ui/views/widget/widget.h"

namespace aura {
class RootWindow;
}

namespace ui {
#if defined(OS_WIN)
class HWNDSubclass;
#endif
}

namespace views {
class NativeWidgetAura;
class WidgetMessageFilter;

// Implementation of non-Ash desktop integration code, allowing
// NativeWidgetAuras to work in a traditional desktop environment.
class VIEWS_EXPORT DesktopNativeWidgetHelperAura
    : public NativeWidgetHelperAura,
      public aura::RootWindowObserver {
 public:
  explicit DesktopNativeWidgetHelperAura(NativeWidgetAura* widget);
  virtual ~DesktopNativeWidgetHelperAura();

  // Overridden from aura::NativeWidgetHelperAura:
  virtual void PreInitialize(const Widget::InitParams& params) OVERRIDE;
  virtual void PostInitialize() OVERRIDE;
  virtual void ShowRootWindow() OVERRIDE;
  virtual aura::RootWindow* GetRootWindow() OVERRIDE;
  virtual gfx::Rect ModifyAndSetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual gfx::Rect ChangeRootWindowBoundsToScreenBounds(
      const gfx::Rect& bounds) OVERRIDE;

  // Overridden from aura::RootWindowObserver:
  virtual void OnRootWindowResized(const aura::RootWindow* root,
                                   const gfx::Size& old_size) OVERRIDE;
  virtual void OnRootWindowHostClosed(const aura::RootWindow* root) OVERRIDE;

 private:
  // A weak pointer back to our owning widget.
  NativeWidgetAura* widget_;

  // Optionally, a RootWindow that we attach ourselves to.
  scoped_ptr<aura::RootWindow> root_window_;

  // We want some windows (omnibox, status bar) to have their own
  // NativeWidgetAura, but still act as if they're screen bounded toplevel
  // windows.
  bool is_embedded_window_;

#if defined(OS_WIN)
  scoped_ptr<ui::HWNDSubclass> subclass_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DesktopNativeWidgetHelperAura);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_NATIVE_WIDGET_HELPER_AURA_H_

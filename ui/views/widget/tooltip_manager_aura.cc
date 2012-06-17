// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "ui/aura/client/tooltip_client.h"
#include "ui/aura/root_window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/tooltip_manager_aura.h"

namespace views {

// static
int TooltipManager::GetTooltipHeight() {
  // Not used for linux and chromeos.
  NOTIMPLEMENTED();
  return 0;
}

// static
gfx::Font TooltipManager::GetDefaultFont() {
  return ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::BaseFont);
}

// static
int TooltipManager::GetMaxWidth(int x, int y) {
  gfx::Rect monitor_bounds =
      gfx::Screen::GetDisplayNearestPoint(gfx::Point(x, y)).bounds();
  return (monitor_bounds.width() + 1) / 2;
}

////////////////////////////////////////////////////////////////////////////////
// TooltipManagerAura public:

TooltipManagerAura::TooltipManagerAura(NativeWidgetAura* native_widget_aura)
    : native_widget_aura_(native_widget_aura) {
  aura::client::SetTooltipText(native_widget_aura_->GetNativeView(),
                               &tooltip_text_);
}

TooltipManagerAura::~TooltipManagerAura() {
  aura::client::SetTooltipText(native_widget_aura_->GetNativeView(), NULL);
}

////////////////////////////////////////////////////////////////////////////////
// TooltipManagerAura, TooltipManager implementation:

void TooltipManagerAura::UpdateTooltip() {
  aura::Window* window = native_widget_aura_->GetNativeView();
  aura::RootWindow* root_window = window->GetRootWindow();
  if (aura::client::GetTooltipClient(root_window)) {
    gfx::Point view_point = root_window->last_mouse_location();
    aura::Window::ConvertPointToWindow(root_window, window, &view_point);
    View* view = GetViewUnderPoint(view_point);
    if (view) {
      View::ConvertPointFromWidget(view, &view_point);
      if (!view->GetTooltipText(view_point, &tooltip_text_))
        tooltip_text_.clear();
    } else {
      tooltip_text_.clear();
    }
    aura::client::GetTooltipClient(root_window)->UpdateTooltip(window);
  }
}

void TooltipManagerAura::TooltipTextChanged(View* view)  {
  aura::Window* window = native_widget_aura_->GetNativeView();
  aura::RootWindow* root_window = window->GetRootWindow();
  if (aura::client::GetTooltipClient(root_window)) {
    gfx::Point view_point = root_window->last_mouse_location();
    aura::Window::ConvertPointToWindow(root_window, window, &view_point);
    View* target = GetViewUnderPoint(view_point);
    if (target != view)
      return;
    if (target) {
      View::ConvertPointFromWidget(view, &view_point);
      if (!view->GetTooltipText(view_point, &tooltip_text_))
        tooltip_text_.clear();
    } else {
      tooltip_text_.clear();
    }
    aura::client::GetTooltipClient(root_window)->UpdateTooltip(window);
  }
}

void TooltipManagerAura::ShowKeyboardTooltip(View* view) {
  NOTREACHED();
}

void TooltipManagerAura::HideKeyboardTooltip()  {
  NOTREACHED();
}

View* TooltipManagerAura::GetViewUnderPoint(const gfx::Point& point) {
  View* root_view = native_widget_aura_->GetWidget()->GetRootView();
  return root_view->GetEventHandlerForPoint(point);
}

}  // namespace views.

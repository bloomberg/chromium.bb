// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_keyboard_bounds_observer.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "chrome/browser/apps/platform_apps/app_window_registry_util.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/render_widget_host_view.h"
#include "extensions/browser/app_window/app_window.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/view.h"
#include "ui/wm/core/coordinate_conversion.h"

ChromeKeyboardBoundsObserver::ChromeKeyboardBoundsObserver(
    aura::Window* keyboard_window)
    : keyboard_window_(keyboard_window) {
  DCHECK(keyboard_window_);
  ChromeKeyboardControllerClient::Get()->AddObserver(this);
}

ChromeKeyboardBoundsObserver::~ChromeKeyboardBoundsObserver() {
  UpdateOccludedBounds(gfx::Rect());

  RemoveAllObservedWindows();

  ChromeKeyboardControllerClient::Get()->RemoveObserver(this);
}

void ChromeKeyboardBoundsObserver::OnKeyboardOccludedBoundsChanged(
    const gfx::Rect& screen_bounds) {
  gfx::Rect new_bounds(screen_bounds);
  wm::ConvertRectFromScreen(keyboard_window_, &new_bounds);
  UpdateOccludedBounds(
      ChromeKeyboardControllerClient::Get()->IsKeyboardOverscrollEnabled()
          ? new_bounds
          : gfx::Rect());
}

void ChromeKeyboardBoundsObserver::UpdateOccludedBounds(
    const gfx::Rect& occluded_bounds) {
  DVLOG(1) << "UpdateOccludedBounds: " << occluded_bounds.ToString();
  occluded_bounds_ = occluded_bounds;

  // Adjust the height of the viewport for visible windows on the primary
  // display. TODO(kevers): Add EnvObserver to properly initialize insets if a
  // window is created while the keyboard is visible.
  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    content::RenderWidgetHostView* view = widget->GetView();
    // Can be null, e.g. if the RenderWidget is being destroyed or
    // the render process crashed.
    if (!view)
      continue;

    if (occluded_bounds.IsEmpty()) {
      view->SetInsets(gfx::Insets());
      continue;
    }

    aura::Window* window = view->GetNativeView();
    // Added while we determine if RenderWidgetHostViewChildFrame can be
    // changed to always return a non-null value: https://crbug.com/644726.
    // If we cannot guarantee a non-null value, then this may need to stay.
    if (!window)
      continue;

    if (!ShouldWindowOverscroll(window))
      continue;

    UpdateInsetsForHostView(view);
    AddObservedWindow(window);
  }

  if (occluded_bounds.IsEmpty())
    RemoveAllObservedWindows();
}

void ChromeKeyboardBoundsObserver::AddObservedWindow(aura::Window* window) {
  // Only observe top level windows.
  window = window->GetToplevelWindow();
  if (!window->HasObserver(this)) {
    window->AddObserver(this);
    observed_windows_.insert(window);
  }
}

void ChromeKeyboardBoundsObserver::RemoveAllObservedWindows() {
  for (aura::Window* window : observed_windows_)
    window->RemoveObserver(this);
  observed_windows_.clear();
}

void ChromeKeyboardBoundsObserver::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  DVLOG(1) << "OnWindowBoundsChanged: " << new_bounds.ToString();
  UpdateInsetsForWindow(window);
}

void ChromeKeyboardBoundsObserver::OnWindowDestroyed(aura::Window* window) {
  if (window->HasObserver(this))
    window->RemoveObserver(this);
  observed_windows_.erase(window);
}

void ChromeKeyboardBoundsObserver::UpdateInsetsForWindow(aura::Window* window) {
  if (!ShouldWindowOverscroll(window))
    return;

  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    content::RenderWidgetHostView* view = widget->GetView();
    if (view && window->Contains(view->GetNativeView())) {
      if (ShouldEnableInsets(window))
        UpdateInsetsForHostView(view);
      else
        view->SetInsets(gfx::Insets());
    }
  }
}

void ChromeKeyboardBoundsObserver::UpdateInsetsForHostView(
    content::RenderWidgetHostView* view) {
  gfx::Rect view_bounds = view->GetViewBounds();
  gfx::Rect intersect = gfx::IntersectRects(view_bounds, occluded_bounds_);
  int overlap = intersect.height();
  if (overlap > 0 && overlap < view_bounds.height()) {
    DVLOG(2) << "SetInsets for: " << view << " Overlap: " << overlap;
    view->SetInsets(gfx::Insets(0, 0, overlap, 0));
  } else {
    view->SetInsets(gfx::Insets());
  }
}

bool ChromeKeyboardBoundsObserver::ShouldWindowOverscroll(
    aura::Window* window) {
  // The virtual keyboard should not overscroll.
  if (window->GetToplevelWindow() == keyboard_window_->GetToplevelWindow())
    return false;

  // IME windows should not overscroll.
  extensions::AppWindow* app_window =
      AppWindowRegistryUtil::GetAppWindowForNativeWindowAnyProfile(
          window->GetToplevelWindow());
  if (app_window && app_window->is_ime_window())
    return false;

  return true;
}

bool ChromeKeyboardBoundsObserver::ShouldEnableInsets(aura::Window* window) {
  return keyboard_window_->IsVisible() &&
         keyboard_window_->GetRootWindow() == window->GetRootWindow() &&
         ChromeKeyboardControllerClient::Get()->IsKeyboardOverscrollEnabled();
}

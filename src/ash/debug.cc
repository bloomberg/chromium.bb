// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/debug.h"

#include <memory>
#include <string>

#include "ash/public/cpp/debug_utils.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/debug_utils.h"
#include "ui/views/debug_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace debug {

void PrintLayerHierarchy(std::ostringstream* out) {
  for (aura::Window* root : Shell::Get()->GetAllRootWindows()) {
    ui::Layer* layer = root->layer();
    if (layer) {
      ui::PrintLayerHierarchy(
          layer,
          RootWindowController::ForWindow(root)->GetLastMouseLocationInRoot(),
          out);
    }
  }
}

void PrintViewHierarchy(std::ostringstream* out) {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window)
    return;
  views::Widget* widget = views::Widget::GetWidgetForNativeView(active_window);
  if (!widget)
    return;
  views::PrintViewHierarchy(widget->GetRootView(), out);
}

void PrintWindowHierarchy(const aura::Window* active_window,
                          const aura::Window* focused_window,
                          aura::Window* window,
                          int indent,
                          bool scrub_data,
                          std::ostringstream* out) {
  std::string indent_str(indent, ' ');
  std::string name(window->GetName());
  if (name.empty())
    name = "\"\"";
  const gfx::Vector2dF& subpixel_position_offset =
      window->layer()->GetSubpixelOffset();
  *out << indent_str;
  *out << name << " (" << window << ")"
       << " type=" << window->type();
  int window_id = window->id();
  if (window_id != aura::Window::kInitialId)
    *out << " id=" << window_id;
  if (window->GetProperty(kWindowStateKey))
    *out << " " << WindowState::Get(window)->GetStateType();
  *out << ((window == active_window) ? " [active]" : "")
       << ((window == focused_window) ? " [focused]" : "")
       << (window->transparent() ? " [transparent]" : "")
       << (window->IsVisible() ? " [visible]" : "") << " "
       << (window->occlusion_state() != aura::Window::OcclusionState::UNKNOWN
               ? aura::Window::OcclusionStateToString(window->occlusion_state())
               : "")
       << " " << window->bounds().ToString();
  if (!subpixel_position_offset.IsZero())
    *out << " subpixel offset=" + subpixel_position_offset.ToString();
  std::string* tree_id = window->GetProperty(ui::kChildAXTreeID);
  if (tree_id)
    *out << " ax_tree_id=" << *tree_id;

  if (!scrub_data) {
    base::string16 title(window->GetTitle());
    if (!title.empty())
      *out << " title=" << title;
  }

  int app_type = window->GetProperty(aura::client::kAppType);
  *out << " app_type=" << app_type;
  std::string* pkg_name = window->GetProperty(ash::kArcPackageNameKey);
  if (pkg_name)
    *out << " pkg_name=" << *pkg_name;
  *out << '\n';

  for (aura::Window* child : window->children()) {
    PrintWindowHierarchy(active_window, focused_window, child, indent + 3,
                         scrub_data, out);
  }
}

void PrintWindowHierarchy(std::ostringstream* out, bool scrub_data) {
  aura::Window* active_window = window_util::GetActiveWindow();
  aura::Window* focused_window = window_util::GetFocusedWindow();
  aura::Window::Windows roots = Shell::Get()->GetAllRootWindows();
  for (size_t i = 0; i < roots.size(); ++i) {
    *out << "RootWindow " << i << ":\n";
    PrintWindowHierarchy(active_window, focused_window, roots[i], 0, scrub_data,
                         out);
  }
}

void ToggleShowDebugBorders() {
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  std::unique_ptr<cc::DebugBorderTypes> value;
  for (auto* window : root_windows) {
    ui::Compositor* compositor = window->GetHost()->compositor();
    cc::LayerTreeDebugState state = compositor->GetLayerTreeDebugState();
    if (!value.get())
      value.reset(new cc::DebugBorderTypes(state.show_debug_borders.flip()));
    state.show_debug_borders = *value.get();
    compositor->SetLayerTreeDebugState(state);
  }
}

void ToggleShowFpsCounter() {
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  std::unique_ptr<bool> value;
  for (auto* window : root_windows) {
    ui::Compositor* compositor = window->GetHost()->compositor();
    cc::LayerTreeDebugState state = compositor->GetLayerTreeDebugState();
    if (!value.get())
      value.reset(new bool(!state.show_fps_counter));
    state.show_fps_counter = *value.get();
    compositor->SetLayerTreeDebugState(state);
  }
}

void ToggleShowPaintRects() {
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  std::unique_ptr<bool> value;
  for (auto* window : root_windows) {
    ui::Compositor* compositor = window->GetHost()->compositor();
    cc::LayerTreeDebugState state = compositor->GetLayerTreeDebugState();
    if (!value.get())
      value.reset(new bool(!state.show_paint_rects));
    state.show_paint_rects = *value.get();
    compositor->SetLayerTreeDebugState(state);
  }
}

}  // namespace debug
}  // namespace ash

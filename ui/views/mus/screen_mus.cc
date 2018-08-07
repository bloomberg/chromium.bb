// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/screen_mus.h"

#include "base/stl_util.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/types/display_constants.h"
#include "ui/views/mus/screen_mus_delegate.h"
#include "ui/views/mus/window_manager_frame_values.h"

namespace mojo {

template <>
struct TypeConverter<views::WindowManagerFrameValues,
                     ui::mojom::FrameDecorationValuesPtr> {
  static views::WindowManagerFrameValues Convert(
      const ui::mojom::FrameDecorationValuesPtr& input) {
    views::WindowManagerFrameValues result;
    result.normal_insets = input->normal_client_area_insets;
    result.maximized_insets = input->maximized_client_area_insets;
    result.max_title_bar_button_width = input->max_title_bar_button_width;
    return result;
  }
};

}  // namespace mojo

namespace views {

using Type = display::DisplayList::Type;

ScreenMus::ScreenMus(ScreenMusDelegate* delegate) : delegate_(delegate) {
  DCHECK(delegate);
  display::Screen::SetScreenInstance(this);
}

ScreenMus::~ScreenMus() {
  DCHECK_EQ(this, display::Screen::GetScreen());
  display::Screen::SetScreenInstance(nullptr);
}

void ScreenMus::OnDisplaysChanged(
    std::vector<ui::mojom::WsDisplayPtr> ws_displays,
    int64_t primary_display_id,
    int64_t internal_display_id,
    int64_t display_id_for_new_windows) {
  const bool primary_changed = primary_display_id != GetPrimaryDisplay().id();
  int64_t handled_display_id = display::kInvalidDisplayId;
  const WindowManagerFrameValues initial_frame_values =
      WindowManagerFrameValues::instance();

  if (internal_display_id != display::kInvalidDisplayId)
    display::Display::SetInternalDisplayId(internal_display_id);

  if (primary_changed) {
    handled_display_id = primary_display_id;
    for (auto& ws_display_ptr : ws_displays) {
      if (ws_display_ptr->display.id() == primary_display_id) {
        // TODO(sky): Make WindowManagerFrameValues per display.
        WindowManagerFrameValues frame_values =
            ws_display_ptr->frame_decoration_values
                .To<WindowManagerFrameValues>();
        WindowManagerFrameValues::SetInstance(frame_values);

        const bool is_primary = true;
        ProcessDisplayChanged(ws_display_ptr->display, is_primary);
        break;
      }
    }
  }

  // Add new displays and update existing ones.
  std::set<int64_t> display_ids;
  for (auto& ws_display_ptr : ws_displays) {
    display_ids.insert(ws_display_ptr->display.id());
    if (handled_display_id == ws_display_ptr->display.id())
      continue;

    const bool is_primary = false;
    ProcessDisplayChanged(ws_display_ptr->display, is_primary);
  }

  // Remove any displays no longer in |ws_displays|.
  std::set<int64_t> existing_display_ids;
  for (const auto& display : display_list().displays())
    existing_display_ids.insert(display.id());
  std::set<int64_t> removed_display_ids =
      base::STLSetDifference<std::set<int64_t>>(existing_display_ids,
                                                display_ids);
  for (int64_t display_id : removed_display_ids) {
    // TODO(kylechar): DisplayList would need to change to handle having no
    // primary display.
    if (display_id != GetPrimaryDisplay().id())
      display_list().RemoveDisplay(display_id);
  }

  if (primary_changed &&
      initial_frame_values != WindowManagerFrameValues::instance()) {
    delegate_->OnWindowManagerFrameValuesChanged();
  }

  // GetDisplayForNewWindows() can handle ids that are not in the list.
  display_id_for_new_windows_ = display_id_for_new_windows;
}

display::Display ScreenMus::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  aura::WindowTreeHostMus* window_tree_host_mus =
      aura::WindowTreeHostMus::ForWindow(window);
  if (!window_tree_host_mus)
    return GetPrimaryDisplay();
  return window_tree_host_mus->GetDisplay();
}

gfx::Point ScreenMus::GetCursorScreenPoint() {
  return aura::Env::GetInstance()->last_mouse_location();
}

bool ScreenMus::IsWindowUnderCursor(gfx::NativeWindow window) {
  return window && window->IsVisible() &&
         window->GetBoundsInScreen().Contains(GetCursorScreenPoint());
}

aura::Window* ScreenMus::GetWindowAtScreenPoint(const gfx::Point& point) {
  return delegate_->GetWindowAtScreenPoint(point);
}

display::Display ScreenMus::GetDisplayForNewWindows() const {
  display::Display display;
  if (GetDisplayWithDisplayId(display_id_for_new_windows_, &display))
    return display;

  // Fallback to primary display.
  return GetPrimaryDisplay();
}

}  // namespace views

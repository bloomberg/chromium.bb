// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This has to be before any other includes, else default is picked up.
// See base/logging for details on this.
#define NOTIMPLEMENTED_POLICY 5

#include "ui/views/mus/screen_mus.h"

#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"
#include "ui/aura/window.h"
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

ScreenMus::ScreenMus(ScreenMusDelegate* delegate)
    : delegate_(delegate),
      display_manager_observer_binding_(this) {
}

ScreenMus::~ScreenMus() {}

void ScreenMus::Init(shell::Connector* connector) {
  connector->ConnectToInterface("service:ui", &display_manager_);

  display_manager_->AddObserver(
      display_manager_observer_binding_.CreateInterfacePtrAndBind());

  // We need the set of displays before we can continue. Wait for it.
  //
  // TODO(rockot): Do something better here. This should not have to block tasks
  // from running on the calling thread. http://crbug.com/594852.
  bool success = display_manager_observer_binding_.WaitForIncomingMethodCall();

  // The WaitForIncomingMethodCall() should have supplied the set of Displays,
  // unless mus is going down, in which case encountered_error() is true, or the
  // call to WaitForIncomingMethodCall() failed.
  if (display_list()->displays().empty()) {
    DCHECK(display_manager_.encountered_error() || !success);
    // In this case we install a default display and assume the process is
    // going to exit shortly so that the real value doesn't matter.
    display_list()->AddDisplay(
        display::Display(0xFFFFFFFF, gfx::Rect(0, 0, 801, 802)),
        display::DisplayList::Type::PRIMARY);
  }
}

gfx::Point ScreenMus::GetCursorScreenPoint() {
  if (!delegate_) {
    // TODO(erg): If we need the cursor point in the window manager, we'll need
    // to make |delegate_| required. It only recently changed to be optional.
    NOTIMPLEMENTED();
    return gfx::Point();
  }

  return delegate_->GetCursorScreenPoint();
}

bool ScreenMus::IsWindowUnderCursor(gfx::NativeWindow window) {
  return window && window->IsVisible() &&
         window->GetBoundsInScreen().Contains(GetCursorScreenPoint());
}

void ScreenMus::OnDisplays(mojo::Array<ui::mojom::WsDisplayPtr> ws_displays) {
  // This should only be called once from Init() before any observers have been
  // added.
  DCHECK(display_list()->displays().empty());
  for (size_t i = 0; i < ws_displays.size(); ++i) {
    const bool is_primary = ws_displays[i]->is_primary;
    display_list()->AddDisplay(ws_displays[i]->display,
                               is_primary
                                   ? display::DisplayList::Type::PRIMARY
                                   : display::DisplayList::Type::NOT_PRIMARY);
    if (is_primary) {
      // TODO(sky): Make WindowManagerFrameValues per display.
      WindowManagerFrameValues frame_values =
          ws_displays[i]
              ->frame_decoration_values.To<WindowManagerFrameValues>();
      WindowManagerFrameValues::SetInstance(frame_values);
    }
  }
  DCHECK(!display_list()->displays().empty());
}

void ScreenMus::OnDisplaysChanged(
    mojo::Array<ui::mojom::WsDisplayPtr> ws_displays) {
  for (size_t i = 0; i < ws_displays.size(); ++i) {
    const bool is_primary = ws_displays[i]->is_primary;
    ProcessDisplayChanged(ws_displays[i]->display, is_primary);
    if (is_primary) {
      WindowManagerFrameValues frame_values =
          ws_displays[i]
              ->frame_decoration_values.To<WindowManagerFrameValues>();
      WindowManagerFrameValues::SetInstance(frame_values);
      if (delegate_)
        delegate_->OnWindowManagerFrameValuesChanged();
    }
  }
}

void ScreenMus::OnDisplayRemoved(int64_t id) {
  display_list()->RemoveDisplay(id);
}

}  // namespace views

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/property_utils.h"

#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/wm/public/window_types.h"

namespace aura {
namespace {

ui::wm::WindowType UiWindowTypeToWmWindowType(ui::mojom::WindowType type) {
  switch (type) {
    case ui::mojom::WindowType::WINDOW:
      return ui::wm::WINDOW_TYPE_NORMAL;
    case ui::mojom::WindowType::PANEL:
      return ui::wm::WINDOW_TYPE_PANEL;
    case ui::mojom::WindowType::CONTROL:
      return ui::wm::WINDOW_TYPE_CONTROL;
    case ui::mojom::WindowType::WINDOW_FRAMELESS:
    case ui::mojom::WindowType::POPUP:
    case ui::mojom::WindowType::BUBBLE:
    case ui::mojom::WindowType::DRAG:
      return ui::wm::WINDOW_TYPE_POPUP;
    case ui::mojom::WindowType::MENU:
      return ui::wm::WINDOW_TYPE_MENU;
    case ui::mojom::WindowType::TOOLTIP:
      return ui::wm::WINDOW_TYPE_TOOLTIP;
    case ui::mojom::WindowType::UNKNOWN:
      return ui::wm::WINDOW_TYPE_UNKNOWN;
  }
  NOTREACHED();
  return ui::wm::WINDOW_TYPE_UNKNOWN;
}

}  // namespace

void SetWindowType(Window* window, ui::mojom::WindowType window_type) {
  window->SetProperty(client::kWindowTypeKey, window_type);
  window->SetType(UiWindowTypeToWmWindowType(window_type));
}

}  // namespace aura

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/property_utils.h"

#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"

namespace aura {
namespace {

client::WindowType UiWindowTypeToWindowType(ui::mojom::WindowType type) {
  switch (type) {
    case ui::mojom::WindowType::WINDOW:
      return client::WINDOW_TYPE_NORMAL;
    case ui::mojom::WindowType::PANEL:
      return client::WINDOW_TYPE_PANEL;
    case ui::mojom::WindowType::CONTROL:
      return client::WINDOW_TYPE_CONTROL;
    case ui::mojom::WindowType::WINDOW_FRAMELESS:
    case ui::mojom::WindowType::POPUP:
    case ui::mojom::WindowType::BUBBLE:
    case ui::mojom::WindowType::DRAG:
      return client::WINDOW_TYPE_POPUP;
    case ui::mojom::WindowType::MENU:
      return client::WINDOW_TYPE_MENU;
    case ui::mojom::WindowType::TOOLTIP:
      return client::WINDOW_TYPE_TOOLTIP;
    case ui::mojom::WindowType::UNKNOWN:
      return client::WINDOW_TYPE_UNKNOWN;
  }
  NOTREACHED();
  return client::WINDOW_TYPE_UNKNOWN;
}

}  // namespace

void SetWindowType(Window* window, ui::mojom::WindowType window_type) {
  window->SetProperty(client::kWindowTypeKey, window_type);
  window->SetType(UiWindowTypeToWindowType(window_type));
}

}  // namespace aura

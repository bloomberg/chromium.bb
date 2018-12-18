// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/window_manager_constants_converters.h"

namespace mojo {

// static
ws::mojom::WindowType
TypeConverter<ws::mojom::WindowType, views::Widget::InitParams::Type>::Convert(
    views::Widget::InitParams::Type type) {
  switch (type) {
    case views::Widget::InitParams::TYPE_WINDOW:
      return ws::mojom::WindowType::WINDOW;
    case views::Widget::InitParams::TYPE_WINDOW_FRAMELESS:
      return ws::mojom::WindowType::WINDOW_FRAMELESS;
    case views::Widget::InitParams::TYPE_CONTROL:
      return ws::mojom::WindowType::CONTROL;
    case views::Widget::InitParams::TYPE_POPUP:
      return ws::mojom::WindowType::POPUP;
    case views::Widget::InitParams::TYPE_MENU:
      return ws::mojom::WindowType::MENU;
    case views::Widget::InitParams::TYPE_TOOLTIP:
      return ws::mojom::WindowType::TOOLTIP;
    case views::Widget::InitParams::TYPE_BUBBLE:
      return ws::mojom::WindowType::BUBBLE;
    case views::Widget::InitParams::TYPE_DRAG:
      return ws::mojom::WindowType::DRAG;
  }
  return ws::mojom::WindowType::POPUP;
}

}  // namespace mojo

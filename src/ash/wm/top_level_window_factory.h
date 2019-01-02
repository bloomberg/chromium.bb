// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TOP_LEVEL_WINDOW_FACTORY_H_
#define ASH_WM_TOP_LEVEL_WINDOW_FACTORY_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "ash/ash_export.h"

namespace aura {
class PropertyConverter;
class Window;
}

namespace ws {
namespace mojom {
enum class WindowType;
}
}  // namespace ws

namespace ash {

// Creates and parents a new top-level window and returns it. The returned
// aura::Window is owned by its parent. A value of null is returned if invalid
// poarameters are supplied.
ASH_EXPORT aura::Window* CreateAndParentTopLevelWindow(
    ws::mojom::WindowType window_type,
    aura::PropertyConverter* property_converter,
    std::map<std::string, std::vector<uint8_t>>* properties);

}  // namespace ash

#endif  // ASH_WM_TOP_LEVEL_WINDOW_FACTORY_H_

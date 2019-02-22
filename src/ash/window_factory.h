// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WINDOW_FACTORY_H_
#define ASH_WINDOW_FACTORY_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/aura/client/window_types.h"

namespace aura {
class Window;
class WindowDelegate;
}  // namespace aura

namespace ash {
namespace window_factory {

// All aura::Window creation in Ash goes through this function. It ensures the
// aura::Window is created as appropriate for Ash.
// Exporting solely for tests.
ASH_EXPORT std::unique_ptr<aura::Window> NewWindow(
    aura::WindowDelegate* delegate = nullptr,
    aura::client::WindowType type = aura::client::WINDOW_TYPE_UNKNOWN);

}  // namespace window_factory
}  // namespace ash

#endif  // ASH_WINDOW_FACTORY_H_

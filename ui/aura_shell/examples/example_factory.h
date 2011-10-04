// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_SHELL_FACTORY_H_
#define UI_AURA_SHELL_SHELL_FACTORY_H_
#pragma once

#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
}

namespace aura_shell {
namespace examples {

void CreatePointyBubble(gfx::NativeWindow parent, const gfx::Point& origin);

void CreateLock();

}  // namespace examples
}  // namespace aura_shell


#endif  // UI_AURA_SHELL_SHELL_FACTORY_H_

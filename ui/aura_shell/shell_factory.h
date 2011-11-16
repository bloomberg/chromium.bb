// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_SHELL_FACTORY_H_
#define UI_AURA_SHELL_SHELL_FACTORY_H_
#pragma once

#include "ui/aura_shell/aura_shell_export.h"

namespace views {
class Widget;
}

// Declarations of shell component factory functions.

namespace aura_shell {

namespace internal {
views::Widget* CreateDesktopBackground();
AURA_SHELL_EXPORT views::Widget* CreateStatusArea();
}  // namespace internal

}  // namespace aura_shell


#endif  // UI_AURA_SHELL_SHELL_FACTORY_H_

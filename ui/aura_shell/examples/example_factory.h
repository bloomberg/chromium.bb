// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_EXAMPLES_EXAMPLE_FACTORY_H_
#define UI_AURA_SHELL_EXAMPLES_EXAMPLE_FACTORY_H_
#pragma once

namespace views {
class View;
}

namespace aura_shell {
namespace examples {

void CreatePointyBubble(views::View* anchor_view);

void CreateLockScreen();

// Creates a window showing samples of commonly used widgets.
void CreateWidgetsWindow();

}  // namespace examples
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_EXAMPLES_EXAMPLE_FACTORY_H_

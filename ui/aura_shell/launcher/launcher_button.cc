// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/launcher/launcher_button.h"

namespace aura_shell {
namespace internal {

LauncherButton::LauncherButton(views::ButtonListener* listener)
    : views::ImageButton(listener) {
}

LauncherButton::~LauncherButton() {
}

}  // namespace internal
}  // namespace aura_shell

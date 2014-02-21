// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_UI_CONTROLS_FACTORY_AURA_H_
#define UI_AURA_TEST_UI_CONTROLS_FACTORY_AURA_H_

#include "ui/base/test/ui_controls_aura.h"

namespace aura {
class WindowEventDispatcher;

namespace test {

// TODO(beng): Should be changed to take a WindowTreeHost.
ui_controls::UIControlsAura* CreateUIControlsAura(
    WindowEventDispatcher* dispatcher);

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_UI_CONTROLS_FACTORY_AURA_H_

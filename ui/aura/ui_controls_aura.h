// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_UI_CONTROLS_AURA_H_
#define UI_AURA_UI_CONTROLS_AURA_H_

namespace ui_controls {
class UIControlsAura;
}

namespace aura {
class RootWindow;

ui_controls::UIControlsAura* CreateUIControlsAura(RootWindow* root_window);

}  // namespace aura

#endif // UI_AURA_UI_CONTROLS_AURA_H_

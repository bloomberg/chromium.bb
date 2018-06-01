// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host_platform.h"

#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout_manager.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"

namespace aura {

base::flat_map<std::string, std::string>
WindowTreeHostPlatform::GetKeyboardLayoutMap() {
  std::unique_ptr<ui::DomKeyboardLayoutManager> layouts =
      std::make_unique<ui::DomKeyboardLayoutManager>();

  ui::KeyboardLayoutEngine* kle =
      ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine();

  for (unsigned int i_domcode = 0;
       i_domcode < ui::kWritingSystemKeyDomCodeEntries; ++i_domcode) {
    ui::DomCode dom_code = ui::writing_system_key_domcodes[i_domcode];
    ui::DomKey dom_key;
    ui::KeyboardCode code;
    if (kle->Lookup(dom_code, 0, &dom_key, &code)) {
      int32_t unicode = 0;
      if (dom_key.IsDeadKey())
        unicode = dom_key.ToDeadKeyCombiningCharacter();
      else if (dom_key.IsCharacter())
        unicode = dom_key.ToCharacter();
      if (unicode)
        // There is only 1 layout available on Ozone, so we arbitrarily assign
        // it to be id = 0.
        layouts->GetLayout(0)->AddKeyMapping(dom_code, unicode);
    }
  }
  return layouts->GetFirstAsciiCapableLayout()->GetMap();
}

}  // namespace aura

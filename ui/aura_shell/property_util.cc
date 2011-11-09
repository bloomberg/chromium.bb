// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/property_util.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/rect.h"

namespace aura_shell {

void SetRestoreBounds(aura::Window* window, const gfx::Rect& bounds) {
  delete GetRestoreBounds(window);
  window->SetProperty(aura::kRestoreBoundsKey, new gfx::Rect(bounds));
}

void SetRestoreBoundsIfNotSet(aura::Window* window) {
  if (!GetRestoreBounds(window))
    SetRestoreBounds(window, window->bounds());
}

const gfx::Rect* GetRestoreBounds(aura::Window* window) {
  return reinterpret_cast<gfx::Rect*>(
      window->GetProperty(aura::kRestoreBoundsKey));
}

void ClearRestoreBounds(aura::Window* window) {
  delete GetRestoreBounds(window);
  window->SetProperty(aura::kRestoreBoundsKey, NULL);
}

}

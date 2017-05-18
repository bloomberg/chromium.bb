// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/public/tooltip_client.h"

#include "ui/aura/window.h"
#include "ui/base/class_property.h"

DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT,
                                        aura::client::TooltipClient*)
DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(AURA_EXPORT, void**)

namespace aura {
namespace client {

DEFINE_UI_CLASS_PROPERTY_KEY(TooltipClient*, kRootWindowTooltipClientKey, NULL);
DEFINE_UI_CLASS_PROPERTY_KEY(base::string16*, kTooltipTextKey, NULL);
DEFINE_UI_CLASS_PROPERTY_KEY(void*, kTooltipIdKey, NULL);

void SetTooltipClient(Window* root_window, TooltipClient* client) {
  DCHECK_EQ(root_window->GetRootWindow(), root_window);
  root_window->SetProperty(kRootWindowTooltipClientKey, client);
}

TooltipClient* GetTooltipClient(Window* root_window) {
  if (root_window)
    DCHECK_EQ(root_window->GetRootWindow(), root_window);
  return root_window ?
      root_window->GetProperty(kRootWindowTooltipClientKey) : NULL;
}

void SetTooltipText(Window* window, base::string16* tooltip_text) {
  window->SetProperty(kTooltipTextKey, tooltip_text);
}

void SetTooltipId(Window* window, void* id) {
  if (id != GetTooltipId(window))
    window->SetProperty(kTooltipIdKey, id);
}

const base::string16 GetTooltipText(Window* window) {
  base::string16* string_ptr = window->GetProperty(kTooltipTextKey);
  return string_ptr ? *string_ptr : base::string16();
}

const void* GetTooltipId(Window* window) {
  return window->GetProperty(kTooltipIdKey);
}

}  // namespace client
}  // namespace aura

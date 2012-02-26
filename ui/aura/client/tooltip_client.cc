// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/tooltip_client.h"

#include "ui/aura/root_window.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(aura::client::TooltipClient*)
DECLARE_WINDOW_PROPERTY_TYPE(string16*)

namespace aura {
namespace client {
namespace {

// A property key to store tooltip text for a window.
const WindowProperty<TooltipClient*> kRootWindowTooltipClientProp = {NULL};
const WindowProperty<TooltipClient*>* const
    kRootWindowTooltipClientKey = &kRootWindowTooltipClientProp;

// A property key to store the tooltip client for the root window.
const WindowProperty<string16*> kTooltipTextProp = {NULL};
const WindowProperty<string16*>* const kTooltipTextKey = &kTooltipTextProp;

}  // namespace

void SetTooltipClient(RootWindow* root_window, TooltipClient* client) {
  root_window->SetProperty(kRootWindowTooltipClientKey, client);
}

TooltipClient* GetTooltipClient(RootWindow* root_window) {
  return root_window ?
      root_window->GetProperty(kRootWindowTooltipClientKey) : NULL;
}

void SetTooltipText(Window* window, string16* tooltip_text) {
  window->SetProperty(kTooltipTextKey, tooltip_text);
}

const string16 GetTooltipText(Window* window) {
  string16* string_ptr = window->GetProperty(kTooltipTextKey);
  return string_ptr ? *string_ptr : string16();
}

}  // namespace client
}  // namespace aura

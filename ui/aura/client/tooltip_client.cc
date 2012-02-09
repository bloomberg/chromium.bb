// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/tooltip_client.h"

#include "ui/aura/root_window.h"

namespace aura {
namespace client {

const char kRootWindowTooltipClientKey[] = "RootWindowTooltipClient";
const char kTooltipTextKey[] = "TooltipText";

void SetTooltipClient(TooltipClient* client) {
  RootWindow::GetInstance()->SetProperty(kRootWindowTooltipClientKey, client);
}

TooltipClient* GetTooltipClient() {
  return reinterpret_cast<TooltipClient*>(
      RootWindow::GetInstance()->GetProperty(kRootWindowTooltipClientKey));
}

void SetTooltipText(Window* window, string16* tooltip_text) {
  window->SetProperty(kTooltipTextKey, tooltip_text);
}

const string16 GetTooltipText(Window* window) {
  string16* string_ptr = reinterpret_cast<string16*>(
      window->GetProperty(kTooltipTextKey));
  return string_ptr ? *string_ptr : string16();
}

}  // namespace client
}  // namespace aura

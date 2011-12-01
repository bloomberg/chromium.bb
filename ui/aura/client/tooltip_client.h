// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_TOOLTIP_CLIENT_H_
#define UI_AURA_CLIENT_TOOLTIP_CLIENT_H_
#pragma once

#include "ui/aura/aura_export.h"
#include "ui/aura/event.h"
#include "ui/gfx/font.h"

namespace aura {

class Window;

class AURA_EXPORT TooltipClient {
 public:
  // Returns the default font used by tooltips.
  static gfx::Font GetDefaultFont();

  // Returns the maximum width of the tooltip. |x| and |y| give the location
  // the tooltip is to be displayed on in screen coordinates.
  static int GetMaxWidth(int x, int y);

  // Informs the shell tooltip manager of change in tooltip for window |target|.
  virtual void UpdateTooltip(Window* target) = 0;
};

}  // namespace aura

#endif  // UI_AURA_CLIENT_TOOLTIP_CLIENT_H_

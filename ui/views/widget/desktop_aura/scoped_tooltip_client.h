// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_SCOPED_TOOLTIP_CLIENT_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_SCOPED_TOOLTIP_CLIENT_H_

#include "base/basictypes.h"

namespace aura {
class RootWindow;
}

namespace views {

namespace corewm {
class TooltipController;
}

// ScopedTooltipClient is responsible for installing a TooltipClient
// implementation on a RootWindow. Additionally it ensures only one
// TooltipController is only ever created. In this way all
// DesktopNativeWidgetAuras share the same TooltipClient.
class ScopedTooltipClient {
 public:
  explicit ScopedTooltipClient(aura::RootWindow* root_window);
  ~ScopedTooltipClient();

 private:
  // Single TooltipController.
  static corewm::TooltipController* tooltip_controller_;

  // Number of ScopedTooltipClients created.
  static int scoped_tooltip_client_count_;

  aura::RootWindow* root_window_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTooltipClient);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_SCOPED_TOOLTIP_CLIENT_H_

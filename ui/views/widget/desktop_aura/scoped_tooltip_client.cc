// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/scoped_tooltip_client.h"

#include "ui/aura/root_window.h"
#include "ui/views/corewm/tooltip_controller.h"

namespace views {

// static
corewm::TooltipController* ScopedTooltipClient::tooltip_controller_ = NULL;

// static
int ScopedTooltipClient::scoped_tooltip_client_count_ = 0;

ScopedTooltipClient::ScopedTooltipClient(aura::RootWindow* root_window)
    : root_window_(root_window) {
  if (scoped_tooltip_client_count_++ == 0) {
    tooltip_controller_ =
        new corewm::TooltipController(gfx::SCREEN_TYPE_NATIVE);
  }
  aura::client::SetTooltipClient(root_window_, tooltip_controller_);
  root_window_->AddPreTargetHandler(tooltip_controller_);
}

ScopedTooltipClient::~ScopedTooltipClient() {
  root_window_->RemovePreTargetHandler(tooltip_controller_);
  aura::client::SetTooltipClient(root_window_, NULL);
  if (--scoped_tooltip_client_count_ == 0) {
    delete tooltip_controller_;
    tooltip_controller_ = NULL;
  }
}

}  // namespace views

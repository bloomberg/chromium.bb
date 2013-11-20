// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host.h"

#include "ui/aura/root_window.h"
#include "ui/aura/window_tree_host_delegate.h"
#include "ui/gfx/point.h"

namespace aura {

////////////////////////////////////////////////////////////////////////////////
// RootWindowHost, public:

RootWindowHost::~RootWindowHost() {
}

void RootWindowHost::ConvertPointToNativeScreen(gfx::Point* point) const {
  delegate_->AsRootWindow()->ConvertPointToHost(point);
  gfx::Point location = GetLocationOnNativeScreen();
  point->Offset(location.x(), location.y());
}

void RootWindowHost::ConvertPointFromNativeScreen(gfx::Point* point) const {
  gfx::Point location = GetLocationOnNativeScreen();
  point->Offset(-location.x(), -location.y());
  delegate_->AsRootWindow()->ConvertPointFromHost(point);
}

////////////////////////////////////////////////////////////////////////////////
// RootWindowHost, protected:

RootWindowHost::RootWindowHost()
    : delegate_(NULL) {
}


}  // namespace aura

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/toplevel_window_container.h"

#include "ui/aura/toplevel_window_event_filter.h"

namespace aura {
namespace internal {

ToplevelWindowContainer::ToplevelWindowContainer()
    : Window(NULL) {
  set_name(L"ToplevelWindowContainer");
  SetEventFilter(new ToplevelWindowEventFilter(this));
}

ToplevelWindowContainer::~ToplevelWindowContainer() {
}

bool ToplevelWindowContainer::IsToplevelWindowContainer() const {
  return true;
}

}  // namespace internal
}  // namespace aura

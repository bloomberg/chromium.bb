// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/toplevel_window_container.h"

#include "base/utf_string_conversions.h"

namespace aura {

ToplevelWindowContainer::ToplevelWindowContainer()
    : Window(NULL) {
  set_name("ToplevelWindowContainer");
}

ToplevelWindowContainer::~ToplevelWindowContainer() {
}

Window* ToplevelWindowContainer::GetTopmostWindowToActivate(
    Window* ignore) const {
  for (Window::Windows::const_reverse_iterator i = children().rbegin();
       i != children().rend(); ++i) {
    if (*i != ignore && (*i)->CanActivate())
      return *i;
  }
  return NULL;
}

ToplevelWindowContainer* ToplevelWindowContainer::AsToplevelWindowContainer() {
  return this;
}

const ToplevelWindowContainer*
    ToplevelWindowContainer::AsToplevelWindowContainer() const {
  return this;
}
}  // namespace aura

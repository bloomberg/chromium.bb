// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/permissions/NavigatorPermissions.h"

#include "core/frame/Navigator.h"
#include "modules/permissions/Permissions.h"

namespace blink {

NavigatorPermissions::NavigatorPermissions() {}

// static
const char* NavigatorPermissions::SupplementName() {
  return "NavigatorPermissions";
}

// static
NavigatorPermissions& NavigatorPermissions::From(Navigator& navigator) {
  NavigatorPermissions* supplement = static_cast<NavigatorPermissions*>(
      Supplement<Navigator>::From(navigator, SupplementName()));
  if (!supplement) {
    supplement = new NavigatorPermissions();
    ProvideTo(navigator, SupplementName(), supplement);
  }
  return *supplement;
}

// static
Permissions* NavigatorPermissions::permissions(Navigator& navigator) {
  NavigatorPermissions& self = NavigatorPermissions::From(navigator);
  if (!self.permissions_)
    self.permissions_ = new Permissions();
  return self.permissions_.Get();
}

void NavigatorPermissions::Trace(blink::Visitor* visitor) {
  visitor->Trace(permissions_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_capabilities/NavigatorMediaCapabilities.h"

#include "modules/media_capabilities/MediaCapabilities.h"
#include "platform/Supplementable.h"

namespace blink {

MediaCapabilities* NavigatorMediaCapabilities::mediaCapabilities(
    Navigator& navigator) {
  NavigatorMediaCapabilities& self =
      NavigatorMediaCapabilities::From(navigator);
  if (!self.capabilities_)
    self.capabilities_ = new MediaCapabilities();
  return self.capabilities_.Get();
}

void NavigatorMediaCapabilities::Trace(blink::Visitor* visitor) {
  visitor->Trace(capabilities_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorMediaCapabilities::NavigatorMediaCapabilities(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

NavigatorMediaCapabilities& NavigatorMediaCapabilities::From(
    Navigator& navigator) {
  NavigatorMediaCapabilities* supplement =
      static_cast<NavigatorMediaCapabilities*>(
          Supplement<Navigator>::From(navigator, SupplementName()));
  if (!supplement) {
    supplement = new NavigatorMediaCapabilities(navigator);
    ProvideTo(navigator, SupplementName(), supplement);
  }
  return *supplement;
}

const char* NavigatorMediaCapabilities::SupplementName() {
  return "NavigatorMediaCapabilities";
}

}  // namespace blink

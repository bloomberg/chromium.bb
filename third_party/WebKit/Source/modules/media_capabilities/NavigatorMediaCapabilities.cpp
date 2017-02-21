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
      NavigatorMediaCapabilities::from(navigator);
  if (!self.m_capabilities)
    self.m_capabilities = new MediaCapabilities();
  return self.m_capabilities.get();
}

DEFINE_TRACE(NavigatorMediaCapabilities) {
  visitor->trace(m_capabilities);
  Supplement<Navigator>::trace(visitor);
}

NavigatorMediaCapabilities::NavigatorMediaCapabilities(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

NavigatorMediaCapabilities& NavigatorMediaCapabilities::from(
    Navigator& navigator) {
  NavigatorMediaCapabilities* supplement =
      static_cast<NavigatorMediaCapabilities*>(
          Supplement<Navigator>::from(navigator, supplementName()));
  if (!supplement) {
    supplement = new NavigatorMediaCapabilities(navigator);
    provideTo(navigator, supplementName(), supplement);
  }
  return *supplement;
}

const char* NavigatorMediaCapabilities::supplementName() {
  return "NavigatorMediaCapabilities";
}

}  // namespace blink

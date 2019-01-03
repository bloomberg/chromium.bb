// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/portal/dom_window_portal_host.h"

namespace blink {

// static
PortalHost* DOMWindowPortalHost::portalHost(LocalDOMWindow& window) {
  return nullptr;
}

}  // namespace blink

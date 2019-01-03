// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_HOST_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"

namespace blink {

class CORE_EXPORT PortalHost : public EventTargetWithInlineData {
  DEFINE_WRAPPERTYPEINFO();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_PORTAL_HOST_H_

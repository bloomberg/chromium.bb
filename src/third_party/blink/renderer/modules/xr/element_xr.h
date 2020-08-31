// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_ELEMENT_XR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_ELEMENT_XR_H_

#include "third_party/blink/renderer/modules/event_target_modules.h"

namespace blink {

class ElementXR {
  STATIC_ONLY(ElementXR);

 public:
  DEFINE_STATIC_ATTRIBUTE_EVENT_LISTENER(beforexrselect, kBeforexrselect)
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_ELEMENT_XR_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fullscreen/element_fullscreen.h"

#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"

namespace blink {

void ElementFullscreen::requestFullscreen(Element& element) {
  Fullscreen::RequestFullscreen(element, Fullscreen::RequestType::kUnprefixed);
}

void ElementFullscreen::webkitRequestFullscreen(Element& element) {
  if (element.IsInShadowTree()) {
    UseCounter::Count(element.GetDocument(),
                      WebFeature::kPrefixedElementRequestFullscreenInShadow);
  }
  Fullscreen::RequestFullscreen(element, Fullscreen::RequestType::kPrefixed);
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fullscreen/ElementFullscreen.h"

#include "core/frame/UseCounter.h"
#include "core/fullscreen/Fullscreen.h"

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

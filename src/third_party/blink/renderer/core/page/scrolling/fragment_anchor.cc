// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"

#include "third_party/blink/renderer/core/page/scrolling/element_fragment_anchor.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_anchor.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

FragmentAnchor* FragmentAnchor::TryCreate(const KURL& url,
                                          LocalFrame& frame) {
  FragmentAnchor* anchor = nullptr;

  anchor = ElementFragmentAnchor::TryCreate(url, frame);
  if (!anchor) {
    if (RuntimeEnabledFeatures::TextFragmentIdentifiersEnabled())
      anchor = TextFragmentAnchor::TryCreate(url, frame);
  }

  return anchor;
}

}  // namespace blink

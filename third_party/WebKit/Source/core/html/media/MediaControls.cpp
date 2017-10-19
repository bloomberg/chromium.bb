// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/media/MediaControls.h"

#include "core/html/media/HTMLMediaElement.h"

namespace blink {

MediaControls::MediaControls(HTMLMediaElement& media_element)
    : media_element_(&media_element) {}

HTMLMediaElement& MediaControls::MediaElement() const {
  return *media_element_;
}

void MediaControls::Trace(blink::Visitor* visitor) {
  visitor->Trace(media_element_);
}

}  // namespace blink

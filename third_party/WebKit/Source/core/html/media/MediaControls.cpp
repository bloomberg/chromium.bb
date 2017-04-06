// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/media/MediaControls.h"

#include "core/html/HTMLMediaElement.h"

namespace blink {

MediaControls::MediaControls(HTMLMediaElement& mediaElement)
    : m_mediaElement(&mediaElement) {}

HTMLMediaElement& MediaControls::mediaElement() const {
  return *m_mediaElement;
}

DEFINE_TRACE(MediaControls) {
  visitor->trace(m_mediaElement);
}

}  // namespace blink

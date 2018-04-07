/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SCROLLBAR_LAYER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SCROLLBAR_LAYER_H_

#include "third_party/blink/public/platform/web_layer.h"
#include "third_party/blink/public/platform/web_scrollbar_theme_geometry.h"
#include "third_party/blink/public/platform/web_scrollbar_theme_painter.h"

namespace cc {

struct ElementId;
}

namespace blink {

class WebScrollbarLayer {
 public:
  virtual ~WebScrollbarLayer() = default;

  virtual WebLayer* Layer() = 0;

  virtual void SetScrollLayer(WebLayer*) = 0;

  // This is an element id for the scrollbar, not the scrolling layer.
  // This is not to be confused with scrolling_element_id, which is the
  // element id of the scrolling layer that has a scrollbar.
  // All scrollbar layers require element ids.
  virtual void SetElementId(const cc::ElementId&) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SCROLLBAR_LAYER_H_

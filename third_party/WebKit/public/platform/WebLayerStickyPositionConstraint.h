/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef WebLayerStickyPositionConstraint_h
#define WebLayerStickyPositionConstraint_h

#include "public/platform/WebLayer.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebRect.h"

namespace blink {

// TODO(flackr): Combine with WebLayerPositionConstraint.
struct WebLayerStickyPositionConstraint {
  // True if the layer is sticky.
  bool is_sticky : 1;

  // For each edge, true if the layer should stick to that edge of its
  // ancestor scroller (or the viewport).
  bool is_anchored_left : 1;
  bool is_anchored_right : 1;
  bool is_anchored_top : 1;
  bool is_anchored_bottom : 1;

  // For each edge, when the anchored bit above is set this gives the distance
  // to keep the element from that edge of the ancestor scroller or viewport.
  float left_offset;
  float right_offset;
  float top_offset;
  float bottom_offset;

  // The layout rectangle of the sticky element before it has been shifted
  // to stick.
  WebRect scroll_container_relative_sticky_box_rect;

  // The layout rectangle of the containing block edges which this sticky
  // element should not be shifted beyond.
  WebRect scroll_container_relative_containing_block_rect;

  // The element ids of the nearest ancestor sticky layers which affect the
  // sticky box constraint rect and the containing block constraint rect
  // respectively.
  cc::ElementId nearest_element_shifting_sticky_box;
  cc::ElementId nearest_element_shifting_containing_block;

  WebLayerStickyPositionConstraint()
      : is_sticky(false),
        is_anchored_left(false),
        is_anchored_right(false),
        is_anchored_top(false),
        is_anchored_bottom(false),
        left_offset(0.f),
        right_offset(0.f),
        top_offset(0.f),
        bottom_offset(0.f) {}
};

}  // namespace blink

#endif  // WebLayerStickyPositionConstraint_h

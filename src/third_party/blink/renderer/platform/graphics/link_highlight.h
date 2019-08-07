// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_LINK_HIGHLIGHT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_LINK_HIGHLIGHT_H_

#include "third_party/blink/renderer/platform/graphics/paint/display_item_client.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace cc {
class Layer;
}

namespace blink {

class EffectPaintPropertyNode;
class FloatPoint;

class PLATFORM_EXPORT LinkHighlight : public DisplayItemClient {
 public:
  ~LinkHighlight() override {}

  virtual void Invalidate() = 0;
  virtual void ClearCurrentGraphicsLayer() = 0;
  virtual cc::Layer* Layer() = 0;

  virtual const EffectPaintPropertyNode& Effect() const = 0;

  // This returns the link highlight offset from its parent transform node,
  // including both the link location and the layer location.
  virtual FloatPoint GetOffsetFromTransformNode() const = 0;

  // DisplayItemClient methods
  // TODO(wangxianzhu): This class doesn't need to be a DisplayItemClient in
  // CompositeAfterPaint.
  String DebugName() const final { return "LinkHighlight"; }
  IntRect VisualRect() const final { return IntRect(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_LINK_HIGHLIGHT_H_

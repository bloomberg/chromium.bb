// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintChunksToCcLayer_h
#define PaintChunksToCcLayer_h

#include "base/memory/ref_counted.h"
#include "platform/PlatformExport.h"
#include "platform/wtf/Vector.h"

namespace cc {
class DisplayItemList;
}  // namespace cc

namespace gfx {
class Vector2dF;
}  // namespace gfx

namespace blink {

class DisplayItemList;
struct PaintChunk;
class PropertyTreeState;

class PLATFORM_EXPORT PaintChunksToCcLayer {
 public:
  static scoped_refptr<cc::DisplayItemList> Convert(
      const Vector<const PaintChunk*>&,
      const PropertyTreeState& layer_state,
      const gfx::Vector2dF& layer_offset,
      const DisplayItemList&);
};

}  // namespace blink

#endif  // PaintArtifactCompositor_h

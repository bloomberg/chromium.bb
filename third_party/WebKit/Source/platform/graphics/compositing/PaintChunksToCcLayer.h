// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintChunksToCcLayer_h
#define PaintChunksToCcLayer_h

#include "base/memory/ref_counted.h"
#include "platform/PlatformExport.h"
#include "wtf/Vector.h"

namespace cc {
class DisplayItemList;
}  // namespace cc

namespace gfx {
class Vector2dF;
}  // namespace gfx

namespace blink {

class DisplayItemList;
class GeometryMapper;
struct PaintChunk;
class PropertyTreeState;

class PLATFORM_EXPORT PaintChunksToCcLayer {
 public:
  static scoped_refptr<cc::DisplayItemList> convert(
      const Vector<const PaintChunk*>&,
      const PropertyTreeState& layerState,
      const gfx::Vector2dF& layerOffset,
      const DisplayItemList&,
      GeometryMapper&);
};

}  // namespace blink

#endif  // PaintArtifactCompositor_h

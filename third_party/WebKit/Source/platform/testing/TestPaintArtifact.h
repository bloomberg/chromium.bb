// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TestPaintArtifact_h
#define TestPaintArtifact_h

#include <memory>
#include "base/memory/ref_counted.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/Vector.h"

namespace cc {
class Layer;
}

namespace blink {

class ClipPaintPropertyNode;
class EffectPaintPropertyNode;
class FloatRect;
class PaintArtifact;
class TransformPaintPropertyNode;

// Useful for quickly making a paint artifact in unit tests.
// Must remain in scope while the paint artifact is used, because it owns the
// display item clients.
//
// Usage:
//   TestPaintArtifact artifact;
//   artifact.Chunk(paintProperties)
//       .RectDrawing(bounds, color)
//       .RectDrawing(bounds2, color2);
//   artifact.Chunk(otherPaintProperties)
//       .RectDrawing(bounds3, color3);
//   DoSomethingWithArtifact(artifact);
class TestPaintArtifact {
  STACK_ALLOCATED();

 public:
  TestPaintArtifact();
  ~TestPaintArtifact();

  // Add to the artifact.
  TestPaintArtifact& Chunk(PassRefPtr<const TransformPaintPropertyNode>,
                           PassRefPtr<const ClipPaintPropertyNode>,
                           PassRefPtr<const EffectPaintPropertyNode>);
  TestPaintArtifact& Chunk(const PaintChunkProperties&);
  TestPaintArtifact& RectDrawing(const FloatRect& bounds, Color);
  TestPaintArtifact& ForeignLayer(const FloatPoint&,
                                  const IntSize&,
                                  scoped_refptr<cc::Layer>);

  // Can't add more things once this is called.
  const PaintArtifact& Build();

 private:
  class DummyRectClient;
  Vector<std::unique_ptr<DummyRectClient>> dummy_clients_;

  // Exists if m_built is false.
  DisplayItemList display_item_list_;
  Vector<PaintChunk> paint_chunks_;

  // Exists if m_built is true.
  PaintArtifact paint_artifact_;

  bool built_;
};

}  // namespace blink

#endif  // TestPaintArtifact_h

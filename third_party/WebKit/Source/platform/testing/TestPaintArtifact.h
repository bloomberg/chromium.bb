// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TestPaintArtifact_h
#define TestPaintArtifact_h

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/RefPtr.h"
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
  TestPaintArtifact& Chunk(RefPtr<const TransformPaintPropertyNode>,
                           RefPtr<const ClipPaintPropertyNode>,
                           RefPtr<const EffectPaintPropertyNode>);
  TestPaintArtifact& Chunk(const PaintChunkProperties&);
  TestPaintArtifact& RectDrawing(const FloatRect& bounds, Color);
  TestPaintArtifact& ForeignLayer(const FloatPoint&,
                                  const IntSize&,
                                  scoped_refptr<cc::Layer>);
  TestPaintArtifact& ScrollHitTest(
      RefPtr<const TransformPaintPropertyNode> scroll_offset);
  TestPaintArtifact& KnownToBeOpaque();

  // Add to the artifact, with specified display item client. These are used
  // to test incremental paint artifact updates.
  TestPaintArtifact& Chunk(DisplayItemClient&,
                           RefPtr<const TransformPaintPropertyNode>,
                           RefPtr<const ClipPaintPropertyNode>,
                           RefPtr<const EffectPaintPropertyNode>);
  TestPaintArtifact& Chunk(DisplayItemClient&, const PaintChunkProperties&);
  TestPaintArtifact& RectDrawing(DisplayItemClient&,
                                 const FloatRect& bounds,
                                 Color);
  TestPaintArtifact& ForeignLayer(DisplayItemClient&,
                                  const FloatPoint&,
                                  const IntSize&,
                                  scoped_refptr<cc::Layer>);
  TestPaintArtifact& ScrollHitTest(
      DisplayItemClient&,
      RefPtr<const TransformPaintPropertyNode> scroll_offset);

  // Can't add more things once this is called.
  const PaintArtifact& Build();

  // Create a new display item client which is owned by this TestPaintArtifact.
  DisplayItemClient& NewClient();

  DisplayItemClient& Client(size_t) const;

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

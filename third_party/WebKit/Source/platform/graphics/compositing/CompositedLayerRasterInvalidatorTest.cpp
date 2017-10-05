// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/CompositedLayerRasterInvalidator.h"

#include "platform/testing/FakeDisplayItemClient.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/wtf/dtoa/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

static const IntRect kDefaultLayerBounds(-9999, -7777, 18888, 16666);

class CompositedLayerRasterInvalidatorTest
    : public ::testing::Test,
      private ScopedSlimmingPaintV2ForTest {
 protected:
  CompositedLayerRasterInvalidatorTest() : ScopedSlimmingPaintV2ForTest(true) {}

  static PropertyTreeState DefaultPropertyTreeState() {
    return PropertyTreeState(TransformPaintPropertyNode::Root(),
                             ClipPaintPropertyNode::Root(),
                             EffectPaintPropertyNode::Root());
  }

  static PaintChunk Chunk(
      int type,
      int raster_invalidation_count = 0,
      PaintChunk::Cacheable cacheable = PaintChunk::kCacheable) {
    DEFINE_STATIC_LOCAL(FakeDisplayItemClient, fake_client, ());
    fake_client.ClearIsJustCreated();
    // The enum arithmetics and magic numbers are to produce different values
    // of paint chunk and raster invalidation properties.
    PaintChunk::Id id(fake_client, static_cast<DisplayItem::Type>(
                                       DisplayItem::kDrawingFirst + type));
    PaintChunk chunk(0, 0, id, PaintChunkProperties(DefaultPropertyTreeState()),
                     cacheable);
    chunk.bounds =
        FloatRect(type * 110, type * 220, type * 220 + 200, type * 110 + 200);
    for (int i = 0; i < raster_invalidation_count; ++i) {
      chunk.raster_invalidation_rects.push_back(FloatRect(
          type * 11, type * 22, type * 22 + 100 + i, type * 11 + 100 + i));
      RasterInvalidationInfo info;
      info.client = &id.client;
      info.reason = static_cast<PaintInvalidationReason>(
          static_cast<int>(PaintInvalidationReason::kFull) + type + i);
      chunk.raster_invalidation_tracking.push_back(info);
    }
    return chunk;
  }

  static const Vector<RasterInvalidationInfo> TrackedRasterInvalidations(
      CompositedLayerRasterInvalidator& invalidator) {
    DCHECK(invalidator.GetRasterInvalidationTracking());
    return invalidator.GetRasterInvalidationTracking()->invalidations;
  }

  static IntRect ChunkRectToLayer(const FloatRect& rect,
                                  const IntPoint& layer_offset) {
    FloatRect r = rect;
    r.MoveBy(-layer_offset);
    return EnclosingIntRect(r);
  }

  static void ExpectDisplayItemInvalidations(
      const Vector<RasterInvalidationInfo>& invalidations,
      size_t index,
      const PaintChunk& chunk,
      const IntPoint& layer_offset = kDefaultLayerBounds.Location()) {
    for (size_t i = 0; i < chunk.raster_invalidation_rects.size(); ++i) {
      SCOPED_TRACE(index + i);
      const auto& info = invalidations[index + i];
      EXPECT_EQ(
          ChunkRectToLayer(chunk.raster_invalidation_rects[i], layer_offset),
          info.rect);
      EXPECT_EQ(&chunk.id.client, info.client);
      EXPECT_EQ(chunk.raster_invalidation_tracking[i].reason, info.reason);
    }
  }

  static void ExpectChunkInvalidation(
      const Vector<RasterInvalidationInfo>& invalidations,
      size_t index,
      const PaintChunk& chunk,
      PaintInvalidationReason reason,
      const IntPoint& layer_offset = kDefaultLayerBounds.Location()) {
    SCOPED_TRACE(index);
    const auto& info = invalidations[index];
    EXPECT_EQ(ChunkRectToLayer(chunk.bounds, layer_offset), info.rect);
    EXPECT_EQ(&chunk.id.client, info.client);
    EXPECT_EQ(reason, info.reason);
  }

  CompositedLayerRasterInvalidator::RasterInvalidationFunction
      kNoopRasterInvalidation = [this](const IntRect& rect) {};

  Vector<IntRect> raster_invalidations_;
};

#define CHUNKS(name, ...)                               \
  PaintChunk name##_array[] = {__VA_ARGS__};            \
  Vector<const PaintChunk*> name;                       \
  for (size_t i = 0; i < ARRAY_SIZE(name##_array); ++i) \
    name.push_back(&name##_array[i]);

TEST_F(CompositedLayerRasterInvalidatorTest, LayerBounds) {
  CompositedLayerRasterInvalidator invalidator(kNoopRasterInvalidation);
  invalidator.SetTracksRasterInvalidations(true);
  CHUNKS(chunks, Chunk(0));

  invalidator.Generate(kDefaultLayerBounds, chunks, DefaultPropertyTreeState());
  // No raster invalidations needed for a new layer.
  EXPECT_TRUE(TrackedRasterInvalidations(invalidator).IsEmpty());

  invalidator.Generate(
      IntRect(kDefaultLayerBounds.Location(), IntSize(1234, 2345)), chunks,
      DefaultPropertyTreeState());
  // No raster invalidations needed if layer origin doesn't change.
  EXPECT_TRUE(TrackedRasterInvalidations(invalidator).IsEmpty());

  invalidator.Generate(IntRect(-555, -666, 777, 888), chunks,
                       DefaultPropertyTreeState());
  // Invalidate the whole layer on layer origin change.
  const auto& invalidations = TrackedRasterInvalidations(invalidator);
  ASSERT_EQ(1u, invalidations.size());
  EXPECT_EQ(IntRect(0, 0, 777, 888), invalidations[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kFullLayer, invalidations[0].reason);
}

TEST_F(CompositedLayerRasterInvalidatorTest, ReorderChunks) {
  CompositedLayerRasterInvalidator invalidator(kNoopRasterInvalidation);
  CHUNKS(chunks, Chunk(0), Chunk(1), Chunk(2));
  invalidator.SetTracksRasterInvalidations(true);
  invalidator.Generate(kDefaultLayerBounds, chunks, DefaultPropertyTreeState());
  EXPECT_TRUE(TrackedRasterInvalidations(invalidator).IsEmpty());

  // Swap chunk 1 and 2. All chunks have their own local raster invalidations.
  CHUNKS(new_chunks, Chunk(0, 2), Chunk(2, 4), Chunk(1, 3));
  new_chunks_array[1].bounds = FloatRect(11, 22, 33, 44);
  invalidator.Generate(kDefaultLayerBounds, new_chunks,
                       DefaultPropertyTreeState());
  const auto& invalidations = TrackedRasterInvalidations(invalidator);
  ASSERT_EQ(5u, invalidations.size());
  // The first chunk should always match because otherwise we won't reuse the
  // CompositedLayerRasterInvalidator (which is according to the first chunk's
  // id). For matched chunk, we issue raster invalidations if any found by
  // PaintController.
  ExpectDisplayItemInvalidations(invalidations, 0, *new_chunks[0]);
  // Invalidated new chunk 1's old (as chunks[2]) and new (as new_chunks[1])
  // bounds.
  ExpectChunkInvalidation(invalidations, 2, *chunks[2],
                          PaintInvalidationReason::kChunkReordered);
  ExpectChunkInvalidation(invalidations, 3, *new_chunks[1],
                          PaintInvalidationReason::kChunkReordered);
  // Invalidated new chunk 2's new bounds. Didn't invalidate old bounds because
  // it's the same as the new bounds.
  ExpectChunkInvalidation(invalidations, 4, *new_chunks[2],
                          PaintInvalidationReason::kChunkReordered);
}

TEST_F(CompositedLayerRasterInvalidatorTest, AppearAndDisappear) {
  CompositedLayerRasterInvalidator invalidator(kNoopRasterInvalidation);
  CHUNKS(chunks, Chunk(0), Chunk(1), Chunk(2));
  invalidator.SetTracksRasterInvalidations(true);
  invalidator.Generate(kDefaultLayerBounds, chunks, DefaultPropertyTreeState());
  EXPECT_TRUE(TrackedRasterInvalidations(invalidator).IsEmpty());

  // Chunk 1 and 2 disappeared, 3 and 4 appeared. All chunks have their own
  // local raster invalidations.
  CHUNKS(new_chunks, Chunk(0, 2), Chunk(3, 3), Chunk(4, 3));
  invalidator.Generate(kDefaultLayerBounds, new_chunks,
                       DefaultPropertyTreeState());
  const auto& invalidations = TrackedRasterInvalidations(invalidator);
  ASSERT_EQ(6u, invalidations.size());
  ExpectDisplayItemInvalidations(invalidations, 0, *new_chunks[0]);
  ExpectChunkInvalidation(invalidations, 2, *new_chunks[1],
                          PaintInvalidationReason::kAppeared);
  ExpectChunkInvalidation(invalidations, 3, *new_chunks[2],
                          PaintInvalidationReason::kAppeared);
  ExpectChunkInvalidation(invalidations, 4, *chunks[1],
                          PaintInvalidationReason::kDisappeared);
  ExpectChunkInvalidation(invalidations, 5, *chunks[2],
                          PaintInvalidationReason::kDisappeared);
}

TEST_F(CompositedLayerRasterInvalidatorTest, AppearAtEnd) {
  CompositedLayerRasterInvalidator invalidator(kNoopRasterInvalidation);
  CHUNKS(chunks, Chunk(0));
  invalidator.SetTracksRasterInvalidations(true);
  invalidator.Generate(kDefaultLayerBounds, chunks, DefaultPropertyTreeState());
  EXPECT_TRUE(TrackedRasterInvalidations(invalidator).IsEmpty());

  CHUNKS(new_chunks, Chunk(0, 2), Chunk(1, 3), Chunk(2, 3));
  invalidator.Generate(kDefaultLayerBounds, new_chunks,
                       DefaultPropertyTreeState());
  const auto& invalidations = TrackedRasterInvalidations(invalidator);
  ASSERT_EQ(4u, invalidations.size());
  ExpectDisplayItemInvalidations(invalidations, 0, *new_chunks[0]);
  ExpectChunkInvalidation(invalidations, 2, *new_chunks[1],
                          PaintInvalidationReason::kAppeared);
  ExpectChunkInvalidation(invalidations, 3, *new_chunks[2],
                          PaintInvalidationReason::kAppeared);
}

TEST_F(CompositedLayerRasterInvalidatorTest, UncacheableChunks) {
  CompositedLayerRasterInvalidator invalidator(kNoopRasterInvalidation);
  CHUNKS(chunks, Chunk(0), Chunk(1, 0, PaintChunk::kUncacheable), Chunk(2));

  invalidator.SetTracksRasterInvalidations(true);
  invalidator.Generate(kDefaultLayerBounds, chunks, DefaultPropertyTreeState());
  EXPECT_TRUE(TrackedRasterInvalidations(invalidator).IsEmpty());

  CHUNKS(new_chunks, Chunk(0, 2), Chunk(2, 3),
         Chunk(1, 3, PaintChunk::kUncacheable));
  invalidator.Generate(kDefaultLayerBounds, new_chunks,
                       DefaultPropertyTreeState());
  const auto& invalidations = TrackedRasterInvalidations(invalidator);
  ASSERT_EQ(5u, invalidations.size());
  ExpectDisplayItemInvalidations(invalidations, 0, *new_chunks[0]);
  ExpectChunkInvalidation(invalidations, 2, *new_chunks[1],
                          PaintInvalidationReason::kChunkReordered);
  ExpectChunkInvalidation(invalidations, 3, *new_chunks[2],
                          PaintInvalidationReason::kChunkUncacheable);
  ExpectChunkInvalidation(invalidations, 4, *chunks[1],
                          PaintInvalidationReason::kChunkUncacheable);
}

TEST_F(CompositedLayerRasterInvalidatorTest, PaintPropertyChange) {
  CompositedLayerRasterInvalidator invalidator(kNoopRasterInvalidation);
  CHUNKS(chunks, Chunk(0), Chunk(1), Chunk(2));
  FloatRoundedRect clip_rect(-100000, -100000, 200000, 200000);
  RefPtr<ClipPaintPropertyNode> clip0 = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      clip_rect);
  RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::Create(
      clip0, TransformPaintPropertyNode::Root(), clip_rect);

  PropertyTreeState layer_state(TransformPaintPropertyNode::Root(), clip0.get(),
                                EffectPaintPropertyNode::Root());
  chunks_array[0].properties = PaintChunkProperties(layer_state);
  chunks_array[1].properties = PaintChunkProperties(layer_state);
  chunks_array[2].properties = PaintChunkProperties(
      PropertyTreeState(TransformPaintPropertyNode::Root(), clip2.get(),
                        EffectPaintPropertyNode::Root()));

  invalidator.SetTracksRasterInvalidations(true);
  invalidator.Generate(kDefaultLayerBounds, chunks, layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations(invalidator).IsEmpty());

  // Change both clip0 and clip2.
  CHUNKS(new_chunks, Chunk(0), Chunk(1), Chunk(2));
  FloatRoundedRect new_clip_rect(-200000, -200000, 400000, 400000);
  clip0->Update(clip0->Parent(), clip0->LocalTransformSpace(), new_clip_rect);
  clip2->Update(clip2->Parent(), clip2->LocalTransformSpace(), new_clip_rect);
  new_chunks_array[0].properties = chunks[0]->properties;
  new_chunks_array[1].properties = chunks[1]->properties;
  new_chunks_array[2].properties = chunks[2]->properties;

  invalidator.Generate(kDefaultLayerBounds, new_chunks, layer_state);
  const auto& invalidations = TrackedRasterInvalidations(invalidator);
  ASSERT_EQ(1u, invalidations.size());
  // Property change in the layer state should not trigger raster invalidation.
  // |clip2| change should trigger raster invalidation.
  ExpectChunkInvalidation(invalidations, 0, *new_chunks[2],
                          PaintInvalidationReason::kPaintProperty);
  invalidator.SetTracksRasterInvalidations(false);
  clip2->ClearChangedToRoot();

  // Change chunk1's properties to use a different property tree state.
  CHUNKS(new_chunks1, Chunk(0), Chunk(1), Chunk(2));
  new_chunks1_array[0].properties = chunks[0]->properties;
  new_chunks1_array[1].properties = chunks[2]->properties;
  new_chunks1_array[2].properties = chunks[2]->properties;

  invalidator.SetTracksRasterInvalidations(true);
  invalidator.Generate(kDefaultLayerBounds, new_chunks1, layer_state);
  const auto& invalidations1 = TrackedRasterInvalidations(invalidator);
  ASSERT_EQ(1u, invalidations1.size());
  ExpectChunkInvalidation(invalidations1, 0, *new_chunks1[1],
                          PaintInvalidationReason::kPaintProperty);
  invalidator.SetTracksRasterInvalidations(false);

  // Change of layer state invalidates the whole layer.
  invalidator.SetTracksRasterInvalidations(true);
  invalidator.Generate(kDefaultLayerBounds, new_chunks1,
                       DefaultPropertyTreeState());
  const auto& invalidations2 = TrackedRasterInvalidations(invalidator);
  ASSERT_EQ(1u, invalidations2.size());
  EXPECT_EQ(PaintInvalidationReason::kFullLayer, invalidations2[0].reason);
}

}  // namespace blink

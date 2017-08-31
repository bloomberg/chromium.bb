// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/ContentLayerClientImpl.h"

#include "cc/layers/picture_layer.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/testing/FakeDisplayItemClient.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/wtf/dtoa/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

static const IntRect kDefaultLayerBounds(-9999, -7777, 18888, 16666);

class ContentLayerClientImplTest : public ::testing::Test,
                                   private ScopedSlimmingPaintV2ForTest {
 protected:
  ContentLayerClientImplTest() : ScopedSlimmingPaintV2ForTest(true) {}

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
      const ContentLayerClientImpl& c) {
    return c.TrackedRasterInvalidations();
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
};

#define CHUNKS(name, ...)                               \
  PaintChunk name##_array[] = {__VA_ARGS__};            \
  Vector<const PaintChunk*> name;                       \
  for (size_t i = 0; i < ARRAY_SIZE(name##_array); ++i) \
    name.push_back(&name##_array[i]);

TEST_F(ContentLayerClientImplTest, LayerBounds) {
  ContentLayerClientImpl c;
  c.SetTracksRasterInvalidations(true);
  CHUNKS(chunks, Chunk(0));

  auto cc_layer = c.UpdateCcPictureLayer(PaintArtifact(), kDefaultLayerBounds,
                                         chunks, DefaultPropertyTreeState());
  ASSERT_TRUE(cc_layer);
  EXPECT_EQ(gfx::Rect(kDefaultLayerBounds.Size()), c.PaintableRegion());
  EXPECT_EQ(gfx::Size(kDefaultLayerBounds.Size()), cc_layer->bounds());
  // No raster invalidations needed for a new layer.
  EXPECT_TRUE(TrackedRasterInvalidations(c).IsEmpty());

  auto cc_layer1 = c.UpdateCcPictureLayer(
      PaintArtifact(),
      IntRect(kDefaultLayerBounds.Location(), IntSize(1234, 2345)), chunks,
      DefaultPropertyTreeState());
  EXPECT_EQ(cc_layer, cc_layer1);
  EXPECT_EQ(gfx::Rect(0, 0, 1234, 2345), c.PaintableRegion());
  EXPECT_EQ(gfx::Size(1234, 2345), cc_layer->bounds());
  // No raster invalidations needed if layer origin doesn't change.
  EXPECT_TRUE(TrackedRasterInvalidations(c).IsEmpty());

  auto cc_layer2 =
      c.UpdateCcPictureLayer(PaintArtifact(), IntRect(-555, -666, 777, 888),
                             chunks, DefaultPropertyTreeState());
  EXPECT_EQ(cc_layer, cc_layer2);
  EXPECT_EQ(gfx::Rect(0, 0, 777, 888), c.PaintableRegion());
  EXPECT_EQ(gfx::Size(777, 888), cc_layer->bounds());
  // Invalidate the whole layer on layer origin change.
  const auto& invalidations = TrackedRasterInvalidations(c);
  ASSERT_EQ(1u, invalidations.size());
  EXPECT_EQ(IntRect(0, 0, 777, 888), invalidations[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kFullLayer, invalidations[0].reason);
}

TEST_F(ContentLayerClientImplTest, RasterInvalidationReorderChunks) {
  ContentLayerClientImpl c;
  CHUNKS(chunks, Chunk(0), Chunk(1), Chunk(2));
  c.SetTracksRasterInvalidations(true);
  c.UpdateCcPictureLayer(PaintArtifact(), kDefaultLayerBounds, chunks,
                         DefaultPropertyTreeState());
  EXPECT_TRUE(TrackedRasterInvalidations(c).IsEmpty());

  // Swap chunk 1 and 2. All chunks have their own local raster invalidations.
  CHUNKS(new_chunks, Chunk(0, 2), Chunk(2, 4), Chunk(1, 3));
  new_chunks_array[1].bounds = FloatRect(11, 22, 33, 44);
  c.UpdateCcPictureLayer(PaintArtifact(), kDefaultLayerBounds, new_chunks,
                         DefaultPropertyTreeState());
  const auto& invalidations = TrackedRasterInvalidations(c);
  ASSERT_EQ(5u, invalidations.size());
  // The first chunk should always match because otherwise we won't reuse the
  // ContentLayerClientImpl (which is according to the first chunk's id).
  // For matched chunk, we issue raster invalidations if any found by
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

TEST_F(ContentLayerClientImplTest, RasterInvalidationAppearAndDisappear) {
  ContentLayerClientImpl c;
  CHUNKS(chunks, Chunk(0), Chunk(1), Chunk(2));
  c.SetTracksRasterInvalidations(true);
  c.UpdateCcPictureLayer(PaintArtifact(), kDefaultLayerBounds, chunks,
                         DefaultPropertyTreeState());
  EXPECT_TRUE(TrackedRasterInvalidations(c).IsEmpty());

  // Chunk 1 and 2 disappeared, 3 and 4 appeared. All chunks have their own
  // local raster invalidations.
  CHUNKS(new_chunks, Chunk(0, 2), Chunk(3, 3), Chunk(4, 3));
  c.UpdateCcPictureLayer(PaintArtifact(), kDefaultLayerBounds, new_chunks,
                         DefaultPropertyTreeState());
  const auto& invalidations = TrackedRasterInvalidations(c);
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

TEST_F(ContentLayerClientImplTest, RasterInvalidationAppearAtEnd) {
  ContentLayerClientImpl c;
  CHUNKS(chunks, Chunk(0));
  c.SetTracksRasterInvalidations(true);
  c.UpdateCcPictureLayer(PaintArtifact(), kDefaultLayerBounds, chunks,
                         DefaultPropertyTreeState());
  EXPECT_TRUE(TrackedRasterInvalidations(c).IsEmpty());

  CHUNKS(new_chunks, Chunk(0, 2), Chunk(1, 3), Chunk(2, 3));
  c.UpdateCcPictureLayer(PaintArtifact(), kDefaultLayerBounds, new_chunks,
                         DefaultPropertyTreeState());
  const auto& invalidations = TrackedRasterInvalidations(c);
  ASSERT_EQ(4u, invalidations.size());
  ExpectDisplayItemInvalidations(invalidations, 0, *new_chunks[0]);
  ExpectChunkInvalidation(invalidations, 2, *new_chunks[1],
                          PaintInvalidationReason::kAppeared);
  ExpectChunkInvalidation(invalidations, 3, *new_chunks[2],
                          PaintInvalidationReason::kAppeared);
}

TEST_F(ContentLayerClientImplTest, RasterInvalidationUncacheableChunks) {
  ContentLayerClientImpl c;
  CHUNKS(chunks, Chunk(0), Chunk(1, 0, PaintChunk::kUncacheable), Chunk(2));

  c.SetTracksRasterInvalidations(true);
  c.UpdateCcPictureLayer(PaintArtifact(), kDefaultLayerBounds, chunks,
                         DefaultPropertyTreeState());
  EXPECT_TRUE(TrackedRasterInvalidations(c).IsEmpty());

  CHUNKS(new_chunks, Chunk(0, 2), Chunk(2, 3),
         Chunk(1, 3, PaintChunk::kUncacheable));
  c.UpdateCcPictureLayer(PaintArtifact(), kDefaultLayerBounds, new_chunks,
                         DefaultPropertyTreeState());
  const auto& invalidations = TrackedRasterInvalidations(c);
  ASSERT_EQ(5u, invalidations.size());
  ExpectDisplayItemInvalidations(invalidations, 0, *new_chunks[0]);
  ExpectChunkInvalidation(invalidations, 2, *new_chunks[1],
                          PaintInvalidationReason::kChunkReordered);
  ExpectChunkInvalidation(invalidations, 3, *new_chunks[2],
                          PaintInvalidationReason::kChunkUncacheable);
  ExpectChunkInvalidation(invalidations, 4, *chunks[1],
                          PaintInvalidationReason::kChunkUncacheable);
}

TEST_F(ContentLayerClientImplTest, RasterInvalidationPaintPropertyChange) {
  ContentLayerClientImpl c;
  CHUNKS(chunks, Chunk(0), Chunk(1), Chunk(2));
  FloatRoundedRect clip_rect(-100000, -100000, 200000, 200000);
  RefPtr<ClipPaintPropertyNode> clip0 = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      clip_rect);
  RefPtr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::Create(
      clip0, TransformPaintPropertyNode::Root(), clip_rect);

  PropertyTreeState layer_state(TransformPaintPropertyNode::Root(), clip0.Get(),
                                EffectPaintPropertyNode::Root());
  chunks_array[0].properties = PaintChunkProperties(layer_state);
  chunks_array[1].properties = PaintChunkProperties(layer_state);
  chunks_array[2].properties = PaintChunkProperties(
      PropertyTreeState(TransformPaintPropertyNode::Root(), clip2.Get(),
                        EffectPaintPropertyNode::Root()));

  c.SetTracksRasterInvalidations(true);
  c.UpdateCcPictureLayer(PaintArtifact(), kDefaultLayerBounds, chunks,
                         layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations(c).IsEmpty());

  // Change both clip0 and clip2.
  CHUNKS(new_chunks, Chunk(0), Chunk(1), Chunk(2));
  FloatRoundedRect new_clip_rect(-200000, -200000, 400000, 400000);
  clip0->Update(clip0->Parent(), clip0->LocalTransformSpace(), new_clip_rect);
  clip2->Update(clip2->Parent(), clip2->LocalTransformSpace(), new_clip_rect);
  new_chunks_array[0].properties = chunks[0]->properties;
  new_chunks_array[1].properties = chunks[1]->properties;
  new_chunks_array[2].properties = chunks[2]->properties;

  c.UpdateCcPictureLayer(PaintArtifact(), kDefaultLayerBounds, new_chunks,
                         layer_state);
  const auto& invalidations = TrackedRasterInvalidations(c);
  ASSERT_EQ(1u, invalidations.size());
  // Property change in the layer state should not trigger raster invalidation.
  // |clip2| change should trigger raster invalidation.
  ExpectChunkInvalidation(invalidations, 0, *new_chunks[2],
                          PaintInvalidationReason::kPaintProperty);
  c.SetTracksRasterInvalidations(false);
  clip2->ClearChangedToRoot();

  // Change chunk1's properties to use a different property tree state.
  CHUNKS(new_chunks1, Chunk(0), Chunk(1), Chunk(2));
  new_chunks1_array[0].properties = chunks[0]->properties;
  new_chunks1_array[1].properties = chunks[2]->properties;
  new_chunks1_array[2].properties = chunks[2]->properties;

  c.SetTracksRasterInvalidations(true);
  c.UpdateCcPictureLayer(PaintArtifact(), kDefaultLayerBounds, new_chunks1,
                         layer_state);
  const auto& invalidations1 = TrackedRasterInvalidations(c);
  ASSERT_EQ(1u, invalidations1.size());
  ExpectChunkInvalidation(invalidations1, 0, *new_chunks1[1],
                          PaintInvalidationReason::kPaintProperty);
  c.SetTracksRasterInvalidations(false);

  // Change of layer state invalidates the whole layer.
  c.SetTracksRasterInvalidations(true);
  c.UpdateCcPictureLayer(PaintArtifact(), kDefaultLayerBounds, new_chunks1,
                         DefaultPropertyTreeState());
  const auto& invalidations2 = TrackedRasterInvalidations(c);
  ASSERT_EQ(1u, invalidations2.size());
  EXPECT_EQ(PaintInvalidationReason::kFullLayer, invalidations2[0].reason);
}

}  // namespace blink

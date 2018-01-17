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
    DCHECK(invalidator.GetTracking());
    return invalidator.GetTracking()->Invalidations();
  }

  static IntRect ChunkRectToLayer(const FloatRect& rect,
                                  const IntPoint& chunk_offset_from_layer) {
    FloatRect r = rect;
    r.MoveBy(chunk_offset_from_layer);
    return EnclosingIntRect(r);
  }

  CompositedLayerRasterInvalidator::RasterInvalidationFunction
      kNoopRasterInvalidation = [this](const IntRect& rect) {};

  Vector<IntRect> raster_invalidations_;
};

#define EXPECT_DISPLAY_ITEM_INVALIDATIONS(invalidations, index, chunk)        \
  do {                                                                        \
    for (size_t i = 0; i < (chunk).raster_invalidation_rects.size(); ++i) {   \
      SCOPED_TRACE(index + i);                                                \
      const auto& info = (invalidations)[index + i];                          \
      EXPECT_EQ(ChunkRectToLayer((chunk).raster_invalidation_rects[i],        \
                                 -kDefaultLayerBounds.Location()),            \
                info.rect);                                                   \
      EXPECT_EQ(&(chunk).id.client, info.client);                             \
      EXPECT_EQ((chunk).raster_invalidation_tracking[i].reason, info.reason); \
    }                                                                         \
  } while (false)

#define EXPECT_CHUNK_INVALIDATION_WITH_LAYER_OFFSET(                      \
    invalidations, index, chunk, expected_reason, layer_offset)           \
  do {                                                                    \
    const auto& info = (invalidations)[index];                            \
    EXPECT_EQ(ChunkRectToLayer((chunk).bounds, layer_offset), info.rect); \
    EXPECT_EQ(&(chunk).id.client, info.client);                           \
    EXPECT_EQ(expected_reason, info.reason);                              \
  } while (false)

#define EXPECT_CHUNK_INVALIDATION(invalidations, index, chunk, reason) \
  EXPECT_CHUNK_INVALIDATION_WITH_LAYER_OFFSET(                         \
      invalidations, index, chunk, reason, -kDefaultLayerBounds.Location())

#define EXPECT_INCREMENTAL_INVALIDATION(invalidations, index, chunk,         \
                                        chunk_rect)                          \
  do {                                                                       \
    const auto& info = (invalidations)[index];                               \
    EXPECT_EQ(ChunkRectToLayer(chunk_rect, -kDefaultLayerBounds.Location()), \
              info.rect);                                                    \
    EXPECT_EQ(&(chunk).id.client, info.client);                              \
    EXPECT_EQ(PaintInvalidationReason::kIncremental, info.reason);           \
  } while (false)

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

  invalidator.Generate(kDefaultLayerBounds, chunks, DefaultPropertyTreeState());
  // No raster invalidations needed if layer origin doesn't change.
  EXPECT_TRUE(TrackedRasterInvalidations(invalidator).IsEmpty());

  auto new_layer_bounds = kDefaultLayerBounds;
  new_layer_bounds.Move(66, 77);
  invalidator.Generate(new_layer_bounds, chunks, DefaultPropertyTreeState());
  // Change of layer origin causes change of chunk0's transform to layer.
  const auto& invalidations = TrackedRasterInvalidations(invalidator);
  ASSERT_EQ(2u, invalidations.size());
  EXPECT_CHUNK_INVALIDATION(invalidations, 0, *chunks[0],
                            PaintInvalidationReason::kPaintProperty);
  EXPECT_CHUNK_INVALIDATION_WITH_LAYER_OFFSET(
      invalidations, 1, *chunks[0], PaintInvalidationReason::kPaintProperty,
      -new_layer_bounds.Location());
}

TEST_F(CompositedLayerRasterInvalidatorTest, ReorderChunks) {
  CompositedLayerRasterInvalidator invalidator(kNoopRasterInvalidation);
  CHUNKS(chunks, Chunk(0), Chunk(1), Chunk(2));
  invalidator.SetTracksRasterInvalidations(true);
  invalidator.Generate(kDefaultLayerBounds, chunks, DefaultPropertyTreeState());
  EXPECT_TRUE(TrackedRasterInvalidations(invalidator).IsEmpty());

  // Swap chunk 1 and 2. All chunks have their own local raster invalidations.
  CHUNKS(new_chunks, Chunk(0, 2), Chunk(2, 4), Chunk(1, 3));
  new_chunks_array[2].bounds = FloatRect(11, 22, 33, 44);
  invalidator.Generate(kDefaultLayerBounds, new_chunks,
                       DefaultPropertyTreeState());
  const auto& invalidations = TrackedRasterInvalidations(invalidator);
  ASSERT_EQ(8u, invalidations.size());
  // The first chunk should always match because otherwise we won't reuse the
  // CompositedLayerRasterInvalidator (which is according to the first chunk's
  // id). For matched chunk, we issue raster invalidations if any found by
  // PaintController.
  EXPECT_DISPLAY_ITEM_INVALIDATIONS(invalidations, 0, *new_chunks[0]);
  EXPECT_DISPLAY_ITEM_INVALIDATIONS(invalidations, 2, *new_chunks[1]);
  // Invalidated new chunk 2's old (as chunks[1]) and new (as new_chunks[2])
  // bounds.
  EXPECT_CHUNK_INVALIDATION(invalidations, 6, *chunks[1],
                            PaintInvalidationReason::kChunkReordered);
  EXPECT_CHUNK_INVALIDATION(invalidations, 7, *new_chunks[2],
                            PaintInvalidationReason::kChunkReordered);
}

TEST_F(CompositedLayerRasterInvalidatorTest, ReorderChunkSubsequences) {
  CompositedLayerRasterInvalidator invalidator(kNoopRasterInvalidation);
  CHUNKS(chunks, Chunk(0), Chunk(1), Chunk(2), Chunk(3), Chunk(4));
  invalidator.SetTracksRasterInvalidations(true);
  invalidator.Generate(kDefaultLayerBounds, chunks, DefaultPropertyTreeState());
  EXPECT_TRUE(TrackedRasterInvalidations(invalidator).IsEmpty());

  // Swap chunk (1,2) and (3,4). All chunks have their own local raster
  // invalidations.
  CHUNKS(new_chunks, Chunk(0, 2), Chunk(3, 3), Chunk(4, 4), Chunk(1, 1),
         Chunk(2, 2));
  new_chunks_array[3].bounds = FloatRect(11, 22, 33, 44);
  invalidator.Generate(kDefaultLayerBounds, new_chunks,
                       DefaultPropertyTreeState());
  const auto& invalidations = TrackedRasterInvalidations(invalidator);
  ASSERT_EQ(12u, invalidations.size());
  // The first chunk should always match because otherwise we won't reuse the
  // CompositedLayerRasterInvalidator (which is according to the first chunk's
  // id). For matched chunk, we issue raster invalidations if any found by
  // PaintController.
  EXPECT_DISPLAY_ITEM_INVALIDATIONS(invalidations, 0, *new_chunks[0]);
  EXPECT_DISPLAY_ITEM_INVALIDATIONS(invalidations, 2, *new_chunks[1]);
  EXPECT_DISPLAY_ITEM_INVALIDATIONS(invalidations, 5, *new_chunks[2]);
  // Invalidated new chunk 3's old (as chunks[1]) and new (as new_chunks[3])
  // bounds.
  EXPECT_CHUNK_INVALIDATION(invalidations, 9, *chunks[1],
                            PaintInvalidationReason::kChunkReordered);
  EXPECT_CHUNK_INVALIDATION(invalidations, 10, *new_chunks[3],
                            PaintInvalidationReason::kChunkReordered);
  // Invalidated new chunk 4's new bounds. Didn't invalidate old bounds because
  // it's the same as the new bounds.
  EXPECT_CHUNK_INVALIDATION(invalidations, 11, *new_chunks[4],
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
  EXPECT_DISPLAY_ITEM_INVALIDATIONS(invalidations, 0, *new_chunks[0]);
  EXPECT_CHUNK_INVALIDATION(invalidations, 2, *new_chunks[1],
                            PaintInvalidationReason::kAppeared);
  EXPECT_CHUNK_INVALIDATION(invalidations, 3, *new_chunks[2],
                            PaintInvalidationReason::kAppeared);
  EXPECT_CHUNK_INVALIDATION(invalidations, 4, *chunks[1],
                            PaintInvalidationReason::kDisappeared);
  EXPECT_CHUNK_INVALIDATION(invalidations, 5, *chunks[2],
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
  EXPECT_DISPLAY_ITEM_INVALIDATIONS(invalidations, 0, *new_chunks[0]);
  EXPECT_CHUNK_INVALIDATION(invalidations, 2, *new_chunks[1],
                            PaintInvalidationReason::kAppeared);
  EXPECT_CHUNK_INVALIDATION(invalidations, 3, *new_chunks[2],
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
  ASSERT_EQ(7u, invalidations.size());
  EXPECT_DISPLAY_ITEM_INVALIDATIONS(invalidations, 0, *new_chunks[0]);
  EXPECT_DISPLAY_ITEM_INVALIDATIONS(invalidations, 2, *new_chunks[1]);
  EXPECT_CHUNK_INVALIDATION(invalidations, 5, *new_chunks[2],
                            PaintInvalidationReason::kChunkUncacheable);
  EXPECT_CHUNK_INVALIDATION(invalidations, 6, *chunks[1],
                            PaintInvalidationReason::kChunkUncacheable);
}

// Tests the path based on ClipPaintPropertyNode::Changed().
TEST_F(CompositedLayerRasterInvalidatorTest, ClipPropertyChangeRounded) {
  CompositedLayerRasterInvalidator invalidator(kNoopRasterInvalidation);
  CHUNKS(chunks, Chunk(0), Chunk(1), Chunk(2));
  FloatRoundedRect::Radii radii(FloatSize(1, 2), FloatSize(2, 3),
                                FloatSize(3, 4), FloatSize(4, 5));
  FloatRoundedRect clip_rect(FloatRect(-1000, -1000, 2000, 2000), radii);
  scoped_refptr<ClipPaintPropertyNode> clip0 = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      clip_rect);
  scoped_refptr<ClipPaintPropertyNode> clip2 = ClipPaintPropertyNode::Create(
      clip0, TransformPaintPropertyNode::Root(), clip_rect);

  PropertyTreeState layer_state(TransformPaintPropertyNode::Root(), clip0.get(),
                                EffectPaintPropertyNode::Root());
  chunks_array[0].properties = PaintChunkProperties(layer_state);
  chunks_array[1].properties = PaintChunkProperties(layer_state);
  chunks_array[2].properties = PaintChunkProperties(
      PropertyTreeState(TransformPaintPropertyNode::Root(), clip2.get(),
                        EffectPaintPropertyNode::Root()));

  GeometryMapperClipCache::ClearCache();
  invalidator.SetTracksRasterInvalidations(true);
  invalidator.Generate(kDefaultLayerBounds, chunks, layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations(invalidator).IsEmpty());

  // Change both clip0 and clip2.
  LOG(ERROR) << "22222222222222222222222222222222222222222222";
  CHUNKS(new_chunks, Chunk(0), Chunk(1), Chunk(2));
  FloatRoundedRect new_clip_rect(FloatRect(-2000, -2000, 4000, 4000), radii);
  LOG(ERROR) << "new_clip_rect: " << new_clip_rect.ToString();
  clip0->Update(clip0->Parent(), clip0->LocalTransformSpace(), new_clip_rect);
  clip2->Update(clip2->Parent(), clip2->LocalTransformSpace(), new_clip_rect);
  new_chunks_array[0].properties = chunks[0]->properties;
  new_chunks_array[1].properties = chunks[1]->properties;
  new_chunks_array[2].properties = chunks[2]->properties;

  GeometryMapperClipCache::ClearCache();
  invalidator.Generate(kDefaultLayerBounds, new_chunks, layer_state);
  const auto& invalidations = TrackedRasterInvalidations(invalidator);
  ASSERT_EQ(1u, invalidations.size());
  // Property change in the layer state should not trigger raster invalidation.
  // |clip2| change should trigger raster invalidation.
  EXPECT_CHUNK_INVALIDATION(invalidations, 0, *new_chunks[2],
                            PaintInvalidationReason::kPaintProperty);
  invalidator.SetTracksRasterInvalidations(false);
  clip2->ClearChangedToRoot();
  LOG(ERROR) << "333333333333333333333333333333333333333333333";

  // Change chunk1's properties to use a different property tree state.
  CHUNKS(new_chunks1, Chunk(0), Chunk(1), Chunk(2));
  new_chunks1_array[0].properties = chunks[0]->properties;
  new_chunks1_array[1].properties = chunks[2]->properties;
  new_chunks1_array[2].properties = chunks[2]->properties;

  GeometryMapperClipCache::ClearCache();
  invalidator.SetTracksRasterInvalidations(true);
  invalidator.Generate(kDefaultLayerBounds, new_chunks1, layer_state);
  const auto& invalidations1 = TrackedRasterInvalidations(invalidator);
  ASSERT_EQ(1u, invalidations1.size());
  EXPECT_CHUNK_INVALIDATION(invalidations1, 0, *new_chunks1[1],
                            PaintInvalidationReason::kPaintProperty);
  invalidator.SetTracksRasterInvalidations(false);
}

// Tests the path detecting change of PaintChunkInfo::chunk_to_layer_clip.
TEST_F(CompositedLayerRasterInvalidatorTest, ClipPropertyChangeSimple) {
  CompositedLayerRasterInvalidator invalidator(kNoopRasterInvalidation);
  CHUNKS(chunks, Chunk(0), Chunk(1));
  FloatRoundedRect clip_rect(-1000, -1000, 2000, 2000);
  scoped_refptr<ClipPaintPropertyNode> clip0 = ClipPaintPropertyNode::Create(
      ClipPaintPropertyNode::Root(), TransformPaintPropertyNode::Root(),
      clip_rect);
  scoped_refptr<ClipPaintPropertyNode> clip1 = ClipPaintPropertyNode::Create(
      clip0, TransformPaintPropertyNode::Root(), clip_rect);

  PropertyTreeState layer_state = PropertyTreeState::Root();
  chunks_array[0].properties = PaintChunkProperties(
      PropertyTreeState(TransformPaintPropertyNode::Root(), clip0.get(),
                        EffectPaintPropertyNode::Root()));
  chunks_array[0].bounds = clip_rect.Rect();
  chunks_array[1].properties = PaintChunkProperties(
      PropertyTreeState(TransformPaintPropertyNode::Root(), clip1.get(),
                        EffectPaintPropertyNode::Root()));
  chunks_array[1].bounds = clip_rect.Rect();

  GeometryMapperClipCache::ClearCache();
  invalidator.SetTracksRasterInvalidations(true);
  invalidator.Generate(kDefaultLayerBounds, chunks, layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations(invalidator).IsEmpty());

  // Change clip1 to bigger, which is still bound by clip0, resulting no actual
  // visual change.
  CHUNKS(new_chunks1, Chunk(0), Chunk(1));
  FloatRoundedRect new_clip_rect1(-2000, -2000, 4000, 4000);
  clip1->Update(clip1->Parent(), clip1->LocalTransformSpace(), new_clip_rect1);
  new_chunks1_array[0].properties = chunks[0]->properties;
  new_chunks1_array[0].bounds = chunks[0]->bounds;
  new_chunks1_array[1].properties = chunks[1]->properties;
  new_chunks1_array[1].bounds = chunks[1]->bounds;

  GeometryMapperClipCache::ClearCache();
  invalidator.Generate(kDefaultLayerBounds, new_chunks1, layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations(invalidator).IsEmpty());
  clip1->ClearChangedToRoot();

  // Change clip1 to smaller.
  CHUNKS(new_chunks2, Chunk(0), Chunk(1));
  FloatRoundedRect new_clip_rect2(-500, -500, 1000, 1000);
  clip1->Update(clip1->Parent(), clip1->LocalTransformSpace(), new_clip_rect2);
  new_chunks2_array[0].properties = chunks[0]->properties;
  new_chunks2_array[0].bounds = chunks[0]->bounds;
  new_chunks2_array[1].properties = chunks[1]->properties;
  new_chunks2_array[1].bounds = new_clip_rect2.Rect();

  GeometryMapperClipCache::ClearCache();
  invalidator.Generate(kDefaultLayerBounds, new_chunks2, layer_state);
  const auto& invalidations = TrackedRasterInvalidations(invalidator);
  ASSERT_EQ(4u, invalidations.size());
  // |clip1| change should trigger incremental raster invalidation.
  EXPECT_INCREMENTAL_INVALIDATION(invalidations, 0, *new_chunks2[1],
                                  IntRect(-1000, -1000, 2000, 500));
  EXPECT_INCREMENTAL_INVALIDATION(invalidations, 1, *new_chunks2[1],
                                  IntRect(-1000, -500, 500, 1000));
  EXPECT_INCREMENTAL_INVALIDATION(invalidations, 2, *new_chunks2[1],
                                  IntRect(500, -500, 500, 1000));
  EXPECT_INCREMENTAL_INVALIDATION(invalidations, 3, *new_chunks2[1],
                                  IntRect(-1000, 500, 2000, 500));
  invalidator.SetTracksRasterInvalidations(false);
  clip1->ClearChangedToRoot();

  // Change clip1 bigger at one side.
  CHUNKS(new_chunks3, Chunk(0), Chunk(1));
  FloatRoundedRect new_clip_rect3(-500, -500, 2000, 1000);
  clip1->Update(clip1->Parent(), clip1->LocalTransformSpace(), new_clip_rect3);
  new_chunks3_array[0].properties = chunks[0]->properties;
  new_chunks3_array[0].bounds = chunks[0]->bounds;
  new_chunks3_array[1].properties = chunks[1]->properties;
  new_chunks3_array[1].bounds = new_clip_rect3.Rect();

  GeometryMapperClipCache::ClearCache();
  invalidator.SetTracksRasterInvalidations(true);
  invalidator.Generate(kDefaultLayerBounds, new_chunks3, layer_state);
  const auto& invalidations1 = TrackedRasterInvalidations(invalidator);
  ASSERT_EQ(1u, invalidations1.size());
  // |clip1| change should trigger incremental raster invalidation.
  EXPECT_INCREMENTAL_INVALIDATION(invalidations1, 0, *new_chunks3[1],
                                  IntRect(500, -500, 500, 1000));
  invalidator.SetTracksRasterInvalidations(false);
  clip1->ClearChangedToRoot();
}

TEST_F(CompositedLayerRasterInvalidatorTest, TransformPropertyChange) {
  CompositedLayerRasterInvalidator invalidator(kNoopRasterInvalidation);
  CHUNKS(chunks, Chunk(0), Chunk(1));

  auto layer_transform = TransformPaintPropertyNode::Create(
      TransformPaintPropertyNode::Root(), TransformationMatrix().Scale(5),
      FloatPoint3D());
  auto transform0 = TransformPaintPropertyNode::Create(
      layer_transform, TransformationMatrix().Translate(10, 20),
      FloatPoint3D());
  auto transform1 = TransformPaintPropertyNode::Create(
      transform0, TransformationMatrix().Translate(-50, -60), FloatPoint3D());

  PropertyTreeState layer_state(layer_transform.get(),
                                ClipPaintPropertyNode::Root(),
                                EffectPaintPropertyNode::Root());
  chunks_array[0].properties = PaintChunkProperties(
      PropertyTreeState(transform0.get(), ClipPaintPropertyNode::Root(),
                        EffectPaintPropertyNode::Root()));
  chunks_array[1].properties = PaintChunkProperties(
      PropertyTreeState(transform1.get(), ClipPaintPropertyNode::Root(),
                        EffectPaintPropertyNode::Root()));

  GeometryMapperTransformCache::ClearCache();
  invalidator.SetTracksRasterInvalidations(true);
  invalidator.Generate(kDefaultLayerBounds, chunks, layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations(invalidator).IsEmpty());

  // Change layer_transform should not cause raster invalidation in the layer.
  CHUNKS(new_chunks, Chunk(0), Chunk(1));
  layer_transform->Update(layer_transform->Parent(),
                          TransformationMatrix().Scale(10), FloatPoint3D());
  new_chunks_array[0].properties = chunks[0]->properties;
  new_chunks_array[1].properties = chunks[1]->properties;

  GeometryMapperTransformCache::ClearCache();
  invalidator.Generate(kDefaultLayerBounds, new_chunks, layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations(invalidator).IsEmpty());

  // Inserting another node between layer_transform and transform0 and letting
  // the new node become the transform of the layer state should not cause
  // raster invalidation in the layer. This simulates a composited layer is
  // scrolled from its original location.
  CHUNKS(new_chunks1, Chunk(0), Chunk(1));
  auto new_layer_transform = TransformPaintPropertyNode::Create(
      layer_transform, TransformationMatrix().Translate(-100, -200),
      FloatPoint3D());
  layer_state = PropertyTreeState(new_layer_transform.get(),
                                  ClipPaintPropertyNode::Root(),
                                  EffectPaintPropertyNode::Root());
  transform0->Update(new_layer_transform, transform0->Matrix(), FloatPoint3D());
  new_chunks1_array[0].properties = chunks[0]->properties;
  new_chunks1_array[1].properties = chunks[1]->properties;

  GeometryMapperTransformCache::ClearCache();
  invalidator.Generate(kDefaultLayerBounds, new_chunks1, layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations(invalidator).IsEmpty());

  // Removing transform nodes above the layer state should not cause raster
  // invalidation in the layer.
  CHUNKS(new_chunks2, Chunk(0), Chunk(1));
  layer_state = DefaultPropertyTreeState();
  transform0->Update(layer_state.Transform(), transform0->Matrix(),
                     FloatPoint3D());
  new_chunks2_array[0].properties = chunks[0]->properties;
  new_chunks2_array[1].properties = chunks[1]->properties;

  GeometryMapperTransformCache::ClearCache();
  invalidator.Generate(kDefaultLayerBounds, new_chunks2, layer_state);
  EXPECT_TRUE(TrackedRasterInvalidations(invalidator).IsEmpty());

  // Change transform0 and transform1, while keeping the combined transform0
  // and transform1 unchanged for chunk 2. We should invalidate only chunk 0
  // for changed paint property.
  CHUNKS(new_chunks3, Chunk(0), Chunk(1));
  transform0->Update(
      layer_state.Transform(),
      TransformationMatrix(transform0->Matrix()).Translate(20, 30),
      FloatPoint3D());
  transform1->Update(
      transform0,
      TransformationMatrix(transform1->Matrix()).Translate(-20, -30),
      FloatPoint3D());
  new_chunks3_array[0].properties = new_chunks2[0]->properties;
  new_chunks3_array[1].properties = new_chunks2[1]->properties;

  GeometryMapperTransformCache::ClearCache();
  invalidator.Generate(kDefaultLayerBounds, new_chunks3, layer_state);
  const auto& invalidations = TrackedRasterInvalidations(invalidator);
  ASSERT_EQ(2u, invalidations.size());
  EXPECT_CHUNK_INVALIDATION_WITH_LAYER_OFFSET(
      invalidations, 0, *new_chunks3[0],
      PaintInvalidationReason::kPaintProperty,
      -kDefaultLayerBounds.Location() + IntSize(10, 20));
  EXPECT_CHUNK_INVALIDATION_WITH_LAYER_OFFSET(
      invalidations, 1, *new_chunks3[0],
      PaintInvalidationReason::kPaintProperty,
      -kDefaultLayerBounds.Location() + IntSize(30, 50));
  invalidator.SetTracksRasterInvalidations(false);
}

}  // namespace blink

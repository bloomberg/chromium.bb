// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunker.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

using testing::ElementsAre;

namespace blink {

namespace {

class PaintChunkerTest : public testing::Test,
                         private ScopedSlimmingPaintV175ForTest {
 public:
  PaintChunkerTest() : ScopedSlimmingPaintV175ForTest(true) {}

 protected:
  class TestDisplayItemClient : public DisplayItemClient {
    String DebugName() const final { return "Test"; }
    LayoutRect VisualRect() const final { return LayoutRect(); }
  };
  TestDisplayItemClient client_;
};

DisplayItem::Type DisplayItemType(int offset) {
  return static_cast<DisplayItem::Type>(DisplayItem::kDrawingFirst + offset);
}

class TestChunkerDisplayItem : public DisplayItem {
 public:
  TestChunkerDisplayItem(const DisplayItemClient& client,
                         DisplayItem::Type type = DisplayItem::kDrawingFirst)
      : DisplayItem(client, type, sizeof(*this)) {}

  void Replay(GraphicsContext&) const final { NOTREACHED(); }
  void AppendToDisplayItemList(const FloatSize&,
                               cc::DisplayItemList&) const final {
    NOTREACHED();
  }
};

class TestDisplayItemRequiringSeparateChunk : public TestChunkerDisplayItem {
 public:
  TestDisplayItemRequiringSeparateChunk(const DisplayItemClient& client)
      : TestChunkerDisplayItem(client, DisplayItem::kForeignLayerPlugin) {}
};

TEST_F(PaintChunkerTest, Empty) {
  PaintChunker chunker;
  EXPECT_TRUE(chunker.PaintChunks().IsEmpty());

  auto chunks_data = chunker.ReleaseData();
  EXPECT_TRUE(chunks_data.chunks.IsEmpty());
}

TEST_F(PaintChunkerTest, SingleNonEmptyRange) {
  PaintChunker chunker;
  PaintChunk::Id id(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(id, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  const auto& chunks = chunker.PaintChunks();
  EXPECT_EQ(chunks.size(), 1u);
  EXPECT_EQ(chunks[0], PaintChunk(0, 2, id, DefaultPaintChunkProperties()));

  auto chunks_data = chunker.ReleaseData();
  EXPECT_TRUE(chunker.PaintChunks().IsEmpty());
  EXPECT_EQ(chunks_data.chunks.size(), 1u);
  EXPECT_EQ(chunks_data.chunks[0],
            PaintChunk(0, 2, id, DefaultPaintChunkProperties()));
}

TEST_F(PaintChunkerTest, SamePropertiesTwiceCombineIntoOneChunk) {
  PaintChunker chunker;
  PaintChunk::Id id(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(id, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.UpdateCurrentPaintChunkProperties(id, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  const auto& chunks = chunker.PaintChunks();
  EXPECT_EQ(chunks.size(), 1u);
  EXPECT_EQ(chunks[0], PaintChunk(0, 3, id, DefaultPaintChunkProperties()));

  auto chunks_data = chunker.ReleaseData();
  EXPECT_TRUE(chunker.PaintChunks().IsEmpty());
  EXPECT_EQ(chunks_data.chunks.size(), 1u);
  EXPECT_EQ(chunks_data.chunks[0],
            PaintChunk(0, 3, id, DefaultPaintChunkProperties()));
}

TEST_F(PaintChunkerTest, BuildMultipleChunksWithSinglePropertyChanging) {
  PaintChunker chunker;
  PaintChunk::Id id1(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(id1, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  auto simple_transform_node = CreateTransform(
      nullptr, TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));
  auto simple_transform = DefaultPaintChunkProperties();
  simple_transform.SetTransform(simple_transform_node.get());

  PaintChunk::Id id2(client_, DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(id2, simple_transform);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  auto another_transform_node = CreateTransform(
      nullptr, TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));
  auto another_transform = DefaultPaintChunkProperties();
  another_transform.SetTransform(another_transform_node.get());
  PaintChunk::Id id3(client_, DisplayItemType(3));
  chunker.UpdateCurrentPaintChunkProperties(id3, another_transform);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  const auto& chunks = chunker.PaintChunks();
  EXPECT_EQ(chunks.size(), 3u);
  EXPECT_EQ(chunks[0], PaintChunk(0, 2, id1, DefaultPaintChunkProperties()));
  EXPECT_EQ(chunks[1], PaintChunk(2, 3, id2, simple_transform));
  EXPECT_EQ(chunks[2], PaintChunk(3, 4, id3, another_transform));
}

TEST_F(PaintChunkerTest, BuildMultipleChunksWithDifferentPropertyChanges) {
  PaintChunker chunker;
  PaintChunk::Id id1(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(id1, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  auto simple_transform_node = CreateTransform(
      nullptr, TransformationMatrix(0, 0, 0, 0, 0, 0), FloatPoint3D(9, 8, 7));
  auto simple_transform = DefaultPaintChunkProperties();
  simple_transform.SetTransform(simple_transform_node.get());
  PaintChunk::Id id2(client_, DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(id2, simple_transform);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  auto simple_effect_node =
      CreateOpacityEffect(EffectPaintPropertyNode::Root(), 0.5f);
  auto simple_transform_and_effect = DefaultPaintChunkProperties();
  simple_transform_and_effect.SetTransform(simple_transform_node.get());
  simple_transform_and_effect.SetEffect(simple_effect_node.get());
  PaintChunk::Id id3(client_, DisplayItemType(3));
  chunker.UpdateCurrentPaintChunkProperties(id3, simple_transform_and_effect);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  auto new_transform_node = CreateTransform(
      nullptr, TransformationMatrix(1, 1, 0, 0, 0, 0), FloatPoint3D(9, 8, 7));
  auto simple_transform_and_effect_with_updated_transform =
      DefaultPaintChunkProperties();
  auto new_effect_node =
      CreateOpacityEffect(EffectPaintPropertyNode::Root(), 0.5f);
  simple_transform_and_effect_with_updated_transform.SetTransform(
      new_transform_node.get());
  simple_transform_and_effect_with_updated_transform.SetEffect(
      new_effect_node.get());
  PaintChunk::Id id4(client_, DisplayItemType(4));
  chunker.UpdateCurrentPaintChunkProperties(
      id4, simple_transform_and_effect_with_updated_transform);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  // Test that going back to a previous chunk property still creates a new
  // chunk.
  chunker.UpdateCurrentPaintChunkProperties(base::nullopt,
                                            simple_transform_and_effect);
  TestChunkerDisplayItem item_after_restore(client_, DisplayItemType(10));
  chunker.IncrementDisplayItemIndex(item_after_restore);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  const auto& chunks = chunker.PaintChunks();
  EXPECT_EQ(chunks.size(), 5u);
  EXPECT_EQ(chunks[0], PaintChunk(0, 1, id1, DefaultPaintChunkProperties()));
  EXPECT_EQ(chunks[1], PaintChunk(1, 3, id2, simple_transform));
  EXPECT_EQ(chunks[2], PaintChunk(3, 5, id3, simple_transform_and_effect));
  EXPECT_EQ(chunks[3],
            PaintChunk(5, 7, id4,
                       simple_transform_and_effect_with_updated_transform));
  EXPECT_EQ(chunks[4], PaintChunk(7, 9, item_after_restore.GetId(),
                                  simple_transform_and_effect));
}

TEST_F(PaintChunkerTest, BuildChunksFromNestedTransforms) {
  // Test that "nested" transforms linearize using the following
  // sequence of transforms and display items:
  // <root xform>
  //   <paint>
  //   <a xform>
  //     <paint><paint>
  //   </a xform>
  //   <paint>
  // </root xform>
  PaintChunker chunker;
  PaintChunk::Id id1(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(id1, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  auto simple_transform_node = CreateTransform(
      nullptr, TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));
  auto simple_transform = DefaultPaintChunkProperties();
  simple_transform.SetTransform(simple_transform_node.get());
  PaintChunk::Id id2(client_, DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(id2, simple_transform);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  chunker.UpdateCurrentPaintChunkProperties(base::nullopt,
                                            DefaultPaintChunkProperties());
  TestChunkerDisplayItem item_after_restore(client_, DisplayItemType(10));
  chunker.IncrementDisplayItemIndex(item_after_restore);

  const auto& chunks = chunker.PaintChunks();
  EXPECT_EQ(chunks.size(), 3u);
  EXPECT_EQ(chunks[0], PaintChunk(0, 1, id1, DefaultPaintChunkProperties()));
  EXPECT_EQ(chunks[1], PaintChunk(1, 3, id2, simple_transform));
  EXPECT_EQ(chunks[2], PaintChunk(3, 4, item_after_restore.GetId(),
                                  DefaultPaintChunkProperties()));
}

TEST_F(PaintChunkerTest, ChangingPropertiesWithoutItems) {
  // Test that properties can change without display items being generated.
  PaintChunker chunker;
  PaintChunk::Id id1(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(id1, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  auto first_transform_node = CreateTransform(
      nullptr, TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));
  auto first_transform = DefaultPaintChunkProperties();
  first_transform.SetTransform(first_transform_node.get());
  PaintChunk::Id id2(client_, DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(base::nullopt, first_transform);

  auto second_transform_node = CreateTransform(
      nullptr, TransformationMatrix(9, 8, 7, 6, 5, 4), FloatPoint3D(3, 2, 1));
  auto second_transform = DefaultPaintChunkProperties();
  second_transform.SetTransform(second_transform_node.get());
  PaintChunk::Id id3(client_, DisplayItemType(3));
  chunker.UpdateCurrentPaintChunkProperties(id3, second_transform);

  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  const auto& chunks = chunker.PaintChunks();
  EXPECT_EQ(chunks.size(), 2u);
  EXPECT_EQ(chunks[0], PaintChunk(0, 1, id1, DefaultPaintChunkProperties()));
  EXPECT_EQ(chunks[1], PaintChunk(1, 2, id3, second_transform));
}

TEST_F(PaintChunkerTest, CreatesSeparateChunksWhenRequested) {
  // Tests that the chunker creates a separate chunks for display items which
  // require it.
  PaintChunker chunker;
  TestDisplayItemClient client1;
  TestDisplayItemRequiringSeparateChunk i1(client1);
  TestDisplayItemClient client2;
  TestDisplayItemRequiringSeparateChunk i2(client2);
  TestDisplayItemClient client3;
  TestDisplayItemRequiringSeparateChunk i3(client3);

  PaintChunk::Id id0(client_, DisplayItemType(0));
  chunker.UpdateCurrentPaintChunkProperties(id0, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(i1);
  chunker.IncrementDisplayItemIndex(i2);
  TestChunkerDisplayItem after_i2(client_, DisplayItemType(10));
  chunker.IncrementDisplayItemIndex(after_i2);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(i3);

  const auto& chunks = chunker.PaintChunks();
  EXPECT_EQ(chunks.size(), 5u);
  EXPECT_EQ(chunks[0], PaintChunk(0, 1, id0, DefaultPaintChunkProperties()));
  EXPECT_EQ(chunks[1],
            PaintChunk(1, 2, i1.GetId(), DefaultPaintChunkProperties()));
  EXPECT_EQ(chunks[2],
            PaintChunk(2, 3, i2.GetId(), DefaultPaintChunkProperties()));
  EXPECT_EQ(chunks[3],
            PaintChunk(3, 5, after_i2.GetId(), DefaultPaintChunkProperties()));
  EXPECT_EQ(chunks[4],
            PaintChunk(5, 6, i3.GetId(), DefaultPaintChunkProperties()));
}

TEST_F(PaintChunkerTest, ForceNewChunkWithNewId) {
  PaintChunker chunker;
  PaintChunk::Id id0(client_, DisplayItemType(0));
  chunker.UpdateCurrentPaintChunkProperties(id0, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  chunker.ForceNewChunk();
  PaintChunk::Id id1(client_, DisplayItemType(10));
  chunker.UpdateCurrentPaintChunkProperties(id1, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  chunker.ForceNewChunk();
  PaintChunk::Id id2(client_, DisplayItemType(20));
  chunker.UpdateCurrentPaintChunkProperties(id2, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  const auto& chunks = chunker.PaintChunks();
  EXPECT_EQ(chunks.size(), 3u);
  EXPECT_EQ(chunks[0], PaintChunk(0, 2, id0, DefaultPaintChunkProperties()));
  EXPECT_EQ(chunks[1], PaintChunk(2, 4, id1, DefaultPaintChunkProperties()));
  EXPECT_EQ(chunks[2], PaintChunk(4, 6, id2, DefaultPaintChunkProperties()));
}

TEST_F(PaintChunkerTest, ForceNewChunkWithoutNewId) {
  PaintChunker chunker;
  PaintChunk::Id id0(client_, DisplayItemType(0));
  chunker.UpdateCurrentPaintChunkProperties(base::nullopt,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(
      TestChunkerDisplayItem(id0.client, id0.type));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  chunker.ForceNewChunk();
  PaintChunk::Id id1(client_, DisplayItemType(10));
  chunker.IncrementDisplayItemIndex(
      TestChunkerDisplayItem(id1.client, id1.type));
  chunker.IncrementDisplayItemIndex(
      TestChunkerDisplayItem(client_, DisplayItemType(11)));

  chunker.ForceNewChunk();
  PaintChunk::Id id2(client_, DisplayItemType(20));
  chunker.IncrementDisplayItemIndex(
      TestChunkerDisplayItem(id2.client, id2.type));
  chunker.IncrementDisplayItemIndex(
      TestChunkerDisplayItem(client_, DisplayItemType(21)));

  const auto& chunks = chunker.PaintChunks();
  EXPECT_EQ(chunks.size(), 3u);
  EXPECT_EQ(chunks[0], PaintChunk(0, 2, id0, DefaultPaintChunkProperties()));
  EXPECT_EQ(chunks[1], PaintChunk(2, 4, id1, DefaultPaintChunkProperties()));
  EXPECT_EQ(chunks[2], PaintChunk(4, 6, id2, DefaultPaintChunkProperties()));
}

TEST_F(PaintChunkerTest, NoNewChunkForSamePropertyDifferentIds) {
  PaintChunker chunker;
  PaintChunk::Id id0(client_, DisplayItemType(0));
  chunker.UpdateCurrentPaintChunkProperties(id0, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  PaintChunk::Id id1(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(id1, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  chunker.UpdateCurrentPaintChunkProperties(base::nullopt,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  const auto& chunks = chunker.PaintChunks();
  EXPECT_EQ(chunks.size(), 1u);
  EXPECT_EQ(chunks[0], PaintChunk(0, 6, id0, DefaultPaintChunkProperties()));
}

class TestScrollHitTestRequiringSeparateChunk : public TestChunkerDisplayItem {
 public:
  TestScrollHitTestRequiringSeparateChunk(const DisplayItemClient& client)
      : TestChunkerDisplayItem(client, DisplayItem::kScrollHitTest) {}
};

// Ensure that items following a forced chunk begin using the next display
// item's id.
TEST_F(PaintChunkerTest, ChunksFollowingForcedChunk) {
  PaintChunker chunker;
  TestDisplayItemClient client;
  TestChunkerDisplayItem before_forced1(client, DisplayItemType(9));
  TestChunkerDisplayItem before_forced2(client, DisplayItemType(10));
  TestScrollHitTestRequiringSeparateChunk forced(client);
  TestChunkerDisplayItem after_forced1(client, DisplayItemType(11));
  TestChunkerDisplayItem after_forced2(client, DisplayItemType(12));

  PaintChunk::Id id0(client, DisplayItemType(8));
  chunker.UpdateCurrentPaintChunkProperties(id0, DefaultPaintChunkProperties());
  // Both before_forced items should be in a chunk together.
  chunker.IncrementDisplayItemIndex(before_forced1);
  chunker.IncrementDisplayItemIndex(before_forced2);
  // The forced scroll hit test item should be in its own chunk.
  chunker.IncrementDisplayItemIndex(forced);
  // Both after_forced items should be in a chunk together.
  chunker.IncrementDisplayItemIndex(after_forced1);
  chunker.IncrementDisplayItemIndex(after_forced2);

  const auto& chunks = chunker.PaintChunks();
  EXPECT_EQ(chunks.size(), 3u);
  EXPECT_EQ(chunks[0], PaintChunk(0, 2, id0, DefaultPaintChunkProperties()));
  EXPECT_EQ(chunks[1],
            PaintChunk(2, 3, forced.GetId(), DefaultPaintChunkProperties()));
  EXPECT_EQ(chunks[2], PaintChunk(3, 5, after_forced1.GetId(),
                                  DefaultPaintChunkProperties()));
}

TEST_F(PaintChunkerTest, ChunkIdsSkippingCache) {
  PaintChunker chunker;

  PaintChunk::Id id1(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(id1, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  auto simple_transform_node = CreateTransform(
      nullptr, TransformationMatrix(0, 1, 2, 3, 4, 5), FloatPoint3D(9, 8, 7));
  auto simple_transform = DefaultPaintChunkProperties();
  simple_transform.SetTransform(simple_transform_node.get());
  PaintChunk::Id id2(client_, DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(id2, simple_transform);

  TestChunkerDisplayItem uncacheable_item(client_);
  uncacheable_item.SetSkippedCache();
  chunker.IncrementDisplayItemIndex(uncacheable_item);
  chunker.IncrementDisplayItemIndex(TestChunkerDisplayItem(client_));

  TestDisplayItemRequiringSeparateChunk uncacheable_separate_chunk_item(
      client_);
  uncacheable_separate_chunk_item.SetSkippedCache();
  chunker.IncrementDisplayItemIndex(uncacheable_separate_chunk_item);

  TestChunkerDisplayItem after_separate_chunk(client_, DisplayItemType(3));
  chunker.IncrementDisplayItemIndex(after_separate_chunk);

  chunker.UpdateCurrentPaintChunkProperties(base::nullopt,
                                            DefaultPaintChunkProperties());
  TestChunkerDisplayItem after_restore(client_, DisplayItemType(4));
  chunker.IncrementDisplayItemIndex(after_restore);

  const auto& chunks = chunker.PaintChunks();
  EXPECT_EQ(chunks.size(), 5u);
  EXPECT_EQ(chunks[0], PaintChunk(0, 2, id1, DefaultPaintChunkProperties()));
  EXPECT_EQ(chunks[1],
            PaintChunk(2, 4, id2, simple_transform, PaintChunk::kUncacheable));
  EXPECT_EQ(chunks[2], PaintChunk(4, 5, uncacheable_separate_chunk_item.GetId(),
                                  simple_transform, PaintChunk::kUncacheable));
  EXPECT_EQ(chunks[3],
            PaintChunk(5, 6, after_separate_chunk.GetId(), simple_transform));
  EXPECT_EQ(chunks[4], PaintChunk(6, 7, after_restore.GetId(),
                                  DefaultPaintChunkProperties()));
}

}  // namespace
}  // namespace blink

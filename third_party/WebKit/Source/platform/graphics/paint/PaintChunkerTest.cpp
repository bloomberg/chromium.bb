// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintChunker.h"

#include "platform/testing/PaintPropertyTestHelpers.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::blink::testing::CreateOpacityOnlyEffect;
using ::blink::testing::DefaultPaintChunkProperties;
using ::testing::ElementsAre;

namespace blink {
namespace {

class PaintChunkerTest : public ::testing::Test,
                         private ScopedSlimmingPaintV2ForTest {
 public:
  PaintChunkerTest() : ScopedSlimmingPaintV2ForTest(true) {}

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

class TestDisplayItem : public DisplayItem {
 public:
  TestDisplayItem(const DisplayItemClient& client,
                  DisplayItem::Type type = DisplayItem::kDrawingFirst)
      : DisplayItem(client, type, sizeof(*this)) {}

  void Replay(GraphicsContext&) const final { NOTREACHED(); }
  void AppendToWebDisplayItemList(const LayoutSize&,
                                  WebDisplayItemList*) const final {
    NOTREACHED();
  }
};

class TestDisplayItemRequiringSeparateChunk : public TestDisplayItem {
 public:
  TestDisplayItemRequiringSeparateChunk(const DisplayItemClient& client)
      : TestDisplayItem(client, DisplayItem::kForeignLayerPlugin) {}
};

TEST_F(PaintChunkerTest, Empty) {
  Vector<PaintChunk> chunks = PaintChunker().ReleasePaintChunks();
  ASSERT_TRUE(chunks.IsEmpty());
}

TEST_F(PaintChunkerTest, SingleNonEmptyRange) {
  PaintChunker chunker;
  PaintChunk::Id id(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(&id, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));
  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();

  EXPECT_THAT(chunks,
              ElementsAre(PaintChunk(0, 2, id, DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, SamePropertiesTwiceCombineIntoOneChunk) {
  PaintChunker chunker;
  PaintChunk::Id id(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(&id, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));
  chunker.UpdateCurrentPaintChunkProperties(&id, DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));
  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();

  EXPECT_THAT(chunks,
              ElementsAre(PaintChunk(0, 3, id, DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, BuildMultipleChunksWithSinglePropertyChanging) {
  PaintChunker chunker;
  PaintChunk::Id id1(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(&id1,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));

  PaintChunkProperties simple_transform = DefaultPaintChunkProperties();
  simple_transform.property_tree_state.SetTransform(
      TransformPaintPropertyNode::Create(nullptr,
                                         TransformationMatrix(0, 1, 2, 3, 4, 5),
                                         FloatPoint3D(9, 8, 7))
          .Get());

  PaintChunk::Id id2(client_, DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(&id2, simple_transform);
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));

  PaintChunkProperties another_transform = DefaultPaintChunkProperties();
  another_transform.property_tree_state.SetTransform(
      TransformPaintPropertyNode::Create(nullptr,
                                         TransformationMatrix(0, 1, 2, 3, 4, 5),
                                         FloatPoint3D(9, 8, 7))
          .Get());
  PaintChunk::Id id3(client_, DisplayItemType(3));
  chunker.UpdateCurrentPaintChunkProperties(&id3, another_transform);
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));

  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();

  EXPECT_THAT(chunks,
              ElementsAre(PaintChunk(0, 2, id1, DefaultPaintChunkProperties()),
                          PaintChunk(2, 3, id2, simple_transform),
                          PaintChunk(3, 4, id3, another_transform)));
}

TEST_F(PaintChunkerTest, BuildMultipleChunksWithDifferentPropertyChanges) {
  PaintChunker chunker;
  PaintChunk::Id id1(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(&id1,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));

  PaintChunkProperties simple_transform = DefaultPaintChunkProperties();
  simple_transform.property_tree_state.SetTransform(
      TransformPaintPropertyNode::Create(nullptr,
                                         TransformationMatrix(0, 0, 0, 0, 0, 0),
                                         FloatPoint3D(9, 8, 7))
          .Get());
  PaintChunk::Id id2(client_, DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(&id2, simple_transform);
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));

  PaintChunkProperties simple_transform_and_effect =
      DefaultPaintChunkProperties();
  simple_transform_and_effect.property_tree_state.SetTransform(
      simple_transform.property_tree_state.Transform());
  simple_transform_and_effect.property_tree_state.SetEffect(
      CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.5f).Get());
  PaintChunk::Id id3(client_, DisplayItemType(3));
  chunker.UpdateCurrentPaintChunkProperties(&id3, simple_transform_and_effect);
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));

  PaintChunkProperties simple_transform_and_effect_with_updated_transform =
      DefaultPaintChunkProperties();
  simple_transform_and_effect_with_updated_transform.property_tree_state
      .SetTransform(TransformPaintPropertyNode::Create(
                        nullptr, TransformationMatrix(1, 1, 0, 0, 0, 0),
                        FloatPoint3D(9, 8, 7))
                        .Get());
  simple_transform_and_effect_with_updated_transform.property_tree_state
      .SetEffect(CreateOpacityOnlyEffect(
                     EffectPaintPropertyNode::Root(),
                     simple_transform_and_effect.property_tree_state.Effect()
                         ->Opacity())
                     .Get());
  PaintChunk::Id id4(client_, DisplayItemType(4));
  chunker.UpdateCurrentPaintChunkProperties(
      &id4, simple_transform_and_effect_with_updated_transform);
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));

  // Test that going back to a previous chunk property still creates a new
  // chunk.
  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            simple_transform_and_effect);
  TestDisplayItem item_after_restore(client_, DisplayItemType(10));
  chunker.IncrementDisplayItemIndex(item_after_restore);
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));

  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();

  EXPECT_THAT(
      chunks,
      ElementsAre(
          PaintChunk(0, 1, id1, DefaultPaintChunkProperties()),
          PaintChunk(1, 3, id2, simple_transform),
          PaintChunk(3, 5, id3, simple_transform_and_effect),
          PaintChunk(5, 7, id4,
                     simple_transform_and_effect_with_updated_transform),
          PaintChunk(7, 9, item_after_restore.GetId(),
                     simple_transform_and_effect)));
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
  chunker.UpdateCurrentPaintChunkProperties(&id1,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));

  PaintChunkProperties simple_transform = DefaultPaintChunkProperties();
  simple_transform.property_tree_state.SetTransform(
      TransformPaintPropertyNode::Create(nullptr,
                                         TransformationMatrix(0, 1, 2, 3, 4, 5),
                                         FloatPoint3D(9, 8, 7))
          .Get());
  PaintChunk::Id id2(client_, DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(&id2, simple_transform);
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));

  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  TestDisplayItem item_after_restore(client_, DisplayItemType(10));
  chunker.IncrementDisplayItemIndex(item_after_restore);

  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();

  EXPECT_THAT(chunks,
              ElementsAre(PaintChunk(0, 1, id1, DefaultPaintChunkProperties()),
                          PaintChunk(1, 3, id2, simple_transform),
                          PaintChunk(3, 4, item_after_restore.GetId(),
                                     DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ChangingPropertiesWithoutItems) {
  // Test that properties can change without display items being generated.
  PaintChunker chunker;
  PaintChunk::Id id1(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(&id1,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));

  PaintChunkProperties first_transform = DefaultPaintChunkProperties();
  first_transform.property_tree_state.SetTransform(
      TransformPaintPropertyNode::Create(nullptr,
                                         TransformationMatrix(0, 1, 2, 3, 4, 5),
                                         FloatPoint3D(9, 8, 7))
          .Get());
  PaintChunk::Id id2(client_, DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(nullptr, first_transform);

  PaintChunkProperties second_transform = DefaultPaintChunkProperties();
  second_transform.property_tree_state.SetTransform(
      TransformPaintPropertyNode::Create(nullptr,
                                         TransformationMatrix(9, 8, 7, 6, 5, 4),
                                         FloatPoint3D(3, 2, 1))
          .Get());
  PaintChunk::Id id3(client_, DisplayItemType(3));
  chunker.UpdateCurrentPaintChunkProperties(&id3, second_transform);

  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));
  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();

  EXPECT_THAT(chunks,
              ElementsAre(PaintChunk(0, 1, id1, DefaultPaintChunkProperties()),
                          PaintChunk(1, 2, id3, second_transform)));
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
  chunker.UpdateCurrentPaintChunkProperties(&id0,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(i1);
  chunker.IncrementDisplayItemIndex(i2);
  TestDisplayItem after_i2(client_, DisplayItemType(10));
  chunker.IncrementDisplayItemIndex(after_i2);
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(i3);

  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();
  EXPECT_THAT(
      chunks,
      ElementsAre(
          PaintChunk(0, 1, id0, DefaultPaintChunkProperties()),
          PaintChunk(1, 2, i1.GetId(), DefaultPaintChunkProperties()),
          PaintChunk(2, 3, i2.GetId(), DefaultPaintChunkProperties()),
          PaintChunk(3, 5, after_i2.GetId(), DefaultPaintChunkProperties()),
          PaintChunk(5, 6, i3.GetId(), DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ForceNewChunk) {
  PaintChunker chunker;
  PaintChunk::Id id0(client_, DisplayItemType(0));
  chunker.UpdateCurrentPaintChunkProperties(&id0,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));

  chunker.ForceNewChunk();
  PaintChunk::Id id1(client_, DisplayItemType(10));
  chunker.UpdateCurrentPaintChunkProperties(&id1,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));

  PaintChunk::Id id2(client_, DisplayItemType(20));
  chunker.UpdateCurrentPaintChunkProperties(&id2,
                                            DefaultPaintChunkProperties());
  chunker.ForceNewChunk();
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));

  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();
  EXPECT_THAT(
      chunks,
      ElementsAre(PaintChunk(0, 2, id0, DefaultPaintChunkProperties()),
                  PaintChunk(2, 4, id1, DefaultPaintChunkProperties()),
                  PaintChunk(4, 6, id2, DefaultPaintChunkProperties())));
}

class TestScrollHitTestRequiringSeparateChunk : public TestDisplayItem {
 public:
  TestScrollHitTestRequiringSeparateChunk(const DisplayItemClient& client)
      : TestDisplayItem(client, DisplayItem::kScrollHitTest) {}
};

// Ensure that items following a forced chunk begin using the next display
// item's id.
TEST_F(PaintChunkerTest, ChunksFollowingForcedChunk) {
  PaintChunker chunker;
  TestDisplayItemClient client;
  TestDisplayItem before_forced1(client, DisplayItemType(9));
  TestDisplayItem before_forced2(client, DisplayItemType(10));
  TestScrollHitTestRequiringSeparateChunk forced(client);
  TestDisplayItem after_forced1(client, DisplayItemType(11));
  TestDisplayItem after_forced2(client, DisplayItemType(12));

  PaintChunk::Id id0(client, DisplayItemType(8));
  chunker.UpdateCurrentPaintChunkProperties(&id0,
                                            DefaultPaintChunkProperties());
  // Both before_forced items should be in a chunk together.
  chunker.IncrementDisplayItemIndex(before_forced1);
  chunker.IncrementDisplayItemIndex(before_forced2);
  // The forced scroll hit test item should be in its own chunk.
  chunker.IncrementDisplayItemIndex(forced);
  // Both after_forced items should be in a chunk together.
  chunker.IncrementDisplayItemIndex(after_forced1);
  chunker.IncrementDisplayItemIndex(after_forced2);

  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();
  EXPECT_THAT(chunks,
              ElementsAre(PaintChunk(0, 2, id0, DefaultPaintChunkProperties()),
                          PaintChunk(2, 3, forced.GetId(),
                                     DefaultPaintChunkProperties()),
                          PaintChunk(3, 5, after_forced1.GetId(),
                                     DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ChunkIdsSkippingCache) {
  PaintChunker chunker;

  PaintChunk::Id id1(client_, DisplayItemType(1));
  chunker.UpdateCurrentPaintChunkProperties(&id1,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));

  PaintChunkProperties simple_transform = DefaultPaintChunkProperties();
  simple_transform.property_tree_state.SetTransform(
      TransformPaintPropertyNode::Create(nullptr,
                                         TransformationMatrix(0, 1, 2, 3, 4, 5),
                                         FloatPoint3D(9, 8, 7))
          .Get());
  PaintChunk::Id id2(client_, DisplayItemType(2));
  chunker.UpdateCurrentPaintChunkProperties(&id2, simple_transform);

  TestDisplayItem uncacheable_item(client_);
  uncacheable_item.SetSkippedCache();
  chunker.IncrementDisplayItemIndex(uncacheable_item);
  chunker.IncrementDisplayItemIndex(TestDisplayItem(client_));

  TestDisplayItemRequiringSeparateChunk uncacheable_separate_chunk_item(
      client_);
  uncacheable_separate_chunk_item.SetSkippedCache();
  chunker.IncrementDisplayItemIndex(uncacheable_separate_chunk_item);

  TestDisplayItem after_separate_chunk(client_, DisplayItemType(3));
  chunker.IncrementDisplayItemIndex(after_separate_chunk);

  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  TestDisplayItem after_restore(client_, DisplayItemType(4));
  chunker.IncrementDisplayItemIndex(after_restore);

  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();
  EXPECT_THAT(
      chunks,
      ElementsAre(
          PaintChunk(0, 2, id1, DefaultPaintChunkProperties()),
          PaintChunk(2, 4, id2, simple_transform, PaintChunk::kUncacheable),
          PaintChunk(4, 5, uncacheable_separate_chunk_item.GetId(),
                     simple_transform, PaintChunk::kUncacheable),
          PaintChunk(5, 6, after_separate_chunk.GetId(), simple_transform),
          PaintChunk(6, 7, after_restore.GetId(),
                     DefaultPaintChunkProperties())));
}

}  // namespace
}  // namespace blink

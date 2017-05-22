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

class TestDisplayItem : public DisplayItem {
 public:
  TestDisplayItem(const DisplayItemClient& client, DisplayItem::Type type)
      : DisplayItem(client, type, sizeof(*this)) {}

  void Replay(GraphicsContext&) const final { NOTREACHED(); }
  void AppendToWebDisplayItemList(const IntRect&,
                                  WebDisplayItemList*) const final {
    NOTREACHED();
  }
};

class NormalTestDisplayItem : public TestDisplayItem {
 public:
  NormalTestDisplayItem(const DisplayItemClient& client)
      : TestDisplayItem(client, DisplayItem::kDrawingFirst) {}
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
  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();

  EXPECT_THAT(chunks, ElementsAre(PaintChunk(0, 2, nullptr,
                                             DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, SamePropertiesTwiceCombineIntoOneChunk) {
  PaintChunker chunker;
  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();

  EXPECT_THAT(chunks, ElementsAre(PaintChunk(0, 3, nullptr,
                                             DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, CanRewindDisplayItemIndex) {
  PaintChunker chunker;
  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  chunker.DecrementDisplayItemIndex();
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();

  EXPECT_THAT(chunks, ElementsAre(PaintChunk(0, 2, nullptr,
                                             DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, BuildMultipleChunksWithSinglePropertyChanging) {
  PaintChunker chunker;
  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));

  PaintChunkProperties simple_transform = DefaultPaintChunkProperties();
  simple_transform.property_tree_state.SetTransform(
      TransformPaintPropertyNode::Create(nullptr,
                                         TransformationMatrix(0, 1, 2, 3, 4, 5),
                                         FloatPoint3D(9, 8, 7))
          .Get());

  chunker.UpdateCurrentPaintChunkProperties(nullptr, simple_transform);
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));

  PaintChunkProperties another_transform = DefaultPaintChunkProperties();
  another_transform.property_tree_state.SetTransform(
      TransformPaintPropertyNode::Create(nullptr,
                                         TransformationMatrix(0, 1, 2, 3, 4, 5),
                                         FloatPoint3D(9, 8, 7))
          .Get());
  chunker.UpdateCurrentPaintChunkProperties(nullptr, another_transform);
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));

  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();

  EXPECT_THAT(
      chunks,
      ElementsAre(PaintChunk(0, 2, nullptr, DefaultPaintChunkProperties()),
                  PaintChunk(2, 3, nullptr, simple_transform),
                  PaintChunk(3, 4, nullptr, another_transform)));
}

TEST_F(PaintChunkerTest, BuildMultipleChunksWithDifferentPropertyChanges) {
  PaintChunker chunker;
  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));

  PaintChunkProperties simple_transform = DefaultPaintChunkProperties();
  simple_transform.property_tree_state.SetTransform(
      TransformPaintPropertyNode::Create(nullptr,
                                         TransformationMatrix(0, 0, 0, 0, 0, 0),
                                         FloatPoint3D(9, 8, 7))
          .Get());
  chunker.UpdateCurrentPaintChunkProperties(nullptr, simple_transform);
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));

  PaintChunkProperties simple_transform_and_effect =
      DefaultPaintChunkProperties();
  simple_transform_and_effect.property_tree_state.SetTransform(
      simple_transform.property_tree_state.Transform());
  simple_transform_and_effect.property_tree_state.SetEffect(
      CreateOpacityOnlyEffect(EffectPaintPropertyNode::Root(), 0.5f).Get());
  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            simple_transform_and_effect);
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));

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
  chunker.UpdateCurrentPaintChunkProperties(
      nullptr, simple_transform_and_effect_with_updated_transform);
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));

  // Test that going back to a previous chunk property still creates a new
  // chunk.
  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            simple_transform_and_effect);
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));

  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();

  EXPECT_THAT(
      chunks,
      ElementsAre(
          PaintChunk(0, 1, nullptr, DefaultPaintChunkProperties()),
          PaintChunk(1, 3, nullptr, simple_transform),
          PaintChunk(3, 5, nullptr, simple_transform_and_effect),
          PaintChunk(5, 7, nullptr,
                     simple_transform_and_effect_with_updated_transform),
          PaintChunk(7, 9, nullptr, simple_transform_and_effect)));
}

TEST_F(PaintChunkerTest, BuildChunksFromNestedTransforms) {
  // Test that "nested" transforms linearize using the following
  // sequence of transforms and display items:
  // <root xform>, <paint>, <a xform>, <paint>, <paint>, </a xform>, <paint>,
  // </root xform>
  PaintChunker chunker;
  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));

  PaintChunkProperties simple_transform = DefaultPaintChunkProperties();
  simple_transform.property_tree_state.SetTransform(
      TransformPaintPropertyNode::Create(nullptr,
                                         TransformationMatrix(0, 1, 2, 3, 4, 5),
                                         FloatPoint3D(9, 8, 7))
          .Get());
  chunker.UpdateCurrentPaintChunkProperties(nullptr, simple_transform);
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));

  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));

  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();

  EXPECT_THAT(
      chunks,
      ElementsAre(PaintChunk(0, 1, nullptr, DefaultPaintChunkProperties()),
                  PaintChunk(1, 3, nullptr, simple_transform),
                  PaintChunk(3, 4, nullptr, DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ChangingPropertiesWithoutItems) {
  // Test that properties can change without display items being generated.
  PaintChunker chunker;
  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));

  PaintChunkProperties first_transform = DefaultPaintChunkProperties();
  first_transform.property_tree_state.SetTransform(
      TransformPaintPropertyNode::Create(nullptr,
                                         TransformationMatrix(0, 1, 2, 3, 4, 5),
                                         FloatPoint3D(9, 8, 7))
          .Get());
  chunker.UpdateCurrentPaintChunkProperties(nullptr, first_transform);

  PaintChunkProperties second_transform = DefaultPaintChunkProperties();
  second_transform.property_tree_state.SetTransform(
      TransformPaintPropertyNode::Create(nullptr,
                                         TransformationMatrix(9, 8, 7, 6, 5, 4),
                                         FloatPoint3D(3, 2, 1))
          .Get());
  chunker.UpdateCurrentPaintChunkProperties(nullptr, second_transform);

  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();

  EXPECT_THAT(chunks, ElementsAre(PaintChunk(0, 1, nullptr,
                                             DefaultPaintChunkProperties()),
                                  PaintChunk(1, 2, nullptr, second_transform)));
}

TEST_F(PaintChunkerTest, CreatesSeparateChunksWhenRequested) {
  // Tests that the chunker creates a separate chunks for display items which
  // require it.
  PaintChunker chunker;
  TestDisplayItemRequiringSeparateChunk i1(client_);
  TestDisplayItemRequiringSeparateChunk i2(client_);
  TestDisplayItemRequiringSeparateChunk i3(client_);
  TestDisplayItemRequiringSeparateChunk i4(client_);
  TestDisplayItemRequiringSeparateChunk i5(client_);
  TestDisplayItemRequiringSeparateChunk i6(client_);

  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(i1);
  chunker.IncrementDisplayItemIndex(i2);
  chunker.IncrementDisplayItemIndex(i3);
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(i4);
  chunker.IncrementDisplayItemIndex(i5);
  chunker.DecrementDisplayItemIndex();
  chunker.DecrementDisplayItemIndex();
  chunker.DecrementDisplayItemIndex();
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(i6);

  DisplayItem::Id id1 = i1.GetId();
  DisplayItem::Id id2 = i2.GetId();
  DisplayItem::Id id3 = i3.GetId();
  DisplayItem::Id id6 = i6.GetId();
  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();
  EXPECT_THAT(
      chunks,
      ElementsAre(PaintChunk(0, 1, nullptr, DefaultPaintChunkProperties()),
                  PaintChunk(1, 2, &id1, DefaultPaintChunkProperties()),
                  PaintChunk(2, 3, &id2, DefaultPaintChunkProperties()),
                  PaintChunk(3, 4, &id3, DefaultPaintChunkProperties()),
                  PaintChunk(4, 6, nullptr, DefaultPaintChunkProperties()),
                  PaintChunk(6, 7, &id6, DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ChunkIds) {
  PaintChunker chunker;
  TestDisplayItem i1(client_, DisplayItem::kDrawingFirst);
  DisplayItem::Id id1 = i1.GetId();
  TestDisplayItemRequiringSeparateChunk i2(client_);
  DisplayItem::Id id2 = i2.GetId();

  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));

  PaintChunkProperties simple_transform = DefaultPaintChunkProperties();
  simple_transform.property_tree_state.SetTransform(
      TransformPaintPropertyNode::Create(nullptr,
                                         TransformationMatrix(0, 1, 2, 3, 4, 5),
                                         FloatPoint3D(9, 8, 7))
          .Get());
  chunker.UpdateCurrentPaintChunkProperties(&id1, simple_transform);

  chunker.IncrementDisplayItemIndex(i1);
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(i2);
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));

  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));

  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();
  EXPECT_THAT(
      chunks,
      ElementsAre(PaintChunk(0, 2, nullptr, DefaultPaintChunkProperties()),
                  PaintChunk(2, 4, &id1, simple_transform),
                  PaintChunk(4, 5, &id2, simple_transform),
                  PaintChunk(5, 6, nullptr, simple_transform),
                  PaintChunk(6, 7, nullptr, DefaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ChunkIdsSkippingCache) {
  PaintChunker chunker;
  TestDisplayItem i1(client_, DisplayItem::kDrawingFirst);
  i1.SetSkippedCache();
  DisplayItem::Id id1 = i1.GetId();
  TestDisplayItemRequiringSeparateChunk i2(client_);
  i2.SetSkippedCache();

  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));

  PaintChunkProperties simple_transform = DefaultPaintChunkProperties();
  simple_transform.property_tree_state.SetTransform(
      TransformPaintPropertyNode::Create(nullptr,
                                         TransformationMatrix(0, 1, 2, 3, 4, 5),
                                         FloatPoint3D(9, 8, 7))
          .Get());
  chunker.UpdateCurrentPaintChunkProperties(&id1, simple_transform);

  chunker.IncrementDisplayItemIndex(i1);
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));
  chunker.IncrementDisplayItemIndex(i2);
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));

  chunker.UpdateCurrentPaintChunkProperties(nullptr,
                                            DefaultPaintChunkProperties());
  chunker.IncrementDisplayItemIndex(NormalTestDisplayItem(client_));

  Vector<PaintChunk> chunks = chunker.ReleasePaintChunks();
  EXPECT_THAT(
      chunks,
      ElementsAre(PaintChunk(0, 2, nullptr, DefaultPaintChunkProperties()),
                  PaintChunk(2, 4, nullptr, simple_transform),
                  PaintChunk(4, 5, nullptr, simple_transform),
                  PaintChunk(5, 6, nullptr, simple_transform),
                  PaintChunk(6, 7, nullptr, DefaultPaintChunkProperties())));
}

}  // namespace
}  // namespace blink

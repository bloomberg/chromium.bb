// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/PaintChunker.h"

#include "platform/testing/PaintPropertyTestHelpers.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::blink::testing::createOpacityOnlyEffect;
using ::blink::testing::defaultPaintChunkProperties;
using ::testing::ElementsAre;

namespace blink {
namespace {

class PaintChunkerTest : public ::testing::Test,
                         private ScopedSlimmingPaintV2ForTest {
 public:
  PaintChunkerTest() : ScopedSlimmingPaintV2ForTest(true) {}

 protected:
  class TestDisplayItemClient : public DisplayItemClient {
    String debugName() const final { return "Test"; }
    LayoutRect visualRect() const final { return LayoutRect(); }
  };
  TestDisplayItemClient m_client;
};

class TestDisplayItem : public DisplayItem {
 public:
  TestDisplayItem(const DisplayItemClient& client, DisplayItem::Type type)
      : DisplayItem(client, type, sizeof(*this)) {}

  void replay(GraphicsContext&) const final { NOTREACHED(); }
  void appendToWebDisplayItemList(const IntRect&,
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
  Vector<PaintChunk> chunks = PaintChunker().releasePaintChunks();
  ASSERT_TRUE(chunks.isEmpty());
}

TEST_F(PaintChunkerTest, SingleNonEmptyRange) {
  PaintChunker chunker;
  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            defaultPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  Vector<PaintChunk> chunks = chunker.releasePaintChunks();

  EXPECT_THAT(chunks, ElementsAre(PaintChunk(0, 2, nullptr,
                                             defaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, SamePropertiesTwiceCombineIntoOneChunk) {
  PaintChunker chunker;
  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            defaultPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            defaultPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  Vector<PaintChunk> chunks = chunker.releasePaintChunks();

  EXPECT_THAT(chunks, ElementsAre(PaintChunk(0, 3, nullptr,
                                             defaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, CanRewindDisplayItemIndex) {
  PaintChunker chunker;
  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            defaultPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  chunker.decrementDisplayItemIndex();
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  Vector<PaintChunk> chunks = chunker.releasePaintChunks();

  EXPECT_THAT(chunks, ElementsAre(PaintChunk(0, 2, nullptr,
                                             defaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, BuildMultipleChunksWithSinglePropertyChanging) {
  PaintChunker chunker;
  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            defaultPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));

  PaintChunkProperties simpleTransform = defaultPaintChunkProperties();
  simpleTransform.propertyTreeState.setTransform(
      TransformPaintPropertyNode::create(nullptr,
                                         TransformationMatrix(0, 1, 2, 3, 4, 5),
                                         FloatPoint3D(9, 8, 7))
          .get());

  chunker.updateCurrentPaintChunkProperties(nullptr, simpleTransform);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));

  PaintChunkProperties anotherTransform = defaultPaintChunkProperties();
  anotherTransform.propertyTreeState.setTransform(
      TransformPaintPropertyNode::create(nullptr,
                                         TransformationMatrix(0, 1, 2, 3, 4, 5),
                                         FloatPoint3D(9, 8, 7))
          .get());
  chunker.updateCurrentPaintChunkProperties(nullptr, anotherTransform);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));

  Vector<PaintChunk> chunks = chunker.releasePaintChunks();

  EXPECT_THAT(chunks, ElementsAre(PaintChunk(0, 2, nullptr,
                                             defaultPaintChunkProperties()),
                                  PaintChunk(2, 3, nullptr, simpleTransform),
                                  PaintChunk(3, 4, nullptr, anotherTransform)));
}

TEST_F(PaintChunkerTest, BuildMultipleChunksWithDifferentPropertyChanges) {
  PaintChunker chunker;
  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            defaultPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));

  PaintChunkProperties simpleTransform = defaultPaintChunkProperties();
  simpleTransform.propertyTreeState.setTransform(
      TransformPaintPropertyNode::create(nullptr,
                                         TransformationMatrix(0, 0, 0, 0, 0, 0),
                                         FloatPoint3D(9, 8, 7))
          .get());
  chunker.updateCurrentPaintChunkProperties(nullptr, simpleTransform);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));

  PaintChunkProperties simpleTransformAndEffect = defaultPaintChunkProperties();
  simpleTransformAndEffect.propertyTreeState.setTransform(
      simpleTransform.propertyTreeState.transform());
  simpleTransformAndEffect.propertyTreeState.setEffect(
      createOpacityOnlyEffect(EffectPaintPropertyNode::root(), 0.5f).get());
  chunker.updateCurrentPaintChunkProperties(nullptr, simpleTransformAndEffect);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));

  PaintChunkProperties simpleTransformAndEffectWithUpdatedTransform =
      defaultPaintChunkProperties();
  simpleTransformAndEffectWithUpdatedTransform.propertyTreeState.setTransform(
      TransformPaintPropertyNode::create(nullptr,
                                         TransformationMatrix(1, 1, 0, 0, 0, 0),
                                         FloatPoint3D(9, 8, 7))
          .get());
  simpleTransformAndEffectWithUpdatedTransform.propertyTreeState.setEffect(
      createOpacityOnlyEffect(
          EffectPaintPropertyNode::root(),
          simpleTransformAndEffect.propertyTreeState.effect()->opacity())
          .get());
  chunker.updateCurrentPaintChunkProperties(
      nullptr, simpleTransformAndEffectWithUpdatedTransform);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));

  // Test that going back to a previous chunk property still creates a new
  // chunk.
  chunker.updateCurrentPaintChunkProperties(nullptr, simpleTransformAndEffect);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));

  Vector<PaintChunk> chunks = chunker.releasePaintChunks();

  EXPECT_THAT(
      chunks,
      ElementsAre(PaintChunk(0, 1, nullptr, defaultPaintChunkProperties()),
                  PaintChunk(1, 3, nullptr, simpleTransform),
                  PaintChunk(3, 5, nullptr, simpleTransformAndEffect),
                  PaintChunk(5, 7, nullptr,
                             simpleTransformAndEffectWithUpdatedTransform),
                  PaintChunk(7, 9, nullptr, simpleTransformAndEffect)));
}

TEST_F(PaintChunkerTest, BuildChunksFromNestedTransforms) {
  // Test that "nested" transforms linearize using the following
  // sequence of transforms and display items:
  // <root xform>, <paint>, <a xform>, <paint>, <paint>, </a xform>, <paint>,
  // </root xform>
  PaintChunker chunker;
  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            defaultPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));

  PaintChunkProperties simpleTransform = defaultPaintChunkProperties();
  simpleTransform.propertyTreeState.setTransform(
      TransformPaintPropertyNode::create(nullptr,
                                         TransformationMatrix(0, 1, 2, 3, 4, 5),
                                         FloatPoint3D(9, 8, 7))
          .get());
  chunker.updateCurrentPaintChunkProperties(nullptr, simpleTransform);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));

  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            defaultPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));

  Vector<PaintChunk> chunks = chunker.releasePaintChunks();

  EXPECT_THAT(
      chunks,
      ElementsAre(PaintChunk(0, 1, nullptr, defaultPaintChunkProperties()),
                  PaintChunk(1, 3, nullptr, simpleTransform),
                  PaintChunk(3, 4, nullptr, defaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ChangingPropertiesWithoutItems) {
  // Test that properties can change without display items being generated.
  PaintChunker chunker;
  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            defaultPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));

  PaintChunkProperties firstTransform = defaultPaintChunkProperties();
  firstTransform.propertyTreeState.setTransform(
      TransformPaintPropertyNode::create(nullptr,
                                         TransformationMatrix(0, 1, 2, 3, 4, 5),
                                         FloatPoint3D(9, 8, 7))
          .get());
  chunker.updateCurrentPaintChunkProperties(nullptr, firstTransform);

  PaintChunkProperties secondTransform = defaultPaintChunkProperties();
  secondTransform.propertyTreeState.setTransform(
      TransformPaintPropertyNode::create(nullptr,
                                         TransformationMatrix(9, 8, 7, 6, 5, 4),
                                         FloatPoint3D(3, 2, 1))
          .get());
  chunker.updateCurrentPaintChunkProperties(nullptr, secondTransform);

  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  Vector<PaintChunk> chunks = chunker.releasePaintChunks();

  EXPECT_THAT(chunks, ElementsAre(PaintChunk(0, 1, nullptr,
                                             defaultPaintChunkProperties()),
                                  PaintChunk(1, 2, nullptr, secondTransform)));
}

TEST_F(PaintChunkerTest, CreatesSeparateChunksWhenRequested) {
  // Tests that the chunker creates a separate chunks for display items which
  // require it.
  PaintChunker chunker;
  TestDisplayItemRequiringSeparateChunk i1(m_client);
  TestDisplayItemRequiringSeparateChunk i2(m_client);
  TestDisplayItemRequiringSeparateChunk i3(m_client);
  TestDisplayItemRequiringSeparateChunk i4(m_client);
  TestDisplayItemRequiringSeparateChunk i5(m_client);
  TestDisplayItemRequiringSeparateChunk i6(m_client);

  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            defaultPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  chunker.incrementDisplayItemIndex(i1);
  chunker.incrementDisplayItemIndex(i2);
  chunker.incrementDisplayItemIndex(i3);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  chunker.incrementDisplayItemIndex(i4);
  chunker.incrementDisplayItemIndex(i5);
  chunker.decrementDisplayItemIndex();
  chunker.decrementDisplayItemIndex();
  chunker.decrementDisplayItemIndex();
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  chunker.incrementDisplayItemIndex(i6);

  DisplayItem::Id id1 = i1.getId();
  DisplayItem::Id id2 = i2.getId();
  DisplayItem::Id id3 = i3.getId();
  DisplayItem::Id id6 = i6.getId();
  Vector<PaintChunk> chunks = chunker.releasePaintChunks();
  EXPECT_THAT(
      chunks,
      ElementsAre(PaintChunk(0, 1, nullptr, defaultPaintChunkProperties()),
                  PaintChunk(1, 2, &id1, defaultPaintChunkProperties()),
                  PaintChunk(2, 3, &id2, defaultPaintChunkProperties()),
                  PaintChunk(3, 4, &id3, defaultPaintChunkProperties()),
                  PaintChunk(4, 6, nullptr, defaultPaintChunkProperties()),
                  PaintChunk(6, 7, &id6, defaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ChunkIds) {
  PaintChunker chunker;
  TestDisplayItem i1(m_client, DisplayItem::kDrawingFirst);
  DisplayItem::Id id1 = i1.getId();
  TestDisplayItemRequiringSeparateChunk i2(m_client);
  DisplayItem::Id id2 = i2.getId();

  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            defaultPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));

  PaintChunkProperties simpleTransform = defaultPaintChunkProperties();
  simpleTransform.propertyTreeState.setTransform(
      TransformPaintPropertyNode::create(nullptr,
                                         TransformationMatrix(0, 1, 2, 3, 4, 5),
                                         FloatPoint3D(9, 8, 7))
          .get());
  chunker.updateCurrentPaintChunkProperties(&id1, simpleTransform);

  chunker.incrementDisplayItemIndex(i1);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  chunker.incrementDisplayItemIndex(i2);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));

  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            defaultPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));

  Vector<PaintChunk> chunks = chunker.releasePaintChunks();
  EXPECT_THAT(
      chunks,
      ElementsAre(PaintChunk(0, 2, nullptr, defaultPaintChunkProperties()),
                  PaintChunk(2, 4, &id1, simpleTransform),
                  PaintChunk(4, 5, &id2, simpleTransform),
                  PaintChunk(5, 6, nullptr, simpleTransform),
                  PaintChunk(6, 7, nullptr, defaultPaintChunkProperties())));
}

TEST_F(PaintChunkerTest, ChunkIdsSkippingCache) {
  PaintChunker chunker;
  TestDisplayItem i1(m_client, DisplayItem::kDrawingFirst);
  i1.setSkippedCache();
  DisplayItem::Id id1 = i1.getId();
  TestDisplayItemRequiringSeparateChunk i2(m_client);
  i2.setSkippedCache();

  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            defaultPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));

  PaintChunkProperties simpleTransform = defaultPaintChunkProperties();
  simpleTransform.propertyTreeState.setTransform(
      TransformPaintPropertyNode::create(nullptr,
                                         TransformationMatrix(0, 1, 2, 3, 4, 5),
                                         FloatPoint3D(9, 8, 7))
          .get());
  chunker.updateCurrentPaintChunkProperties(&id1, simpleTransform);

  chunker.incrementDisplayItemIndex(i1);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));
  chunker.incrementDisplayItemIndex(i2);
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));

  chunker.updateCurrentPaintChunkProperties(nullptr,
                                            defaultPaintChunkProperties());
  chunker.incrementDisplayItemIndex(NormalTestDisplayItem(m_client));

  Vector<PaintChunk> chunks = chunker.releasePaintChunks();
  EXPECT_THAT(
      chunks,
      ElementsAre(PaintChunk(0, 2, nullptr, defaultPaintChunkProperties()),
                  PaintChunk(2, 4, nullptr, simpleTransform),
                  PaintChunk(4, 5, nullptr, simpleTransform),
                  PaintChunk(5, 6, nullptr, simpleTransform),
                  PaintChunk(6, 7, nullptr, defaultPaintChunkProperties())));
}

}  // namespace
}  // namespace blink

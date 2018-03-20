// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/PaintChunksToCcLayer.h"

#include <initializer_list>

#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer.h"
#include "platform/graphics/LoggingCanvas.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/PaintChunk.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/testing/FakeDisplayItemClient.h"
#include "platform/testing/PaintPropertyTestHelpers.h"
#include "platform/testing/runtime_enabled_features_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
std::ostream& operator<<(std::ostream& os, const PaintRecord& record) {
  return os << "PaintRecord("
            << blink::RecordAsDebugString(record).Utf8().data() << ")";
}
}  // namespace cc

std::ostream& operator<<(std::ostream& os, const SkRect& rect) {
  if (cc::PaintOp::IsUnsetRect(rect))
    return os << "(unset)";
  return os << blink::FloatRect(rect).ToString();
}

namespace blink {
namespace {

using testing::CreateOpacityOnlyEffect;

class PaintChunksToCcLayerTest : public ::testing::Test,
                                 private ScopedSlimmingPaintV2ForTest {
 protected:
  PaintChunksToCcLayerTest() : ScopedSlimmingPaintV2ForTest(true) {}
};

// Matches PaintOpTypes in a PaintRecord.
class PaintRecordMatcher
    : public ::testing::MatcherInterface<const cc::PaintOpBuffer&> {
 public:
  static ::testing::Matcher<const cc::PaintOpBuffer&> Make(
      std::initializer_list<cc::PaintOpType> args) {
    return ::testing::MakeMatcher(new PaintRecordMatcher(args));
  }
  PaintRecordMatcher(std::initializer_list<cc::PaintOpType> args)
      : expected_ops_(args) {}

  bool MatchAndExplain(
      const cc::PaintOpBuffer& buffer,
      ::testing::MatchResultListener* listener) const override {
    size_t index = 0;
    for (cc::PaintOpBuffer::Iterator it(&buffer); it; ++it, ++index) {
      auto op = (*it)->GetType();
      if (index < expected_ops_.size() && expected_ops_[index] == op)
        continue;

      if (listener->IsInterested()) {
        *listener << "unexpected op " << op << " at index " << index << ",";
        if (index < expected_ops_.size())
          *listener << " expecting " << expected_ops_[index] << ".";
        else
          *listener << " expecting end of list.";
      }
      return false;
    }
    if (index == expected_ops_.size())
      return true;
    if (listener->IsInterested()) {
      *listener << "unexpected end of list, expecting " << expected_ops_[index]
                << " at index " << index << ".";
    }
    return false;
  }

  void DescribeTo(::std::ostream* os) const override {
    *os << "[";
    bool first = true;
    for (auto op : expected_ops_) {
      if (first)
        first = false;
      else
        *os << ", ";
      *os << op;
    }
    *os << "]";
  }

 private:
  Vector<cc::PaintOpType> expected_ops_;
};

#define EXPECT_EFFECT_BOUNDS(x, y, width, height, op_buffer, index)           \
  do {                                                                        \
    const auto* save_layer =                                                  \
        (op_buffer).GetOpAtForTesting<cc::SaveLayerOp>(index);                \
    ASSERT_NE(nullptr, save_layer);                                           \
    EXPECT_EQ(FloatRect(x, y, width, height), FloatRect(save_layer->bounds)); \
  } while (false)

#define EXPECT_TRANSFORM_MATRIX(transform, op_buffer, index)                 \
  do {                                                                       \
    const auto* concat = (op_buffer).GetOpAtForTesting<cc::ConcatOp>(index); \
    ASSERT_NE(nullptr, concat);                                              \
    EXPECT_EQ(transform, TransformationMatrix(concat->matrix));              \
  } while (false)

#define EXPECT_TRANSLATE(x, y, op_buffer, index)               \
  do {                                                         \
    const auto* translate =                                    \
        (op_buffer).GetOpAtForTesting<cc::TranslateOp>(index); \
    ASSERT_NE(nullptr, translate);                             \
    EXPECT_EQ(x, translate->dx);                               \
    EXPECT_EQ(y, translate->dy);                               \
  } while (false)

// Convenient shorthands.
const TransformPaintPropertyNode* t0() {
  return TransformPaintPropertyNode::Root();
}
const ClipPaintPropertyNode* c0() {
  return ClipPaintPropertyNode::Root();
}
const EffectPaintPropertyNode* e0() {
  return EffectPaintPropertyNode::Root();
}

PaintChunk::Id DefaultId() {
  DEFINE_STATIC_LOCAL(FakeDisplayItemClient, fake_client,
                      ("FakeDisplayItemClient", LayoutRect(0, 0, 100, 100)));
  return PaintChunk::Id(fake_client, DisplayItem::kDrawingFirst);
}

struct TestChunks {
  Vector<PaintChunk> chunks;
  DisplayItemList items = DisplayItemList(0);

  // Add a paint chunk with a non-empty paint record and given property nodes.
  void AddChunk(const TransformPaintPropertyNode* t,
                const ClipPaintPropertyNode* c,
                const EffectPaintPropertyNode* e,
                const FloatRect& bounds = FloatRect(0, 0, 100, 100)) {
    auto record = sk_make_sp<PaintRecord>();
    record->push<cc::DrawRectOp>(bounds, cc::PaintFlags());
    AddChunk(std::move(record), t, c, e, bounds);
  }

  // Add a paint chunk with a given paint record and property nodes.
  void AddChunk(sk_sp<PaintRecord> record,
                const TransformPaintPropertyNode* t,
                const ClipPaintPropertyNode* c,
                const EffectPaintPropertyNode* e,
                const FloatRect& bounds = FloatRect(0, 0, 100, 100)) {
    size_t i = items.size();
    items.AllocateAndConstruct<DrawingDisplayItem>(
        DefaultId().client, DefaultId().type, std::move(record));
    chunks.emplace_back(i, i + 1, DefaultId(),
                        PaintChunkProperties(PropertyTreeState(t, c, e)));
    chunks.back().bounds = bounds;
  }

  Vector<const PaintChunk*> GetChunkList() const {
    Vector<const PaintChunk*> result;
    for (const PaintChunk& chunk : chunks)
      result.push_back(&chunk);
    return result;
  }
};

TEST_F(PaintChunksToCcLayerTest, EffectGroupingSimple) {
  // This test verifies effects are applied as a group.
  scoped_refptr<EffectPaintPropertyNode> e1 =
      CreateOpacityOnlyEffect(e0(), 0.5f);
  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e1.get(), FloatRect(0, 0, 50, 50));
  chunks.AddChunk(t0(), c0(), e1.get(), FloatRect(20, 20, 70, 70));

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(), PropertyTreeState(t0(), c0(), e0()),
          gfx::Vector2dF(), chunks.items,
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();
  EXPECT_THAT(*output,
              PaintRecordMatcher::Make({cc::PaintOpType::SaveLayer,   // <e1>
                                        cc::PaintOpType::DrawRecord,  // <p0/>
                                        cc::PaintOpType::DrawRecord,  // <p1/>
                                        cc::PaintOpType::Restore}));  // </e1>
  EXPECT_EFFECT_BOUNDS(0, 0, 90, 90, *output, 0);
}

TEST_F(PaintChunksToCcLayerTest, EffectGroupingNested) {
  // This test verifies nested effects are grouped properly.
  scoped_refptr<EffectPaintPropertyNode> e1 =
      CreateOpacityOnlyEffect(e0(), 0.5f);
  scoped_refptr<EffectPaintPropertyNode> e2 =
      CreateOpacityOnlyEffect(e1.get(), 0.5f);
  scoped_refptr<EffectPaintPropertyNode> e3 =
      CreateOpacityOnlyEffect(e1.get(), 0.5f);
  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e2.get());
  chunks.AddChunk(t0(), c0(), e3.get(), FloatRect(111, 222, 333, 444));

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(), PropertyTreeState(t0(), c0(), e0()),
          gfx::Vector2dF(), chunks.items,
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();
  EXPECT_THAT(*output,
              PaintRecordMatcher::Make({cc::PaintOpType::SaveLayer,   // <e1>
                                        cc::PaintOpType::SaveLayer,   // <e2>
                                        cc::PaintOpType::DrawRecord,  // <p0/>
                                        cc::PaintOpType::Restore,     // </e2>
                                        cc::PaintOpType::SaveLayer,   // <e3>
                                        cc::PaintOpType::DrawRecord,  // <p1/>
                                        cc::PaintOpType::Restore,     // </e3>
                                        cc::PaintOpType::Restore}));  // </e1>
  EXPECT_EFFECT_BOUNDS(0, 0, 444, 666, *output, 0);
  EXPECT_EFFECT_BOUNDS(0, 0, 100, 100, *output, 1);
  EXPECT_EFFECT_BOUNDS(111, 222, 333, 444, *output, 4);
}

TEST_F(PaintChunksToCcLayerTest, EffectFilterGroupingNestedWithTransforms) {
  // This test verifies nested effects with transforms are grouped properly.
  auto t1 = TransformPaintPropertyNode::Create(
      t0(), TransformationMatrix().Scale(2.f), FloatPoint3D());
  auto t2 = TransformPaintPropertyNode::Create(
      t1.get(), TransformationMatrix().Translate(-50, -50), FloatPoint3D());
  auto e1 = EffectPaintPropertyNode::Create(e0(), t2.get(), c0(), ColorFilter(),
                                            CompositorFilterOperations(), .5f,
                                            SkBlendMode::kSrcOver);
  CompositorFilterOperations filter;
  filter.AppendBlurFilter(5);
  auto e2 = EffectPaintPropertyNode::Create(
      e1.get(), t2.get(), c0(), ColorFilter(), filter, 1.f,
      SkBlendMode::kSrcOver, CompositingReason::kNone, CompositorElementId(),
      FloatPoint(60, 60));
  CreateOpacityOnlyEffect(e1.get(), 0.5f);
  TestChunks chunks;
  chunks.AddChunk(t2.get(), c0(), e1.get(), FloatRect(0, 0, 50, 50));
  chunks.AddChunk(t1.get(), c0(), e2.get(), FloatRect(20, 20, 70, 70));

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(), PropertyTreeState(t0(), c0(), e0()),
          gfx::Vector2dF(), chunks.items,
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();
  EXPECT_THAT(
      *output,
      PaintRecordMatcher::Make(
          {cc::PaintOpType::SaveLayer,                      // <e1>
           cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t1*t2>
           cc::PaintOpType::DrawRecord,                     // <p1/>
           cc::PaintOpType::Restore,                        // </t1*t2>
           cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t1*t2+e2_offset>
           cc::PaintOpType::SaveLayer,                      // <e2>
           cc::PaintOpType::Translate,                      // </e2_offset>
           cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t2^-1>
           cc::PaintOpType::DrawRecord,                     // <p2/>
           cc::PaintOpType::Restore,                        // </t2^-1>
           cc::PaintOpType::Restore,                        // </e2>
           cc::PaintOpType::Restore,                        // </t1*t2>
           cc::PaintOpType::Restore}));                     // </e1>
  // t1(t2(chunk1.bounds + e2(t2^-1(chunk2.bounds))))
  EXPECT_EFFECT_BOUNDS(-100, -100, 310, 310, *output, 0);
  EXPECT_TRANSFORM_MATRIX(t1->Matrix() * t2->Matrix(), *output, 2);
  EXPECT_TRANSFORM_MATRIX((t1->Matrix() * t2->Matrix()).Translate(60, 60),
                          *output, 6);
  // t2^-1(chunk2.bounds) - e2_offset
  EXPECT_EFFECT_BOUNDS(10, 10, 70, 70, *output, 7);
  EXPECT_TRANSLATE(-60, -60, *output, 8);
  EXPECT_TRANSFORM_MATRIX(t2->Matrix().Inverse(), *output, 10);
}

TEST_F(PaintChunksToCcLayerTest, InterleavedClipEffect) {
  // This test verifies effects are enclosed by their output clips.
  // It is the same as the example made in the class comments of
  // ConversionContext.
  // Refer to PaintChunksToCcLayer.cpp for detailed explanation.
  // (Search "State management example".)
  scoped_refptr<ClipPaintPropertyNode> c1 = ClipPaintPropertyNode::Create(
      c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  scoped_refptr<ClipPaintPropertyNode> c2 = ClipPaintPropertyNode::Create(
      c1.get(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  scoped_refptr<ClipPaintPropertyNode> c3 = ClipPaintPropertyNode::Create(
      c2.get(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  scoped_refptr<ClipPaintPropertyNode> c4 = ClipPaintPropertyNode::Create(
      c3.get(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  scoped_refptr<EffectPaintPropertyNode> e1 = EffectPaintPropertyNode::Create(
      e0(), t0(), c2.get(), ColorFilter(), CompositorFilterOperations(), 0.5f,
      SkBlendMode::kSrcOver);
  scoped_refptr<EffectPaintPropertyNode> e2 = EffectPaintPropertyNode::Create(
      e1.get(), t0(), c4.get(), ColorFilter(), CompositorFilterOperations(),
      0.5f, SkBlendMode::kSrcOver);
  TestChunks chunks;
  chunks.AddChunk(t0(), c2.get(), e0());
  chunks.AddChunk(t0(), c3.get(), e0());
  chunks.AddChunk(t0(), c4.get(), e2.get(), FloatRect(0, 0, 50, 50));
  chunks.AddChunk(t0(), c3.get(), e1.get(), FloatRect(20, 20, 70, 70));
  chunks.AddChunk(t0(), c4.get(), e0());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(), PropertyTreeState(t0(), c0(), e0()),
          gfx::Vector2dF(), chunks.items,
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();
  EXPECT_THAT(
      *output,
      PaintRecordMatcher::Make(
          {cc::PaintOpType::Save,       cc::PaintOpType::ClipRect,  // <c1+c2>
           cc::PaintOpType::DrawRecord,                             // <p0/>
           cc::PaintOpType::Save,       cc::PaintOpType::ClipRect,  // <c3>
           cc::PaintOpType::DrawRecord,                             // <p1/>
           cc::PaintOpType::Restore,                                // </c3>
           cc::PaintOpType::SaveLayer,                              // <e1>
           cc::PaintOpType::Save,       cc::PaintOpType::ClipRect,  // <c3+c4>
           cc::PaintOpType::SaveLayer,                              // <e2>
           cc::PaintOpType::DrawRecord,                             // <p2/>
           cc::PaintOpType::Restore,                                // </e2>
           cc::PaintOpType::Restore,                                // </c3+c4>
           cc::PaintOpType::Save,       cc::PaintOpType::ClipRect,  // <c3>
           cc::PaintOpType::DrawRecord,                             // <p3/>
           cc::PaintOpType::Restore,                                // </c3>
           cc::PaintOpType::Restore,                                // </e1>
           cc::PaintOpType::Save,       cc::PaintOpType::ClipRect,  // <c3+c4>
           cc::PaintOpType::DrawRecord,                             // <p4/>
           cc::PaintOpType::Restore,                                // </c3+c4>
           cc::PaintOpType::Restore}));                             // </c1+c2>
  EXPECT_EFFECT_BOUNDS(0, 0, 90, 90, *output, 7);
  EXPECT_EFFECT_BOUNDS(0, 0, 50, 50, *output, 10);
}

TEST_F(PaintChunksToCcLayerTest, ClipSpaceInversion) {
  // This test verifies chunks that have a shallower transform state than
  // its clip can still be painted. The infamous CSS corner case:
  // <div style="position:absolute; clip:rect(...)">
  //     <div style="position:fixed;">Clipped but not scroll along.</div>
  // </div>
  scoped_refptr<TransformPaintPropertyNode> t1 =
      TransformPaintPropertyNode::Create(
          t0(), TransformationMatrix().Scale(2.f), FloatPoint3D());
  scoped_refptr<ClipPaintPropertyNode> c1 = ClipPaintPropertyNode::Create(
      c0(), t1.get(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  TestChunks chunks;
  chunks.AddChunk(t0(), c1.get(), e0());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(), PropertyTreeState(t0(), c0(), e0()),
          gfx::Vector2dF(), chunks.items,
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();
  EXPECT_THAT(*output,
              PaintRecordMatcher::Make(
                  {cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t1
                   cc::PaintOpType::ClipRect,                       //  c1>
                   cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t1^-1>
                   cc::PaintOpType::DrawRecord,                     // <p0/>
                   cc::PaintOpType::Restore,                        // </t1^-1>
                   cc::PaintOpType::Restore}));                     // </c1 t1>
}

TEST_F(PaintChunksToCcLayerTest, OpacityEffectSpaceInversion) {
  // This test verifies chunks that have a shallower transform state than
  // its effect can still be painted. The infamous CSS corner case:
  // <div style="overflow:scroll">
  //   <div style="opacity:0.5">
  //     <div style="position:absolute;">Transparent but not scroll along.</div>
  //   </div>
  // </div>
  scoped_refptr<TransformPaintPropertyNode> t1 =
      TransformPaintPropertyNode::Create(
          t0(), TransformationMatrix().Scale(2.f), FloatPoint3D());
  scoped_refptr<EffectPaintPropertyNode> e1 = EffectPaintPropertyNode::Create(
      e0(), t1.get(), c0(), ColorFilter(), CompositorFilterOperations(), 0.5f,
      SkBlendMode::kSrcOver);
  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e1.get());
  chunks.AddChunk(t1.get(), c0(), e1.get());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(), PropertyTreeState(t0(), c0(), e0()),
          gfx::Vector2dF(), chunks.items,
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();
  EXPECT_THAT(*output,
              PaintRecordMatcher::Make({cc::PaintOpType::SaveLayer,   // <e1>
                                        cc::PaintOpType::DrawRecord,  // <p0/>
                                        cc::PaintOpType::Save,
                                        cc::PaintOpType::Concat,      // <t1>
                                        cc::PaintOpType::DrawRecord,  // <p1/>
                                        cc::PaintOpType::Restore,     // </t1>
                                        cc::PaintOpType::Restore}));  // </e1>
  EXPECT_EFFECT_BOUNDS(0, 0, 200, 200, *output, 0);
  EXPECT_TRANSFORM_MATRIX(t1->Matrix(), *output, 3);
}

TEST_F(PaintChunksToCcLayerTest, FilterEffectSpaceInversion) {
  // This test verifies chunks that have a shallower transform state than
  // its effect can still be painted. The infamous CSS corner case:
  // <div style="overflow:scroll">
  //   <div style="filter:blur(1px)">
  //     <div style="position:absolute;">Filtered but not scroll along.</div>
  //   </div>
  // </div>
  auto t1 = TransformPaintPropertyNode::Create(
      t0(), TransformationMatrix().Scale(2.f), FloatPoint3D());
  CompositorFilterOperations filter;
  filter.AppendBlurFilter(5);
  auto e1 = EffectPaintPropertyNode::Create(
      e0(), t1.get(), c0(), ColorFilter(), filter, 1.f, SkBlendMode::kSrcOver,
      CompositingReason::kNone, CompositorElementId(), FloatPoint(66, 88));
  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e1.get());

  auto output = PaintChunksToCcLayer::Convert(
                    chunks.GetChunkList(), PropertyTreeState(t0(), c0(), e0()),
                    gfx::Vector2dF(), chunks.items,
                    cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
                    ->ReleaseAsRecord();
  EXPECT_THAT(
      *output,
      PaintRecordMatcher::Make(
          {cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t1*e1_offset>
           cc::PaintOpType::SaveLayer,                      // <e1>
           cc::PaintOpType::Translate,                      // </e1_offset>
           cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t1^-1>
           cc::PaintOpType::DrawRecord,                     // <p0/>
           cc::PaintOpType::Restore,                        // </t1^-1>
           cc::PaintOpType::Restore,                        // </e1>
           cc::PaintOpType::Restore}));                     // </t1>
  EXPECT_TRANSFORM_MATRIX(TransformationMatrix(t1->Matrix()).Translate(66, 88),
                          *output, 1);
  EXPECT_EFFECT_BOUNDS(-66, -88, 50, 50, *output, 2);
  EXPECT_TRANSLATE(-66, -88, *output, 3);
  EXPECT_TRANSFORM_MATRIX(t1->Matrix().Inverse(), *output, 5);
}

TEST_F(PaintChunksToCcLayerTest, NonRootLayerSimple) {
  // This test verifies a layer with composited property state does not
  // apply properties again internally.
  scoped_refptr<TransformPaintPropertyNode> t1 =
      TransformPaintPropertyNode::Create(
          t0(), TransformationMatrix().Scale(2.f), FloatPoint3D());
  scoped_refptr<ClipPaintPropertyNode> c1 = ClipPaintPropertyNode::Create(
      c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  scoped_refptr<EffectPaintPropertyNode> e1 =
      CreateOpacityOnlyEffect(e0(), 0.5f);
  TestChunks chunks;
  chunks.AddChunk(t1.get(), c1.get(), e1.get());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(),
          PropertyTreeState(t1.get(), c1.get(), e1.get()), gfx::Vector2dF(),
          chunks.items, cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();
  EXPECT_THAT(*output, PaintRecordMatcher::Make({cc::PaintOpType::DrawRecord}));
}

TEST_F(PaintChunksToCcLayerTest, NonRootLayerTransformEscape) {
  // This test verifies chunks that have a shallower transform state than the
  // layer can still be painted.
  scoped_refptr<TransformPaintPropertyNode> t1 =
      TransformPaintPropertyNode::Create(
          t0(), TransformationMatrix().Scale(2.f), FloatPoint3D());
  scoped_refptr<ClipPaintPropertyNode> c1 = ClipPaintPropertyNode::Create(
      c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  scoped_refptr<EffectPaintPropertyNode> e1 =
      CreateOpacityOnlyEffect(e0(), 0.5f);
  TestChunks chunks;
  chunks.AddChunk(t0(), c1.get(), e1.get());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(),
          PropertyTreeState(t1.get(), c1.get(), e1.get()), gfx::Vector2dF(),
          chunks.items, cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();
  EXPECT_THAT(*output,
              PaintRecordMatcher::Make(
                  {cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t1^-1>
                   cc::PaintOpType::DrawRecord,                     // <p0/>
                   cc::PaintOpType::Restore}));                     // </t1^-1>
}

TEST_F(PaintChunksToCcLayerTest, EffectWithNoOutputClip) {
  // This test verifies effect with no output clip can be correctly processed.
  scoped_refptr<ClipPaintPropertyNode> c1 = ClipPaintPropertyNode::Create(
      c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  scoped_refptr<ClipPaintPropertyNode> c2 = ClipPaintPropertyNode::Create(
      c1.get(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  scoped_refptr<EffectPaintPropertyNode> e1 = EffectPaintPropertyNode::Create(
      e0(), t0(), nullptr, kColorFilterNone, CompositorFilterOperations(), 0.5,
      SkBlendMode::kSrcOver);

  TestChunks chunks;
  chunks.AddChunk(t0(), c2.get(), e1.get());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(), PropertyTreeState(t0(), c1.get(), e0()),
          gfx::Vector2dF(), chunks.items,
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();
  EXPECT_THAT(*output,
              PaintRecordMatcher::Make({cc::PaintOpType::SaveLayer,  // <e1>
                                        cc::PaintOpType::Save,
                                        cc::PaintOpType::ClipRect,    // <c2>
                                        cc::PaintOpType::DrawRecord,  // <p0/>
                                        cc::PaintOpType::Restore,     // </c2>
                                        cc::PaintOpType::Restore}));  // </e1>
  EXPECT_EFFECT_BOUNDS(0, 0, 100, 100, *output, 0);
}

TEST_F(PaintChunksToCcLayerTest,
       EffectWithNoOutputClipNestedInDecompositedEffect) {
  scoped_refptr<ClipPaintPropertyNode> c1 = ClipPaintPropertyNode::Create(
      c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  scoped_refptr<EffectPaintPropertyNode> e1 = EffectPaintPropertyNode::Create(
      e0(), t0(), c0(), kColorFilterNone, CompositorFilterOperations(), 0.5,
      SkBlendMode::kSrcOver);
  scoped_refptr<EffectPaintPropertyNode> e2 = EffectPaintPropertyNode::Create(
      e1.get(), t0(), nullptr, kColorFilterNone, CompositorFilterOperations(),
      0.5, SkBlendMode::kSrcOver);

  TestChunks chunks;
  chunks.AddChunk(t0(), c1.get(), e2.get());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(), PropertyTreeState(t0(), c0(), e0()),
          gfx::Vector2dF(), chunks.items,
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();
  EXPECT_THAT(*output,
              PaintRecordMatcher::Make({cc::PaintOpType::SaveLayer,  // <e1>
                                        cc::PaintOpType::SaveLayer,  // <e2>
                                        cc::PaintOpType::Save,
                                        cc::PaintOpType::ClipRect,    // <c1>
                                        cc::PaintOpType::DrawRecord,  // <p0/>
                                        cc::PaintOpType::Restore,     // </c1>
                                        cc::PaintOpType::Restore,     // </e2>
                                        cc::PaintOpType::Restore}));  // </e1>
  EXPECT_EFFECT_BOUNDS(0, 0, 100, 100, *output, 0);
  EXPECT_EFFECT_BOUNDS(0, 0, 100, 100, *output, 1);
}

TEST_F(PaintChunksToCcLayerTest,
       EffectWithNoOutputClipNestedInCompositedEffect) {
  scoped_refptr<ClipPaintPropertyNode> c1 = ClipPaintPropertyNode::Create(
      c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  scoped_refptr<EffectPaintPropertyNode> e1 = EffectPaintPropertyNode::Create(
      e0(), t0(), c0(), kColorFilterNone, CompositorFilterOperations(), 0.5,
      SkBlendMode::kSrcOver);
  scoped_refptr<EffectPaintPropertyNode> e2 = EffectPaintPropertyNode::Create(
      e1.get(), t0(), nullptr, kColorFilterNone, CompositorFilterOperations(),
      0.5, SkBlendMode::kSrcOver);

  TestChunks chunks;
  chunks.AddChunk(t0(), c1.get(), e2.get());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(), PropertyTreeState(t0(), c0(), e1.get()),
          gfx::Vector2dF(), chunks.items,
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();
  EXPECT_THAT(*output,
              PaintRecordMatcher::Make({cc::PaintOpType::SaveLayer,  // <e2>
                                        cc::PaintOpType::Save,
                                        cc::PaintOpType::ClipRect,    // <c1>
                                        cc::PaintOpType::DrawRecord,  // <p0/>
                                        cc::PaintOpType::Restore,     // </c1>
                                        cc::PaintOpType::Restore}));  // </e2>
  EXPECT_EFFECT_BOUNDS(0, 0, 100, 100, *output, 0);
}

TEST_F(PaintChunksToCcLayerTest,
       EffectWithNoOutputClipNestedInCompositedEffectAndClip) {
  scoped_refptr<ClipPaintPropertyNode> c1 = ClipPaintPropertyNode::Create(
      c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  scoped_refptr<EffectPaintPropertyNode> e1 = EffectPaintPropertyNode::Create(
      e0(), t0(), c0(), kColorFilterNone, CompositorFilterOperations(), 0.5,
      SkBlendMode::kSrcOver);
  scoped_refptr<EffectPaintPropertyNode> e2 = EffectPaintPropertyNode::Create(
      e1.get(), t0(), nullptr, kColorFilterNone, CompositorFilterOperations(),
      0.5, SkBlendMode::kSrcOver);

  TestChunks chunks;
  chunks.AddChunk(t0(), c1.get(), e2.get());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(), PropertyTreeState(t0(), c1.get(), e1.get()),
          gfx::Vector2dF(), chunks.items,
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();
  EXPECT_THAT(*output,
              PaintRecordMatcher::Make({cc::PaintOpType::SaveLayer,   // <e2>
                                        cc::PaintOpType::DrawRecord,  // <p0/>
                                        cc::PaintOpType::Restore}));  // </e2>
  EXPECT_EFFECT_BOUNDS(0, 0, 100, 100, *output, 0);
}

TEST_F(PaintChunksToCcLayerTest, VisualRect) {
  auto layer_transform = TransformPaintPropertyNode::Create(
      t0(), TransformationMatrix().Scale(20), FloatPoint3D());
  auto chunk_transform = TransformPaintPropertyNode::Create(
      layer_transform.get(), TransformationMatrix().Translate(50, 100),
      FloatPoint3D());

  TestChunks chunks;
  chunks.AddChunk(chunk_transform.get(), c0(), e0());

  auto cc_list = base::MakeRefCounted<cc::DisplayItemList>(
      cc::DisplayItemList::kTopLevelDisplayItemList);
  PaintChunksToCcLayer::ConvertInto(
      chunks.GetChunkList(),
      PropertyTreeState(layer_transform.get(), c0(), e0()),
      gfx::Vector2dF(100, 200), chunks.items, *cc_list);
  EXPECT_EQ(gfx::Rect(-50, -100, 100, 100), cc_list->VisualRectForTesting(4));

  EXPECT_THAT(
      *cc_list->ReleaseAsRecord(),
      PaintRecordMatcher::Make({cc::PaintOpType::Save,       //
                                cc::PaintOpType::Translate,  // <layer_offset>
                                cc::PaintOpType::Save,       //
                                cc::PaintOpType::Concat,  // <layer_transform>
                                cc::PaintOpType::DrawRecord,  // <p0/>
                                cc::PaintOpType::Restore,  // </layer_transform>
                                cc::PaintOpType::Restore}));  // </layer_offset>
}

TEST_F(PaintChunksToCcLayerTest, NoncompositedClipPath) {
  scoped_refptr<RefCountedPath> clip_path = base::AdoptRef(new RefCountedPath);
  auto c1 = ClipPaintPropertyNode::Create(
      c0(), t0(), FloatRoundedRect(25.f, 25.f, 100.f, 100.f), nullptr,
      clip_path);

  TestChunks chunks;
  chunks.AddChunk(t0(), c1.get(), e0());

  auto cc_list = base::MakeRefCounted<cc::DisplayItemList>(
      cc::DisplayItemList::kTopLevelDisplayItemList);
  PaintChunksToCcLayer::ConvertInto(chunks.GetChunkList(),
                                    PropertyTreeState(t0(), c0(), e0()),
                                    gfx::Vector2dF(), chunks.items, *cc_list);

  EXPECT_THAT(
      *cc_list->ReleaseAsRecord(),
      PaintRecordMatcher::Make({cc::PaintOpType::Save,        //
                                cc::PaintOpType::ClipRect,    //
                                cc::PaintOpType::ClipPath,    // <clip_path>
                                cc::PaintOpType::DrawRecord,  // <p0/>
                                cc::PaintOpType::Restore}));  // </clip_path>
}

TEST_F(PaintChunksToCcLayerTest, EmptyClipsAreElided) {
  scoped_refptr<ClipPaintPropertyNode> c1 = ClipPaintPropertyNode::Create(
      c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  scoped_refptr<ClipPaintPropertyNode> c1c2 = ClipPaintPropertyNode::Create(
      c1.get(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  scoped_refptr<ClipPaintPropertyNode> c2 = ClipPaintPropertyNode::Create(
      c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));

  TestChunks chunks;
  chunks.AddChunk(nullptr, t0(), c1.get(), e0());
  chunks.AddChunk(nullptr, t0(), c1c2.get(), e0());
  chunks.AddChunk(nullptr, t0(), c1c2.get(), e0());
  chunks.AddChunk(nullptr, t0(), c1c2.get(), e0());
  chunks.AddChunk(nullptr, t0(), c1.get(), e0());
  // D1
  chunks.AddChunk(t0(), c2.get(), e0());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(), PropertyTreeState(t0(), c0(), e0()),
          gfx::Vector2dF(), chunks.items,
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();

  // Note that c1 and c1c2 are elided.
  EXPECT_THAT(*output, PaintRecordMatcher::Make({
                           cc::PaintOpType::Save,        //
                           cc::PaintOpType::ClipRect,    // <c2>
                           cc::PaintOpType::DrawRecord,  // D1
                           cc::PaintOpType::Restore,     // </c2>
                       }));
}

TEST_F(PaintChunksToCcLayerTest, NonEmptyClipsAreStored) {
  scoped_refptr<ClipPaintPropertyNode> c1 = ClipPaintPropertyNode::Create(
      c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  scoped_refptr<ClipPaintPropertyNode> c1c2 = ClipPaintPropertyNode::Create(
      c1.get(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  scoped_refptr<ClipPaintPropertyNode> c2 = ClipPaintPropertyNode::Create(
      c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));

  TestChunks chunks;
  chunks.AddChunk(nullptr, t0(), c1.get(), e0());
  chunks.AddChunk(nullptr, t0(), c1c2.get(), e0());
  chunks.AddChunk(nullptr, t0(), c1c2.get(), e0());
  // D1
  chunks.AddChunk(t0(), c1c2.get(), e0());
  chunks.AddChunk(nullptr, t0(), c1.get(), e0());
  // D2
  chunks.AddChunk(t0(), c2.get(), e0());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(), PropertyTreeState(t0(), c0(), e0()),
          gfx::Vector2dF(), chunks.items,
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();

  EXPECT_THAT(*output,
              PaintRecordMatcher::Make({
                  cc::PaintOpType::Save, cc::PaintOpType::ClipRect,  // <c1+c2>
                  cc::PaintOpType::DrawRecord,                       // D1
                  cc::PaintOpType::Restore,                          // </c1+c2>
                  cc::PaintOpType::Save, cc::PaintOpType::ClipRect,  // <c2>
                  cc::PaintOpType::DrawRecord,                       // D2
                  cc::PaintOpType::Restore,                          // </c2>
              }));
}

TEST_F(PaintChunksToCcLayerTest, EmptyEffectsAreStored) {
  scoped_refptr<EffectPaintPropertyNode> e1 = EffectPaintPropertyNode::Create(
      e0(), t0(), c0(), kColorFilterNone, CompositorFilterOperations(), 0.5,
      SkBlendMode::kSrcOver);

  TestChunks chunks;
  chunks.AddChunk(nullptr, t0(), c0(), e0());
  chunks.AddChunk(nullptr, t0(), c0(), e1.get());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(), PropertyTreeState(t0(), c0(), e0()),
          gfx::Vector2dF(), chunks.items,
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();

  EXPECT_THAT(*output, PaintRecordMatcher::Make({
                           cc::PaintOpType::SaveLayer,  // <e1>
                           cc::PaintOpType::Restore,    // </e1>
                       }));
  EXPECT_EFFECT_BOUNDS(0, 0, 100, 100, *output, 0);
}

TEST_F(PaintChunksToCcLayerTest, CombineClips) {
  FloatRoundedRect clip_rect(0, 0, 100, 100);
  FloatSize corner(5, 5);
  FloatRoundedRect rounded_clip_rect(clip_rect.Rect(), corner, corner, corner,
                                     corner);
  auto t1 = TransformPaintPropertyNode::Create(
      t0(), TransformationMatrix().Scale(2.f), FloatPoint3D());
  auto c1 = ClipPaintPropertyNode::Create(c0(), t0(), clip_rect);
  auto c2 = ClipPaintPropertyNode::Create(c1.get(), t0(), clip_rect);
  auto c3 = ClipPaintPropertyNode::Create(c2.get(), t1.get(), clip_rect);
  auto c4 = ClipPaintPropertyNode::Create(c3.get(), t1.get(), clip_rect);
  auto c5 =
      ClipPaintPropertyNode::Create(c4.get(), t1.get(), rounded_clip_rect);
  auto c6 = ClipPaintPropertyNode::Create(c5.get(), t1.get(), clip_rect);

  TestChunks chunks;
  chunks.AddChunk(t1.get(), c6.get(), e0());
  chunks.AddChunk(t1.get(), c3.get(), e0());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(), PropertyTreeState(t0(), c0(), e0()),
          gfx::Vector2dF(), chunks.items,
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();

  EXPECT_THAT(
      *output,
      PaintRecordMatcher::Make(
          {cc::PaintOpType::Save,       cc::PaintOpType::ClipRect,  // <c1+c2>
           cc::PaintOpType::Save,       cc::PaintOpType::Concat,    // <t1
           cc::PaintOpType::ClipRect,                               //  c3+c4>
           cc::PaintOpType::Save,       cc::PaintOpType::ClipRect,
           cc::PaintOpType::ClipRRect,                              // <c5>
           cc::PaintOpType::Save,       cc::PaintOpType::ClipRect,  // <c6>
           cc::PaintOpType::DrawRecord,                             // <p0/>
           cc::PaintOpType::Restore,                                // </c6>
           cc::PaintOpType::Restore,                                // </c5>
           cc::PaintOpType::Restore,                              // </c3+c4 t1>
           cc::PaintOpType::Save,       cc::PaintOpType::Concat,  // <t1
           cc::PaintOpType::ClipRect,                             // c3>
           cc::PaintOpType::DrawRecord,                           // <p1/>
           cc::PaintOpType::Restore,                              // </c3 t1>
           cc::PaintOpType::Restore}));                           // </c1+c2>
}

}  // namespace
}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/compositing/PaintChunksToCcLayer.h"

#include <initializer_list>

#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_op_buffer.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/PaintChunk.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/testing/FakeDisplayItemClient.h"
#include "platform/testing/PaintPropertyTestHelpers.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

using testing::CreateOpacityOnlyEffect;

class PaintChunksToCcLayerTest : public ::testing::Test,
                                 private ScopedSlimmingPaintV2ForTest {
 protected:
  PaintChunksToCcLayerTest() : ScopedSlimmingPaintV2ForTest(true) {}
};

const char* GetOpName(cc::PaintOpType op) {
  if (op == cc::PaintOpType::ClipRect)
    return "ClipRect";
  if (op == cc::PaintOpType::Concat)
    return "Concat";
  if (op == cc::PaintOpType::DrawRecord)
    return "DrawRecord";
  if (op == cc::PaintOpType::Save)
    return "Save";
  if (op == cc::PaintOpType::SaveLayer)
    return "SaveLayer";
  if (op == cc::PaintOpType::Restore)
    return "Restore";
  NOTREACHED();
  return "ERROR";
}

// A simple matcher that only looks for a few ops, ignoring all else.
// Recognized ops: ClipRect, Concat, DrawRecord, Save, SaveLayer, Restore.
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
    auto next = expected_ops_.begin();
    size_t op_idx = 0;
    for (cc::PaintOpBuffer::Iterator it(&buffer); it; ++it, ++op_idx) {
      cc::PaintOpType op = (*it)->GetType();
      switch (op) {
        case cc::PaintOpType::ClipRect:
        case cc::PaintOpType::Concat:
        case cc::PaintOpType::DrawRecord:
        case cc::PaintOpType::Save:
        case cc::PaintOpType::SaveLayer:
        case cc::PaintOpType::Restore:
          if (next == expected_ops_.end()) {
            if (listener->IsInterested()) {
              *listener << "unexpected op " << GetOpName(op) << " at index "
                        << op_idx << ", expecting end of list.";
            }
            return false;
          }
          if (*next == op) {
            next++;
            continue;
          }
          if (listener->IsInterested()) {
            *listener << "unexpected op " << GetOpName(op) << " at index "
                      << op_idx << ", expecting " << GetOpName(*next) << "(#"
                      << (next - expected_ops_.begin()) << ").";
          }
          return false;
        default:
          continue;
      }
    }
    if (next == expected_ops_.end())
      return true;
    if (listener->IsInterested()) {
      *listener << "unexpected end of list, expecting " << GetOpName(*next)
                << "(#" << (next - expected_ops_.begin()) << ").";
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
      *os << GetOpName(op);
    }
    *os << "]";
  }

 private:
  Vector<cc::PaintOpType> expected_ops_;
};

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
  DEFINE_STATIC_LOCAL(FakeDisplayItemClient, fake_client, ());
  return PaintChunk::Id(fake_client, DisplayItem::kDrawingFirst);
}

struct TestChunks {
  Vector<PaintChunk> chunks;
  DisplayItemList items = DisplayItemList(0);

  void AddChunk(const TransformPaintPropertyNode* t,
                const ClipPaintPropertyNode* c,
                const EffectPaintPropertyNode* e) {
    size_t i = items.size();
    auto record = sk_make_sp<PaintRecord>();
    record->push<cc::NoopOp>();
    items.AllocateAndConstruct<DrawingDisplayItem>(
        DefaultId().client, DefaultId().type, std::move(record),
        FloatRect(-200000, -200000, 400000, 400000));
    chunks.emplace_back(i, i + 1, DefaultId(), PropertyTreeState(t, c, e));
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
  RefPtr<EffectPaintPropertyNode> e1 = CreateOpacityOnlyEffect(e0(), 0.5f);
  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e1.Get());
  chunks.AddChunk(t0(), c0(), e1.Get());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(), PropertyTreeState(t0(), c0(), e0()),
          gfx::Vector2dF(), chunks.items,
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();
  EXPECT_THAT(
      output,
      Pointee(PaintRecordMatcher::Make(
          {cc::PaintOpType::SaveLayer, cc::PaintOpType::SaveLayer,  // <e1>
           cc::PaintOpType::DrawRecord,                             // <p0/>
           cc::PaintOpType::DrawRecord,                             // <p1/>
           cc::PaintOpType::Restore, cc::PaintOpType::Restore})));  // </e1>
}

TEST_F(PaintChunksToCcLayerTest, EffectGroupingNested) {
  // This test verifies nested effects are grouped properly.
  RefPtr<EffectPaintPropertyNode> e1 = CreateOpacityOnlyEffect(e0(), 0.5f);
  RefPtr<EffectPaintPropertyNode> e2 = CreateOpacityOnlyEffect(e1.Get(), 0.5f);
  RefPtr<EffectPaintPropertyNode> e3 = CreateOpacityOnlyEffect(e1.Get(), 0.5f);
  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e2.Get());
  chunks.AddChunk(t0(), c0(), e3.Get());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(), PropertyTreeState(t0(), c0(), e0()),
          gfx::Vector2dF(), chunks.items,
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();
  EXPECT_THAT(
      output,
      Pointee(PaintRecordMatcher::Make(
          {cc::PaintOpType::SaveLayer, cc::PaintOpType::SaveLayer,  // <e1>
           cc::PaintOpType::SaveLayer, cc::PaintOpType::SaveLayer,  // <e2>
           cc::PaintOpType::DrawRecord,                             // <p0/>
           cc::PaintOpType::Restore, cc::PaintOpType::Restore,      // </e2>
           cc::PaintOpType::SaveLayer, cc::PaintOpType::SaveLayer,  // <e3>
           cc::PaintOpType::DrawRecord,                             // <p1/>
           cc::PaintOpType::Restore, cc::PaintOpType::Restore,      // </e3>
           cc::PaintOpType::Restore, cc::PaintOpType::Restore})));  // </e1>
}

TEST_F(PaintChunksToCcLayerTest, InterleavedClipEffect) {
  // This test verifies effects are enclosed by their output clips.
  // It is the same as the example made in the class comments of
  // ConversionContext.
  // Refer to PaintChunksToCcLayer.cpp for detailed explanation.
  // (Search "State management example".)
  RefPtr<ClipPaintPropertyNode> c1 = ClipPaintPropertyNode::Create(
      c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  RefPtr<ClipPaintPropertyNode> c2 = ClipPaintPropertyNode::Create(
      c1.Get(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  RefPtr<ClipPaintPropertyNode> c3 = ClipPaintPropertyNode::Create(
      c2.Get(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  RefPtr<ClipPaintPropertyNode> c4 = ClipPaintPropertyNode::Create(
      c3.Get(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  RefPtr<EffectPaintPropertyNode> e1 = EffectPaintPropertyNode::Create(
      e0(), t0(), c2.Get(), ColorFilter(), CompositorFilterOperations(), 0.5f,
      SkBlendMode::kSrcOver);
  RefPtr<EffectPaintPropertyNode> e2 = EffectPaintPropertyNode::Create(
      e1.Get(), t0(), c4.Get(), ColorFilter(), CompositorFilterOperations(),
      0.5f, SkBlendMode::kSrcOver);
  TestChunks chunks;
  chunks.AddChunk(t0(), c3.Get(), e0());
  chunks.AddChunk(t0(), c4.Get(), e2.Get());
  chunks.AddChunk(t0(), c3.Get(), e1.Get());
  chunks.AddChunk(t0(), c4.Get(), e0());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(), PropertyTreeState(t0(), c0(), e0()),
          gfx::Vector2dF(), chunks.items,
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();
  EXPECT_THAT(
      output,
      Pointee(PaintRecordMatcher::Make(
          {cc::PaintOpType::Save,       cc::PaintOpType::ClipRect,   // <c1>
           cc::PaintOpType::Save,       cc::PaintOpType::ClipRect,   // <c2>
           cc::PaintOpType::Save,       cc::PaintOpType::ClipRect,   // <c3>
           cc::PaintOpType::DrawRecord,                              // <p0/>
           cc::PaintOpType::Restore,                                 // </c3>
           cc::PaintOpType::SaveLayer,  cc::PaintOpType::SaveLayer,  // <e1>
           cc::PaintOpType::Save,       cc::PaintOpType::ClipRect,   // <c3>
           cc::PaintOpType::Save,       cc::PaintOpType::ClipRect,   // <c4>
           cc::PaintOpType::SaveLayer,  cc::PaintOpType::SaveLayer,  // <e2>
           cc::PaintOpType::DrawRecord,                              // <p1/>
           cc::PaintOpType::Restore,    cc::PaintOpType::Restore,    // </e2>
           cc::PaintOpType::Restore,                                 // </c4>
           cc::PaintOpType::DrawRecord,                              // <p2/>
           cc::PaintOpType::Restore,                                 // </c3>
           cc::PaintOpType::Restore,    cc::PaintOpType::Restore,    // </e1>
           cc::PaintOpType::Save,       cc::PaintOpType::ClipRect,   // <c3>
           cc::PaintOpType::Save,       cc::PaintOpType::ClipRect,   // <c4>
           cc::PaintOpType::DrawRecord,                              // <p3/>
           cc::PaintOpType::Restore,                                 // </c4>
           cc::PaintOpType::Restore,                                 // </c3>
           cc::PaintOpType::Restore,                                 // </c2>
           cc::PaintOpType::Restore})));                             // </c1>
}

TEST_F(PaintChunksToCcLayerTest, ClipSpaceInversion) {
  // This test verifies chunks that have a shallower transform state than
  // its clip can still be painted. The infamous CSS corner case:
  // <div style="position:absolute; clip:rect(...)">
  //     <div style="position:fixed;">Clipped but not scroll along.</div>
  // </div>
  RefPtr<TransformPaintPropertyNode> t1 = TransformPaintPropertyNode::Create(
      t0(), TransformationMatrix().Scale(2.f), FloatPoint3D());
  RefPtr<ClipPaintPropertyNode> c1 = ClipPaintPropertyNode::Create(
      c0(), t1.Get(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  TestChunks chunks;
  chunks.AddChunk(t0(), c1.Get(), e0());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(), PropertyTreeState(t0(), c0(), e0()),
          gfx::Vector2dF(), chunks.items,
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();
  EXPECT_THAT(output,
              Pointee(PaintRecordMatcher::Make(
                  {cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t1
                   cc::PaintOpType::ClipRect,                       //  c1>
                   cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t1^-1>
                   cc::PaintOpType::DrawRecord,                     // <p0/>
                   cc::PaintOpType::Restore,                        // </t1^-1>
                   cc::PaintOpType::Restore})));                    // </c1 t1>
}

TEST_F(PaintChunksToCcLayerTest, EffectSpaceInversion) {
  // This test verifies chunks that have a shallower transform state than
  // its effect can still be painted. The infamous CSS corner case:
  // <div style="overflow:scroll">
  //     <div style="opacity:0.5">
  //         <div style="position:absolute;">Transparent but not scroll
  //         along.</div>
  //     </div>
  // </div>
  RefPtr<TransformPaintPropertyNode> t1 = TransformPaintPropertyNode::Create(
      t0(), TransformationMatrix().Scale(2.f), FloatPoint3D());
  RefPtr<EffectPaintPropertyNode> e1 = EffectPaintPropertyNode::Create(
      e0(), t1.Get(), c0(), ColorFilter(), CompositorFilterOperations(), 0.5f,
      SkBlendMode::kSrcOver);
  TestChunks chunks;
  chunks.AddChunk(t0(), c0(), e1.Get());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(), PropertyTreeState(t0(), c0(), e0()),
          gfx::Vector2dF(), chunks.items,
          cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();
  EXPECT_THAT(
      output,
      Pointee(PaintRecordMatcher::Make(
          {cc::PaintOpType::SaveLayer, cc::PaintOpType::Concat,     // <t1
           cc::PaintOpType::SaveLayer,                              //  e1>
           cc::PaintOpType::Save, cc::PaintOpType::Concat,          // <t1^-1>
           cc::PaintOpType::DrawRecord,                             // <p0/>
           cc::PaintOpType::Restore,                                // </t1^-1>
           cc::PaintOpType::Restore, cc::PaintOpType::Restore})));  // </e1 t1>
}

TEST_F(PaintChunksToCcLayerTest, NonRootLayerSimple) {
  // This test verifies a layer with composited property state does not
  // apply properties again internally.
  RefPtr<TransformPaintPropertyNode> t1 = TransformPaintPropertyNode::Create(
      t0(), TransformationMatrix().Scale(2.f), FloatPoint3D());
  RefPtr<ClipPaintPropertyNode> c1 = ClipPaintPropertyNode::Create(
      c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  RefPtr<EffectPaintPropertyNode> e1 = CreateOpacityOnlyEffect(e0(), 0.5f);
  TestChunks chunks;
  chunks.AddChunk(t1.Get(), c1.Get(), e1.Get());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(),
          PropertyTreeState(t1.Get(), c1.Get(), e1.Get()), gfx::Vector2dF(),
          chunks.items, cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();
  EXPECT_THAT(output,
              Pointee(PaintRecordMatcher::Make({cc::PaintOpType::DrawRecord})));
}

TEST_F(PaintChunksToCcLayerTest, NonRootLayerTransformEscape) {
  // This test verifies chunks that have a shallower transform state than the
  // layer can still be painted.
  RefPtr<TransformPaintPropertyNode> t1 = TransformPaintPropertyNode::Create(
      t0(), TransformationMatrix().Scale(2.f), FloatPoint3D());
  RefPtr<ClipPaintPropertyNode> c1 = ClipPaintPropertyNode::Create(
      c0(), t0(), FloatRoundedRect(0.f, 0.f, 1.f, 1.f));
  RefPtr<EffectPaintPropertyNode> e1 = CreateOpacityOnlyEffect(e0(), 0.5f);
  TestChunks chunks;
  chunks.AddChunk(t0(), c1.Get(), e1.Get());

  sk_sp<PaintRecord> output =
      PaintChunksToCcLayer::Convert(
          chunks.GetChunkList(),
          PropertyTreeState(t1.Get(), c1.Get(), e1.Get()), gfx::Vector2dF(),
          chunks.items, cc::DisplayItemList::kToBeReleasedAsPaintOpBuffer)
          ->ReleaseAsRecord();
  EXPECT_THAT(output,
              Pointee(PaintRecordMatcher::Make(
                  {cc::PaintOpType::Save, cc::PaintOpType::Concat,  // <t1^-1>
                   cc::PaintOpType::DrawRecord,                     // <p0/>
                   cc::PaintOpType::Restore})));                    // </t1^-1>
}

}  // namespace
}  // namespace blink

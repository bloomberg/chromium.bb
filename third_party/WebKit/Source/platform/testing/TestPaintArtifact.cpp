// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/TestPaintArtifact.h"

#include <memory>
#include "cc/layers/layer.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/ForeignLayerDisplayItem.h"
#include "platform/graphics/paint/PaintArtifact.h"
#include "platform/graphics/paint/PaintFlags.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/graphics/paint/PaintRecorder.h"
#include "platform/graphics/paint/ScrollHitTestDisplayItem.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/testing/FakeDisplayItemClient.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

class TestPaintArtifact::DummyRectClient : public FakeDisplayItemClient {
 public:
  DummyRectClient(const FloatRect& rect = FloatRect(),
                  Color color = Color::kTransparent)
      : rect_(rect), color_(color) {}
  LayoutRect VisualRect() const final { return EnclosingLayoutRect(rect_); }
  sk_sp<PaintRecord> MakeRecord() const;

 private:
  FloatRect rect_;
  Color color_;
};

sk_sp<PaintRecord> TestPaintArtifact::DummyRectClient::MakeRecord() const {
  PaintRecorder recorder;
  PaintCanvas* canvas = recorder.beginRecording(rect_);
  PaintFlags flags;
  flags.setColor(color_.Rgb());
  canvas->drawRect(rect_, flags);
  return recorder.finishRecordingAsPicture();
}

TestPaintArtifact::TestPaintArtifact() : display_item_list_(0), built_(false) {}

TestPaintArtifact::~TestPaintArtifact() {}

TestPaintArtifact& TestPaintArtifact::Chunk(
    PassRefPtr<const TransformPaintPropertyNode> transform,
    PassRefPtr<const ClipPaintPropertyNode> clip,
    PassRefPtr<const EffectPaintPropertyNode> effect) {
  PropertyTreeState property_tree_state(transform.Get(), clip.Get(),
                                        effect.Get());
  PaintChunkProperties properties(property_tree_state);
  return Chunk(properties);
}

TestPaintArtifact& TestPaintArtifact::Chunk(
    const PaintChunkProperties& properties) {
  if (!paint_chunks_.IsEmpty())
    paint_chunks_.back().end_index = display_item_list_.size();
  auto client = WTF::MakeUnique<DummyRectClient>();
  paint_chunks_.push_back(PaintChunk(
      display_item_list_.size(), 0,
      PaintChunk::Id(*client, DisplayItem::kDrawingFirst), properties));
  dummy_clients_.push_back(std::move(client));
  return *this;
}

TestPaintArtifact& TestPaintArtifact::RectDrawing(const FloatRect& bounds,
                                                  Color color) {
  auto client = WTF::MakeUnique<DummyRectClient>(bounds, color);
  display_item_list_.AllocateAndConstruct<DrawingDisplayItem>(
      *client, DisplayItem::kDrawingFirst, client->MakeRecord(), bounds);
  dummy_clients_.push_back(std::move(client));
  return *this;
}

TestPaintArtifact& TestPaintArtifact::ForeignLayer(
    const FloatPoint& location,
    const IntSize& size,
    scoped_refptr<cc::Layer> layer) {
  FloatRect float_bounds(location, FloatSize(size));
  auto client = WTF::MakeUnique<DummyRectClient>(float_bounds);
  display_item_list_.AllocateAndConstruct<ForeignLayerDisplayItem>(
      *client, DisplayItem::kForeignLayerFirst, std::move(layer), location,
      size);
  dummy_clients_.push_back(std::move(client));
  return *this;
}

TestPaintArtifact& TestPaintArtifact::ScrollHitTest(
    PassRefPtr<const TransformPaintPropertyNode> scroll_offset) {
  auto client = WTF::MakeUnique<DummyRectClient>();
  display_item_list_.AllocateAndConstruct<ScrollHitTestDisplayItem>(
      *client, DisplayItem::kScrollHitTest, std::move(scroll_offset));
  dummy_clients_.push_back(std::move(client));
  return *this;
}

const PaintArtifact& TestPaintArtifact::Build() {
  if (built_)
    return paint_artifact_;

  if (!paint_chunks_.IsEmpty())
    paint_chunks_.back().end_index = display_item_list_.size();
  paint_artifact_ =
      PaintArtifact(std::move(display_item_list_), std::move(paint_chunks_));
  built_ = true;
  return paint_artifact_;
}

}  // namespace blink

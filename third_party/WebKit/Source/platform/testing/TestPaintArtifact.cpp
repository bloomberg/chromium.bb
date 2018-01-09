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

namespace blink {

class TestPaintArtifact::DummyRectClient : public FakeDisplayItemClient {
 public:
  LayoutRect VisualRect() const final { return EnclosingLayoutRect(rect_); }
  void SetVisualRect(const FloatRect& rect) { rect_ = rect; }

  sk_sp<PaintRecord> MakeRecord(const FloatRect& rect, Color color) {
    rect_ = rect;
    PaintRecorder recorder;
    PaintCanvas* canvas = recorder.beginRecording(rect);
    PaintFlags flags;
    flags.setColor(color.Rgb());
    canvas->drawRect(rect, flags);
    return recorder.finishRecordingAsPicture();
  }

 private:
  FloatRect rect_;
};

TestPaintArtifact::TestPaintArtifact() : display_item_list_(0), built_(false) {}

TestPaintArtifact::~TestPaintArtifact() = default;

TestPaintArtifact& TestPaintArtifact::Chunk(
    scoped_refptr<const TransformPaintPropertyNode> transform,
    scoped_refptr<const ClipPaintPropertyNode> clip,
    scoped_refptr<const EffectPaintPropertyNode> effect) {
  return Chunk(NewClient(), transform, clip, effect);
}

TestPaintArtifact& TestPaintArtifact::Chunk(
    DisplayItemClient& client,
    scoped_refptr<const TransformPaintPropertyNode> transform,
    scoped_refptr<const ClipPaintPropertyNode> clip,
    scoped_refptr<const EffectPaintPropertyNode> effect) {
  PropertyTreeState property_tree_state(transform.get(), clip.get(),
                                        effect.get());
  PaintChunkProperties properties(property_tree_state);
  return Chunk(client, properties);
}

TestPaintArtifact& TestPaintArtifact::Chunk(
    const PaintChunkProperties& properties) {
  return Chunk(NewClient(), properties);
}

TestPaintArtifact& TestPaintArtifact::Chunk(
    DisplayItemClient& client,
    const PaintChunkProperties& properties) {
  if (!paint_chunks_.IsEmpty())
    paint_chunks_.back().end_index = display_item_list_.size();
  paint_chunks_.push_back(PaintChunk(
      display_item_list_.size(), 0,
      PaintChunk::Id(client, DisplayItem::kDrawingFirst), properties));
  // Assume PaintController has processed this chunk.
  paint_chunks_.back().client_is_just_created = false;
  return *this;
}

TestPaintArtifact& TestPaintArtifact::RectDrawing(const FloatRect& bounds,
                                                  Color color) {
  return RectDrawing(NewClient(), bounds, color);
}

TestPaintArtifact& TestPaintArtifact::RectDrawing(DisplayItemClient& client,
                                                  const FloatRect& bounds,
                                                  Color color) {
  display_item_list_.AllocateAndConstruct<DrawingDisplayItem>(
      client, DisplayItem::kDrawingFirst,
      static_cast<DummyRectClient&>(client).MakeRecord(bounds, color));
  return *this;
}

TestPaintArtifact& TestPaintArtifact::ForeignLayer(
    const FloatPoint& location,
    const IntSize& size,
    scoped_refptr<cc::Layer> layer) {
  return ForeignLayer(NewClient(), location, size, layer);
}

TestPaintArtifact& TestPaintArtifact::ForeignLayer(
    DisplayItemClient& client,
    const FloatPoint& location,
    const IntSize& size,
    scoped_refptr<cc::Layer> layer) {
  static_cast<DummyRectClient&>(client).SetVisualRect(
      FloatRect(location, FloatSize(size)));
  display_item_list_.AllocateAndConstruct<ForeignLayerDisplayItem>(
      client, DisplayItem::kForeignLayerFirst, std::move(layer), location,
      size);
  return *this;
}

TestPaintArtifact& TestPaintArtifact::ScrollHitTest(
    scoped_refptr<const TransformPaintPropertyNode> scroll_offset) {
  return ScrollHitTest(NewClient(), scroll_offset);
}

TestPaintArtifact& TestPaintArtifact::ScrollHitTest(
    DisplayItemClient& client,
    scoped_refptr<const TransformPaintPropertyNode> scroll_offset) {
  display_item_list_.AllocateAndConstruct<ScrollHitTestDisplayItem>(
      client, DisplayItem::kScrollHitTest, std::move(scroll_offset));
  return *this;
}

TestPaintArtifact& TestPaintArtifact::KnownToBeOpaque() {
  paint_chunks_.back().known_to_be_opaque = true;
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

DisplayItemClient& TestPaintArtifact::NewClient() {
  dummy_clients_.push_back(std::make_unique<DummyRectClient>());
  return *dummy_clients_.back();
}

DisplayItemClient& TestPaintArtifact::Client(size_t i) const {
  return *dummy_clients_[i];
}

}  // namespace blink

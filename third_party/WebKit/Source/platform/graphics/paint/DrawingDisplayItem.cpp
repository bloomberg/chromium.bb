// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DrawingDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "public/platform/WebDisplayItemList.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"

namespace blink {

void DrawingDisplayItem::Replay(GraphicsContext& context) const {
  if (record_)
    context.DrawRecord(record_);
}

void DrawingDisplayItem::AppendToWebDisplayItemList(
    const LayoutSize& visual_rect_offset,
    WebDisplayItemList* list) const {
  if (record_) {
    // Convert visual rect into the GraphicsLayer's coordinate space.
    LayoutRect visual_rect = VisualRect();
    visual_rect.Move(-visual_rect_offset);
    list->AppendDrawingItem(EnclosingIntRect(visual_rect), record_,
                            EnclosingIntRect(record_bounds_));
  }
}

bool DrawingDisplayItem::DrawsContent() const {
  return record_.get();
}

#ifndef NDEBUG
void DrawingDisplayItem::DumpPropertiesAsDebugString(
    StringBuilder& string_builder) const {
  DisplayItem::DumpPropertiesAsDebugString(string_builder);
  if (record_) {
    string_builder.Append(String::Format(
        ", rect: [%f,%f %fx%f], opaque: %s", record_bounds_.X(),
        record_bounds_.Y(), record_bounds_.Width(), record_bounds_.Height(),
        known_to_be_opaque_ ? "true" : "false"));
  }
}
#endif

static bool RecordsEqual(sk_sp<const PaintRecord> record1,
                         sk_sp<const PaintRecord> record2,
                         const FloatRect& bounds) {
  if (record1->size() != record2->size())
    return false;

  // TODO(enne): PaintRecord should have an operator==
  sk_sp<SkData> data1 = ToSkPicture(record1, bounds)->serialize();
  sk_sp<SkData> data2 = ToSkPicture(record2, bounds)->serialize();
  return data1->equals(data2.get());
}

static SkBitmap RecordToBitmap(sk_sp<const PaintRecord> record,
                               const FloatRect& bounds) {
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(bounds.Width(), bounds.Height()));
  SkiaPaintCanvas canvas(bitmap);
  canvas.clear(SK_ColorTRANSPARENT);
  canvas.translate(-bounds.X(), -bounds.Y());
  canvas.drawPicture(std::move(record));
  return bitmap;
}

static bool BitmapsEqual(sk_sp<const PaintRecord> record1,
                         sk_sp<const PaintRecord> record2,
                         const FloatRect& bounds) {
  SkBitmap bitmap1 = RecordToBitmap(record1, bounds);
  SkBitmap bitmap2 = RecordToBitmap(record2, bounds);
  int mismatch_count = 0;
  const int kMaxMismatches = 10;
  for (int y = 0; y < bounds.Height() && mismatch_count < kMaxMismatches; ++y) {
    for (int x = 0; x < bounds.Width() && mismatch_count < kMaxMismatches;
         ++x) {
      SkColor pixel1 = bitmap1.getColor(x, y);
      SkColor pixel2 = bitmap2.getColor(x, y);
      if (pixel1 != pixel2) {
        LOG(ERROR) << "x=" << x << " y=" << y << " " << std::hex << pixel1
                   << " vs " << std::hex << pixel2;
        ++mismatch_count;
      }
    }
  }
  return !mismatch_count;
}

bool DrawingDisplayItem::Equals(const DisplayItem& other) const {
  if (!DisplayItem::Equals(other))
    return false;

  const sk_sp<const PaintRecord>& record = this->GetPaintRecord();
  const FloatRect& bounds = this->GetPaintRecordBounds();
  const sk_sp<const PaintRecord>& other_record =
      static_cast<const DrawingDisplayItem&>(other).GetPaintRecord();
  const FloatRect& other_bounds =
      static_cast<const DrawingDisplayItem&>(other).GetPaintRecordBounds();

  if (!record && !other_record)
    return true;
  if (!record || !other_record)
    return false;
  if (bounds != other_bounds)
    return false;

  if (RecordsEqual(record, other_record, bounds))
    return true;

  // Sometimes the client may produce different records for the same visual
  // result, which should be treated as equal.
  return BitmapsEqual(std::move(record), std::move(other_record), bounds);
}

}  // namespace blink

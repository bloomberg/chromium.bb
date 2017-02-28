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
#include "third_party/skia/include/core/SkPictureAnalyzer.h"

namespace blink {

void DrawingDisplayItem::replay(GraphicsContext& context) const {
  if (m_record)
    context.drawRecord(m_record.get());
}

void DrawingDisplayItem::appendToWebDisplayItemList(
    const IntRect& visualRect,
    WebDisplayItemList* list) const {
  if (m_record)
    list->appendDrawingItem(visualRect, m_record);
}

bool DrawingDisplayItem::drawsContent() const {
  return m_record.get();
}

void DrawingDisplayItem::analyzeForGpuRasterization(
    SkPictureGpuAnalyzer& analyzer) const {
  // TODO(enne): Need an SkPictureGpuAnalyzer on PictureRecord.
  // This is a bit overkill to ToSkPicture a record just to get
  // numSlowPaths.
  if (!m_record)
    return;
  analyzer.analyzePicture(ToSkPicture(m_record.get()));
}

#ifndef NDEBUG
void DrawingDisplayItem::dumpPropertiesAsDebugString(
    StringBuilder& stringBuilder) const {
  DisplayItem::dumpPropertiesAsDebugString(stringBuilder);
  if (m_record) {
    stringBuilder.append(
        String::format(", rect: [%f,%f %fx%f]", m_record->cullRect().x(),
                       m_record->cullRect().y(), m_record->cullRect().width(),
                       m_record->cullRect().height()));
  }
}
#endif

static bool recordsEqual(const PaintRecord* record1,
                         const PaintRecord* record2) {
  if (record1->approximateOpCount() != record2->approximateOpCount())
    return false;

  // TODO(enne): PaintRecord should have an operator==
  sk_sp<SkData> data1 = ToSkPicture(record1)->serialize();
  sk_sp<SkData> data2 = ToSkPicture(record2)->serialize();
  return data1->equals(data2.get());
}

static SkBitmap recordToBitmap(const PaintRecord* record) {
  SkBitmap bitmap;
  SkRect rect = record->cullRect();
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(rect.width(), rect.height()));
  PaintCanvas canvas(bitmap);
  canvas.clear(SK_ColorTRANSPARENT);
  canvas.translate(-rect.x(), -rect.y());
  canvas.drawPicture(record);
  return bitmap;
}

static bool bitmapsEqual(const PaintRecord* record1,
                         const PaintRecord* record2) {
  SkRect rect = record1->cullRect();
  if (rect != record2->cullRect())
    return false;

  SkBitmap bitmap1 = recordToBitmap(record1);
  SkBitmap bitmap2 = recordToBitmap(record2);
  bitmap1.lockPixels();
  bitmap2.lockPixels();
  int mismatchCount = 0;
  const int maxMismatches = 10;
  for (int y = 0; y < rect.height() && mismatchCount < maxMismatches; ++y) {
    for (int x = 0; x < rect.width() && mismatchCount < maxMismatches; ++x) {
      SkColor pixel1 = bitmap1.getColor(x, y);
      SkColor pixel2 = bitmap2.getColor(x, y);
      if (pixel1 != pixel2) {
        LOG(ERROR) << "x=" << x << " y=" << y << " " << std::hex << pixel1
                   << " vs " << std::hex << pixel2;
        ++mismatchCount;
      }
    }
  }
  bitmap1.unlockPixels();
  bitmap2.unlockPixels();
  return !mismatchCount;
}

bool DrawingDisplayItem::equals(const DisplayItem& other) const {
  if (!DisplayItem::equals(other))
    return false;

  const PaintRecord* record = this->GetPaintRecord();
  const PaintRecord* otherRecord =
      static_cast<const DrawingDisplayItem&>(other).GetPaintRecord();

  if (!record && !otherRecord)
    return true;
  if (!record || !otherRecord)
    return false;

  if (recordsEqual(record, otherRecord))
    return true;

  // Sometimes the client may produce different records for the same visual
  // result, which should be treated as equal.
  return bitmapsEqual(record, otherRecord);
}

}  // namespace blink

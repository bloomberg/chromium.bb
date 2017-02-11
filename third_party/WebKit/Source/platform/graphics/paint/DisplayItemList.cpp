// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DisplayItemList.h"

#include "platform/graphics/LoggingCanvas.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/PaintChunk.h"
#include "third_party/skia/include/core/SkPictureAnalyzer.h"

#ifndef NDEBUG
#include "wtf/text/WTFString.h"
#endif

namespace blink {

DisplayItem& DisplayItemList::appendByMoving(DisplayItem& item) {
#ifndef NDEBUG
  String originalDebugString = item.asDebugString();
#endif
  ASSERT(item.hasValidClient());
  DisplayItem& result =
      ContiguousContainer::appendByMoving(item, item.derivedSize());
  // ContiguousContainer::appendByMoving() calls an in-place constructor
  // on item which replaces it with a tombstone/"dead display item" that
  // can be safely destructed but should never be used.
  ASSERT(!item.hasValidClient());
#ifndef NDEBUG
  // Save original debug string in the old item to help debugging.
  item.setClientDebugString(originalDebugString);
#endif
  return result;
}

void DisplayItemList::appendVisualRect(const IntRect& visualRect) {
  m_visualRects.push_back(visualRect);
}

DisplayItemList::Range<DisplayItemList::iterator>
DisplayItemList::itemsInPaintChunk(const PaintChunk& paintChunk) {
  return Range<iterator>(begin() + paintChunk.beginIndex,
                         begin() + paintChunk.endIndex);
}

DisplayItemList::Range<DisplayItemList::const_iterator>
DisplayItemList::itemsInPaintChunk(const PaintChunk& paintChunk) const {
  return Range<const_iterator>(begin() + paintChunk.beginIndex,
                               begin() + paintChunk.endIndex);
}

std::unique_ptr<JSONArray> DisplayItemList::subsequenceAsJSON(
    size_t beginIndex,
    size_t endIndex,
    JsonFlags options) const {
  std::unique_ptr<JSONArray> jsonArray = JSONArray::create();
  size_t i = 0;
  for (auto it = begin() + beginIndex; it != begin() + endIndex; ++it, ++i) {
    std::unique_ptr<JSONObject> json = JSONObject::create();

    const DisplayItem& displayItem = *it;
    if ((options & SkipNonDrawings) && !displayItem.isDrawing())
      continue;

    json->setInteger("index", i);
#ifndef NDEBUG
    StringBuilder stringBuilder;
    displayItem.dumpPropertiesAsDebugString(stringBuilder);

    if (options & ShownOnlyDisplayItemTypes) {
      json->setString("type",
                      DisplayItem::typeAsDebugString(displayItem.getType()));
    } else {
      json->setString("properties", stringBuilder.toString());
    }

#endif
    if (displayItem.hasValidClient()) {
#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
      if (!displayItem.client().isAlive()) {
        json->setBoolean("clientIsAlive", true);
      } else {
#else

      if (options & ShowClientDebugName) {
#endif
        json->setString(
            "clientDebugName",
            String::format("clientDebugName: \"%s\"",
                           displayItem.client().debugName().ascii().data()));
      }
#ifndef NDEBUG
      if ((options & ShowPaintRecords) && displayItem.isDrawing()) {
        if (const PaintRecord* record =
                static_cast<const DrawingDisplayItem&>(displayItem)
                    .GetPaintRecord()) {
          json->setString("record", recordAsDebugString(record));
        }
      }
#endif
    }
    if (hasVisualRect(i)) {
      IntRect localVisualRect = visualRect(i);
      json->setString(
          "visualRect",
          String::format("[%d,%d %dx%d]", localVisualRect.x(),
                         localVisualRect.y(), localVisualRect.width(),
                         localVisualRect.height()));
    }
    jsonArray->pushObject(std::move(json));
  }
  return jsonArray;
}

}  // namespace blink

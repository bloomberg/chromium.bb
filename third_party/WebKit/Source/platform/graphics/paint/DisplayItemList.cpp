// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DisplayItemList.h"

#include "platform/graphics/LoggingCanvas.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/PaintChunk.h"

#ifndef NDEBUG
#include "platform/wtf/text/WTFString.h"
#endif

namespace blink {

DisplayItemList::Range<DisplayItemList::iterator>
DisplayItemList::ItemsInPaintChunk(const PaintChunk& paint_chunk) {
  return Range<iterator>(begin() + paint_chunk.begin_index,
                         begin() + paint_chunk.end_index);
}

DisplayItemList::Range<DisplayItemList::const_iterator>
DisplayItemList::ItemsInPaintChunk(const PaintChunk& paint_chunk) const {
  return Range<const_iterator>(begin() + paint_chunk.begin_index,
                               begin() + paint_chunk.end_index);
}

std::unique_ptr<JSONArray> DisplayItemList::SubsequenceAsJSON(
    size_t begin_index,
    size_t end_index,
    JsonFlags options) const {
  std::unique_ptr<JSONArray> json_array = JSONArray::Create();
  size_t i = 0;
  for (auto it = begin() + begin_index; it != begin() + end_index; ++it, ++i) {
    std::unique_ptr<JSONObject> json = JSONObject::Create();

    const DisplayItem& display_item = *it;
    if ((options & kSkipNonDrawings) && !display_item.IsDrawing())
      continue;

    json->SetInteger("index", i);
#ifndef NDEBUG
    StringBuilder string_builder;
    display_item.DumpPropertiesAsDebugString(string_builder);

    if (options & kShownOnlyDisplayItemTypes) {
      json->SetString("type",
                      DisplayItem::TypeAsDebugString(display_item.GetType()));
    } else {
      json->SetString("properties", string_builder.ToString());
    }

#endif
    if (display_item.HasValidClient()) {
#if DCHECK_IS_ON()
      if (!display_item.Client().IsAlive()) {
        json->SetBoolean("clientIsAlive", false);
      } else {
#else
      if (options & kShowClientDebugName) {
#endif
        json->SetString(
            "clientDebugName",
            String::Format("clientDebugName: \"%s\"",
                           display_item.Client().DebugName().Ascii().data()));
      }
#ifndef NDEBUG
      if ((options & kShowPaintRecords) && display_item.IsDrawing()) {
        const DrawingDisplayItem& item =
            static_cast<const DrawingDisplayItem&>(display_item);
        if (const PaintRecord* record = item.GetPaintRecord().get()) {
          json->SetString("record", RecordAsDebugString(
                                        record, item.GetPaintRecordBounds()));
        }
      }
#endif
    }

    json->SetString("visualRect", display_item.VisualRect().ToString());

    json_array->PushObject(std::move(json));
  }
  return json_array;
}

}  // namespace blink

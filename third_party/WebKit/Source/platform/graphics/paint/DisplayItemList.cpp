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
  auto json_array = JSONArray::Create();
  AppendSubsequenceAsJSON(begin_index, end_index, options, *json_array);
  return json_array;
}

void DisplayItemList::AppendSubsequenceAsJSON(size_t begin_index,
                                              size_t end_index,
                                              JsonFlags options,
                                              JSONArray& json_array) const {
  for (size_t i = begin_index; i < end_index; ++i) {
    std::unique_ptr<JSONObject> json = JSONObject::Create();

    const auto& display_item = (*this)[i];
    if ((options & kSkipNonDrawings) && !display_item.IsDrawing())
      continue;

    json->SetInteger("index", i);

    bool show_client_debug_name = options & kShowClientDebugName;
#if DCHECK_IS_ON()
    if (display_item.HasValidClient()) {
      if (display_item.Client().IsAlive())
        show_client_debug_name = true;
      else
        json->SetBoolean("clientIsAlive", false);
    }
#endif

#ifdef NDEBUG
    // This is for NDEBUG only because DisplayItem::DumpPropertiesAsDebugString
    // will output these information.
    if (show_client_debug_name)
      json->SetString("clientDebugName", display_item.Client().DebugName());

    json->SetInteger("type", static_cast<int>(display_item.GetType()));
    json->SetString("visualRect", display_item.VisualRect().ToString());
#else
    StringBuilder string_builder;
    display_item.DumpPropertiesAsDebugString(string_builder);

    if (options & kShownOnlyDisplayItemTypes) {
      json->SetString("type",
                      DisplayItem::TypeAsDebugString(display_item.GetType()));
    } else {
      json->SetString("properties", string_builder.ToString());
    }

    if ((options & kShowPaintRecords) && display_item.IsDrawing()) {
      const auto& item = static_cast<const DrawingDisplayItem&>(display_item);
      if (const PaintRecord* record = item.GetPaintRecord().get()) {
        json->SetString(
            "record", RecordAsDebugString(record, item.GetPaintRecordBounds()));
      }
    }
#endif

    json_array.PushObject(std::move(json));
  }
}

}  // namespace blink

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/ClipPathDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/Path.h"
#include "public/platform/WebDisplayItemList.h"
#include "third_party/skia/include/core/SkPictureAnalyzer.h"
#include "third_party/skia/include/core/SkScalar.h"

namespace blink {

void BeginClipPathDisplayItem::Replay(GraphicsContext& context) const {
  context.Save();
  context.ClipPath(clip_path_, kAntiAliased);
}

void BeginClipPathDisplayItem::AppendToWebDisplayItemList(
    const IntRect& visual_rect,
    WebDisplayItemList* list) const {
  list->AppendClipPathItem(clip_path_, true);
}

void BeginClipPathDisplayItem::AnalyzeForGpuRasterization(
    SkPictureGpuAnalyzer& analyzer) const {
  // Temporarily disabled (pref regressions due to GPU veto stickiness:
  // http://crbug.com/603969).
  // analyzer.analyzeClipPath(m_clipPath, SkRegion::kIntersect_Op, true);
}

void EndClipPathDisplayItem::Replay(GraphicsContext& context) const {
  context.Restore();
}

void EndClipPathDisplayItem::AppendToWebDisplayItemList(
    const IntRect& visual_rect,
    WebDisplayItemList* list) const {
  list->AppendEndClipPathItem();
}

#ifndef NDEBUG
void BeginClipPathDisplayItem::DumpPropertiesAsDebugString(
    WTF::StringBuilder& string_builder) const {
  DisplayItem::DumpPropertiesAsDebugString(string_builder);
  string_builder.Append(WTF::String::Format(
      ", pathVerbs: %d, pathPoints: %d, windRule: \"%s\"",
      clip_path_.countVerbs(), clip_path_.countPoints(),
      clip_path_.getFillType() == SkPath::kWinding_FillType ? "nonzero"
                                                            : "evenodd"));
}

#endif

}  // namespace blink

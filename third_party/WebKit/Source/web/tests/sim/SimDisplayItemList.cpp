// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/tests/sim/SimDisplayItemList.h"

#include "core/css/parser/CSSParser.h"
#include "platform/graphics/LoggingCanvas.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "third_party/skia/include/core/SkRect.h"

namespace blink {

SimDisplayItemList::SimDisplayItemList() {}

void SimDisplayItemList::appendDrawingItem(const WebRect&,
                                           sk_sp<const PaintRecord> record) {
  SkIRect bounds = record->cullRect().roundOut();
  SimCanvas canvas(bounds.width(), bounds.height());
  record->playback(&canvas);
  m_commands.append(canvas.commands().data(), canvas.commands().size());
}

bool SimDisplayItemList::contains(SimCanvas::CommandType type,
                                  const String& colorString) const {
  Color color = 0;
  if (!colorString.isNull())
    CHECK(CSSParser::parseColor(color, colorString, true));
  for (auto& command : m_commands) {
    if (command.type == type &&
        (colorString.isNull() || command.color == color.rgb()))
      return true;
  }
  return false;
}

}  // namespace blink

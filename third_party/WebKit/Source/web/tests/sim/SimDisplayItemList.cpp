// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/tests/sim/SimDisplayItemList.h"

#include "core/css/parser/CSSParser.h"
#include "platform/graphics/LoggingCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRect.h"

namespace blink {

SimDisplayItemList::SimDisplayItemList()
    : m_containsText(false)
{
}

void SimDisplayItemList::appendDrawingItem(const WebRect&, const SkPicture* picture)
{
    m_containsText |= picture->hasText();

    SkIRect bounds = picture->cullRect().roundOut();
    SimCanvas canvas(bounds.width(), bounds.height());
    picture->playback(&canvas);
    m_commands.append(canvas.commands().data(), canvas.commands().size());
}

bool SimDisplayItemList::contains(SimCanvas::CommandType type, const String& colorString) const
{
    Color color = 0;
    if (!colorString.isNull())
        RELEASE_ASSERT(CSSParser::parseColor(color, colorString, true));
    for (auto& command : m_commands) {
        if (command.type == type && (colorString.isNull() || command.color == color.rgb()))
            return true;
    }
    return false;
}

} // namespace blink

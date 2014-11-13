// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/ClipDisplayItem.h"

#include "platform/geometry/RoundedRect.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

void ClipDisplayItem::replay(GraphicsContext* context)
{
    context->save();
    context->clip(m_clipRect);
    for (RoundedRect roundedRect : m_roundedRectClips)
        context->clipRoundedRect(roundedRect);
}

void EndClipDisplayItem::replay(GraphicsContext* context)
{
    context->restore();
}

#ifndef NDEBUG
WTF::String ClipDisplayItem::asDebugString() const
{
    return String::format("{%s, type: \"%s\", clipRect: [%d,%d,%d,%d]}",
        clientDebugString().utf8().data(), typeAsDebugString(type()).utf8().data(),
        m_clipRect.x(), m_clipRect.y(), m_clipRect.width(), m_clipRect.height());
}
#endif

} // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/FilterDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"

namespace blink {

void BeginFilterDisplayItem::replay(GraphicsContext* context)
{
    context->save();
    FloatRect boundaries = mapImageFilterRect(m_imageFilter.get(), m_bounds);
    context->translate(m_bounds.x().toFloat(), m_bounds.y().toFloat());
    boundaries.move(-m_bounds.x().toFloat(), -m_bounds.y().toFloat());
    context->beginLayer(1, CompositeSourceOver, &boundaries, ColorFilterNone, m_imageFilter.get());
    context->translate(-m_bounds.x().toFloat(), -m_bounds.y().toFloat());
}

#ifndef NDEBUG
WTF::String BeginFilterDisplayItem::asDebugString() const
{
    return String::format("{%s, type: \"%s\", filter bounds: [%f,%f,%f,%f]}",
        clientDebugString().utf8().data(), typeAsDebugString(type()).utf8().data(),
        m_bounds.x().toFloat(), m_bounds.y().toFloat(), m_bounds.width().toFloat(), m_bounds.height().toFloat());
}
#endif

void EndFilterDisplayItem::replay(GraphicsContext* context)
{
    context->endLayer();
    context->restore();
}

#ifndef NDEBUG
WTF::String EndFilterDisplayItem::asDebugString() const
{
    return String::format("{%s, type: \"%s\"}",
        clientDebugString().utf8().data(), typeAsDebugString(type()).utf8().data());
}
#endif

} // namespace blink

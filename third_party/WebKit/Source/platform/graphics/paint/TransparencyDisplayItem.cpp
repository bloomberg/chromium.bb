// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/TransparencyDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"

namespace blink {

void BeginTransparencyDisplayItem::replay(GraphicsContext* context)
{
    context->save();
    context->clip(m_clipRect);

    bool hasBlendMode = this->hasBlendMode();
    if (hasBlendMode)
        context->setCompositeOperation(context->compositeOperation(), m_blendMode);

    context->beginTransparencyLayer(m_opacity);

    if (hasBlendMode)
        context->setCompositeOperation(context->compositeOperation(), WebBlendModeNormal);
#ifdef REVEAL_TRANSPARENCY_LAYERS
    context->fillRect(clipRect, Color(0.0f, 0.0f, 0.5f, 0.2f));
#endif
}

#ifndef NDEBUG
WTF::String BeginTransparencyDisplayItem::asDebugString() const
{
    return String::format("{%s, type: \"%s\", clip bounds: [%f,%f,%f,%f], hasBlendMode: %d, blendMode: %d, opacity: %f}",
        clientDebugString().utf8().data(), typeAsDebugString(type()).utf8().data(),
        m_clipRect.x().toFloat(), m_clipRect.y().toFloat(), m_clipRect.width().toFloat(), m_clipRect.height().toFloat(),
        hasBlendMode(), m_blendMode, m_opacity);
}
#endif

void EndTransparencyDisplayItem::replay(GraphicsContext* context)
{
    context->endLayer();
    context->restore();
}

#ifndef NDEBUG
WTF::String EndTransparencyDisplayItem::asDebugString() const
{
    return String::format("{%s, type: \"%s\"}",
        clientDebugString().utf8().data(), typeAsDebugString(type()).utf8().data());
}
#endif

} // namespace blink

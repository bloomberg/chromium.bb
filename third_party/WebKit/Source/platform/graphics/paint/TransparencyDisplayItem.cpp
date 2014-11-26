// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/TransparencyDisplayItem.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

void BeginTransparencyDisplayItem::replay(GraphicsContext* context)
{
    bool hasBlendMode = this->hasBlendMode();
    if (hasBlendMode)
        context->setCompositeOperation(context->compositeOperation(), m_blendMode);

    context->beginTransparencyLayer(m_opacity);

    if (hasBlendMode)
        context->setCompositeOperation(context->compositeOperation(), WebBlendModeNormal);
}

#ifndef NDEBUG
WTF::String BeginTransparencyDisplayItem::asDebugString() const
{
    return String::format("{%s, type: \"%s\", hasBlendMode: %d, blendMode: %d, opacity: %f}",
        clientDebugString().utf8().data(), typeAsDebugString(type()).utf8().data(),
        hasBlendMode(), m_blendMode, m_opacity);
}
#endif

void EndTransparencyDisplayItem::replay(GraphicsContext* context)
{
    context->endLayer();
}

#ifndef NDEBUG
WTF::String EndTransparencyDisplayItem::asDebugString() const
{
    return String::format("{%s, type: \"%s\"}",
        clientDebugString().utf8().data(), typeAsDebugString(type()).utf8().data());
}
#endif

} // namespace blink

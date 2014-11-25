// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/TransparencyDisplayItem.h"

#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderView.h"
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
        rendererDebugString(renderer()).utf8().data(), typeAsDebugString(type()).utf8().data(),
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
        rendererDebugString(renderer()).utf8().data(), typeAsDebugString(type()).utf8().data());
}
#endif

TransparencyRecorder::TransparencyRecorder(GraphicsContext* graphicsContext, const RenderObject* renderer, DisplayItem::Type type, const WebBlendMode& blendMode, const float opacity)
    : m_renderer(renderer)
    , m_type(type)
    , m_graphicsContext(graphicsContext)
{
    OwnPtr<BeginTransparencyDisplayItem> beginTransparencyDisplayItem = adoptPtr(new BeginTransparencyDisplayItem(renderer, type, blendMode, opacity));
    if (RuntimeEnabledFeatures::slimmingPaintEnabled())
        renderer->view()->viewDisplayList().add(beginTransparencyDisplayItem.release());
    else
        beginTransparencyDisplayItem->replay(graphicsContext);
}

TransparencyRecorder::~TransparencyRecorder()
{
    OwnPtr<EndTransparencyDisplayItem> endTransparencyDisplayItem = adoptPtr(new EndTransparencyDisplayItem(m_renderer, m_type));
    if (RuntimeEnabledFeatures::slimmingPaintEnabled())
        m_renderer->view()->viewDisplayList().add(endTransparencyDisplayItem.release());
    else
        endTransparencyDisplayItem->replay(m_graphicsContext);
}

} // namespace blink

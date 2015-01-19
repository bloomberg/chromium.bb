// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/CompositingDisplayItem.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "public/platform/WebDisplayItemList.h"

namespace blink {

void BeginCompositingDisplayItem::replay(GraphicsContext* context)
{
    context->setCompositeOperation(m_preCompositeOp, m_preBlendMode);
    context->beginLayer(m_opacity, WebCoreCompositeToSkiaComposite(m_preCompositeOp, m_preBlendMode));
    context->setCompositeOperation(m_postCompositeOp, WebBlendModeNormal);
}

void BeginCompositingDisplayItem::appendToWebDisplayItemList(WebDisplayItemList* list) const
{
    // FIXME: Change this to appendCompositingItem.
    list->appendTransparencyItem(m_opacity, m_preBlendMode);
}

#ifndef NDEBUG
void BeginCompositingDisplayItem::dumpPropertiesAsDebugString(WTF::StringBuilder& stringBuilder) const
{
    DisplayItem::dumpPropertiesAsDebugString(stringBuilder);
    stringBuilder.append(WTF::String::format(", preCompositingOp: %d, preBlendMode: %d, opacity: %f, postCompositingOp: %d", m_preCompositeOp, m_preBlendMode, m_opacity, m_postCompositeOp));
}
#endif

void EndCompositingDisplayItem::replay(GraphicsContext* context)
{
    context->endLayer();
}

void EndCompositingDisplayItem::appendToWebDisplayItemList(WebDisplayItemList* list) const
{
    // FIXME: Change this to appendEndCompositingItem.
    list->appendEndTransparencyItem();
}

} // namespace blink

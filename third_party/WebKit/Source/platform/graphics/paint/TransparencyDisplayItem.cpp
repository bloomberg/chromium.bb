// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/TransparencyDisplayItem.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "public/platform/WebDisplayItemList.h"

namespace blink {

void BeginTransparencyDisplayItem::replay(GraphicsContext* context)
{
    context->setCompositeOperation(m_compositeOperator, m_blendMode);
    context->beginTransparencyLayer(m_opacity);
    context->setCompositeOperation(m_compositeOperator, WebBlendModeNormal);
}

void BeginTransparencyDisplayItem::appendToWebDisplayItemList(WebDisplayItemList* list) const
{
    list->appendTransparencyItem(m_opacity, m_blendMode);
}

#ifndef NDEBUG
void BeginTransparencyDisplayItem::dumpPropertiesAsDebugString(WTF::StringBuilder& stringBuilder) const
{
    DisplayItem::dumpPropertiesAsDebugString(stringBuilder);
    stringBuilder.append(WTF::String::format(", hasBlendMode: %d, blendMode: %d, opacity: %f", hasBlendMode(), m_blendMode, m_opacity));
}
#endif

void EndTransparencyDisplayItem::replay(GraphicsContext* context)
{
    context->endLayer();
}

void EndTransparencyDisplayItem::appendToWebDisplayItemList(WebDisplayItemList* list) const
{
    list->appendEndTransparencyItem();
}

} // namespace blink

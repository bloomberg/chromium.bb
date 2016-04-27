// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/ClipDisplayItem.h"

#include "platform/geometry/FloatRoundedRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "public/platform/WebDisplayItemList.h"
#include "third_party/skia/include/core/SkScalar.h"

namespace blink {

void ClipDisplayItem::replay(GraphicsContext& context) const
{
    context.save();
    context.clipRect(m_clipRect, AntiAliased);

    for (const FloatRoundedRect& roundedRect : m_roundedRectClips)
        context.clipRoundedRect(roundedRect);
}

void ClipDisplayItem::appendToWebDisplayItemList(const IntRect& visualRect, WebDisplayItemList* list) const
{
    WebVector<SkRRect> webRoundedRects(m_roundedRectClips.size());
    for (size_t i = 0; i < m_roundedRectClips.size(); ++i) {
        FloatRoundedRect::Radii rectRadii = m_roundedRectClips[i].getRadii();
        SkVector skRadii[4];
        skRadii[SkRRect::kUpperLeft_Corner].set(SkIntToScalar(rectRadii.topLeft().width()),
            SkIntToScalar(rectRadii.topLeft().height()));
        skRadii[SkRRect::kUpperRight_Corner].set(SkIntToScalar(rectRadii.topRight().width()),
            SkIntToScalar(rectRadii.topRight().height()));
        skRadii[SkRRect::kLowerRight_Corner].set(SkIntToScalar(rectRadii.bottomRight().width()),
            SkIntToScalar(rectRadii.bottomRight().height()));
        skRadii[SkRRect::kLowerLeft_Corner].set(SkIntToScalar(rectRadii.bottomLeft().width()),
            SkIntToScalar(rectRadii.bottomLeft().height()));
        SkRRect skRoundedRect;
        skRoundedRect.setRectRadii(m_roundedRectClips[i].rect(), skRadii);
        webRoundedRects[i] = skRoundedRect;
    }
    list->appendClipItem(visualRect, m_clipRect, webRoundedRects);
}

void EndClipDisplayItem::replay(GraphicsContext& context) const
{
    context.restore();
}

void EndClipDisplayItem::appendToWebDisplayItemList(const IntRect& visualRect, WebDisplayItemList* list) const
{
    list->appendEndClipItem(visualRect);
}

#ifndef NDEBUG
void ClipDisplayItem::dumpPropertiesAsDebugString(WTF::StringBuilder& stringBuilder) const
{
    DisplayItem::dumpPropertiesAsDebugString(stringBuilder);
    stringBuilder.append(WTF::String::format(", clipRect: [%d,%d,%d,%d]",
        m_clipRect.x(), m_clipRect.y(), m_clipRect.width(), m_clipRect.height()));
}
#endif

} // namespace blink

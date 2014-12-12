// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "public/platform/WebDisplayItemList.h"

namespace blink {

void DrawingDisplayItem::replay(GraphicsContext* context)
{
    context->drawPicture(m_picture.get());
}

void DrawingDisplayItem::appendToWebDisplayItemList(WebDisplayItemList* list) const
{
    // FIXME: the offset is no longer necessary and should be removed.
    // FIXME: SkPictures are immutable - update WebDisplayItemList to take a const SkPicture*
    list->appendDrawingItem(const_cast<SkPicture*>(m_picture.get()), FloatPoint());
}

#ifndef NDEBUG
void DrawingDisplayItem::dumpPropertiesAsDebugString(WTF::StringBuilder& stringBuilder) const
{
    DisplayItem::dumpPropertiesAsDebugString(stringBuilder);
    stringBuilder.append(WTF::String::format(", location: [%f,%f]", m_picture->cullRect().x(), m_picture->cullRect().y()));
}
#endif

}

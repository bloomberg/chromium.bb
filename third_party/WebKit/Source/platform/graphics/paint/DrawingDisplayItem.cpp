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
    list->appendDrawingItem(m_picture.get(), FloatPoint());
}

#ifndef NDEBUG
WTF::String DrawingDisplayItem::asDebugString() const
{
    return String::format("{%s, type: \"%s\", location: [%f,%f]}",
        clientDebugString().utf8().data(), typeAsDebugString(type()).utf8().data(),
        m_picture->cullRect().x(), m_picture->cullRect().y());
}
#endif

}

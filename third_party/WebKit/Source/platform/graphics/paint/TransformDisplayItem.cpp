// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/TransformDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"

namespace blink {

BeginTransformDisplayItem::BeginTransformDisplayItem(DisplayItemClient client, const TransformationMatrix& transform)
    : DisplayItem(client, BeginTransform)
    , m_transform(transform)
{ }

void BeginTransformDisplayItem::replay(GraphicsContext* context)
{
    context->save();
    context->concatCTM(m_transform.toAffineTransform());
}

#ifndef NDEBUG
WTF::String BeginTransformDisplayItem::asDebugString() const
{
    return String::format("{%s, type: \"%s\"}",
        clientDebugString().utf8().data(), typeAsDebugString(type()).utf8().data());

}
#endif

void EndTransformDisplayItem::replay(GraphicsContext* context)
{
    context->restore();
}

#ifndef NDEBUG
WTF::String EndTransformDisplayItem::asDebugString() const
{
    return String::format("{%s, type: \"%s\"}",
        clientDebugString().utf8().data(), typeAsDebugString(type()).utf8().data());

}
#endif

} // namespace blink

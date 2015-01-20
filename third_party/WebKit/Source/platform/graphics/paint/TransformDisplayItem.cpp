// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/TransformDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/transforms/AffineTransform.h"
#include "public/platform/WebDisplayItemList.h"

namespace blink {

void BeginTransformDisplayItem::replay(GraphicsContext* context)
{
    context->save();
    context->concatCTM(m_transform);
}

void BeginTransformDisplayItem::appendToWebDisplayItemList(WebDisplayItemList* list) const
{
    list->appendTransformItem(affineTransformToSkMatrix(m_transform));
}

void EndTransformDisplayItem::replay(GraphicsContext* context)
{
    context->restore();
}

void EndTransformDisplayItem::appendToWebDisplayItemList(WebDisplayItemList* list) const
{
    list->appendEndTransformItem();
}

} // namespace blink

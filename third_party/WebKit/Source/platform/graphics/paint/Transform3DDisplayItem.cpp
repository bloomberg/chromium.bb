// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/Transform3DDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/transforms/AffineTransform.h"
#include "public/platform/WebDisplayItemList.h"

namespace blink {

void BeginTransform3DDisplayItem::replay(GraphicsContext* context)
{
    context->save();
    context->concatCTM(m_transform.toAffineTransform());
}

void BeginTransform3DDisplayItem::appendToWebDisplayItemList(WebDisplayItemList* list) const
{
    // TODO: convert to SkMatrix44 in a BeginTransform3DItem
    list->appendTransformItem(affineTransformToSkMatrix(m_transform.toAffineTransform()));
}

void EndTransform3DDisplayItem::replay(GraphicsContext* context)
{
    context->restore();
}

void EndTransform3DDisplayItem::appendToWebDisplayItemList(WebDisplayItemList* list) const
{
    // TODO: convert to End3DTransformItem
    list->appendEndTransformItem();
}

} // namespace blink

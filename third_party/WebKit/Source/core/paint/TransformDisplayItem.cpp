// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/TransformDisplayItem.h"

#include "core/paint/LayerPainter.h"
#include "core/rendering/FilterEffectRenderer.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderView.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/transforms/TransformationMatrix.h"

namespace blink {

BeginTransformDisplayItem::BeginTransformDisplayItem(const RenderObject* renderer, const TransformationMatrix& transform)
    : DisplayItem(renderer, BeginTransform)
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
        rendererDebugString(renderer()).utf8().data(), typeAsDebugString(type()).utf8().data());

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
        rendererDebugString(renderer()).utf8().data(), typeAsDebugString(type()).utf8().data());

}
#endif

} // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"

#include "core/layout/LayoutObject.h"
#include "core/paint/DeprecatedPaintLayer.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

LayoutObjectDrawingRecorder::LayoutObjectDrawingRecorder(GraphicsContext* context, const LayoutObject& layoutObject, DisplayItem::Type displayItemType, const LayoutRect& clip)
    : LayoutObjectDrawingRecorder(context, layoutObject, displayItemType, pixelSnappedIntRect(clip))
{ }

LayoutObjectDrawingRecorder::LayoutObjectDrawingRecorder(GraphicsContext* context, const LayoutObject& layoutObject, PaintPhase phase, const FloatRect& clip)
    : m_drawingRecorder(context, layoutObject.displayItemClient(), DisplayItem::paintPhaseToDrawingType(phase), clip)
#ifndef NDEBUG
    , m_layoutObject(layoutObject)
#endif
{ }

LayoutObjectDrawingRecorder::LayoutObjectDrawingRecorder(GraphicsContext* context, const LayoutObject& layoutObject, DisplayItem::Type displayItemType, const FloatRect& clip)
    : m_drawingRecorder(context, layoutObject.displayItemClient(), displayItemType, clip)
#ifndef NDEBUG
    , m_layoutObject(layoutObject)
#endif
{ }

LayoutObjectDrawingRecorder::~LayoutObjectDrawingRecorder()
{
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
        return;

#ifndef NDEBUG
    m_drawingRecorder.setClientDebugString(String::format("layoutObject: \"%p %s\"", &m_layoutObject,
        m_layoutObject.debugName().utf8().data()));
#endif
}

} // namespace blink

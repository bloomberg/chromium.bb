// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/FilterDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebDisplayItemList.h"

namespace blink {

BeginFilterDisplayItem::BeginFilterDisplayItem(DisplayItemClient client, Type type, PassRefPtr<ImageFilter> imageFilter, const FilterOperations* filterOperations, const LayoutRect& bounds)
    : DisplayItem(client, type)
    , m_imageFilter(imageFilter)
    , m_bounds(bounds)
{
    if (filterOperations) {
        SkiaImageFilterBuilder builder;
        m_webFilterOperations = adoptPtr(Platform::current()->compositorSupport()->createFilterOperations());
        FilterOutsets outsets = filterOperations->outsets();
        builder.setCropOffset(FloatSize(outsets.left(), outsets.top()));
        builder.buildFilterOperations(*filterOperations, m_webFilterOperations.get());
    }
}

void BeginFilterDisplayItem::replay(GraphicsContext* context)
{
    context->save();
    FloatRect boundaries = mapImageFilterRect(m_imageFilter.get(), m_bounds);
    context->translate(m_bounds.x().toFloat(), m_bounds.y().toFloat());
    boundaries.move(-m_bounds.x().toFloat(), -m_bounds.y().toFloat());
    context->beginLayer(1, SkXfermode::kSrcOver_Mode, &boundaries, ColorFilterNone, m_imageFilter.get());
    context->translate(-m_bounds.x().toFloat(), -m_bounds.y().toFloat());
}

void BeginFilterDisplayItem::appendToWebDisplayItemList(WebDisplayItemList* list) const
{
    list->appendFilterItem(m_imageFilter.get(), FloatRect(m_bounds));
}

#ifndef NDEBUG
void BeginFilterDisplayItem::dumpPropertiesAsDebugString(WTF::StringBuilder& stringBuilder) const
{
    DisplayItem::dumpPropertiesAsDebugString(stringBuilder);
    stringBuilder.append(WTF::String::format(", filter bounds: [%f,%f,%f,%f]",
        m_bounds.x().toFloat(), m_bounds.y().toFloat(), m_bounds.width().toFloat(), m_bounds.height().toFloat()));
}
#endif

void EndFilterDisplayItem::replay(GraphicsContext* context)
{
    context->endLayer();
    context->restore();
}

void EndFilterDisplayItem::appendToWebDisplayItemList(WebDisplayItemList* list) const
{
    list->appendEndFilterItem();
}

} // namespace blink

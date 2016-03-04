/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "core/paint/PaintLayerFilterInfo.h"

#include "core/fetch/DocumentResourceReference.h"
#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/layout/svg/ReferenceFilterBuilder.h"
#include "core/paint/FilterEffectBuilder.h"
#include "core/paint/PaintLayer.h"
#include "core/svg/SVGFilterElement.h"

namespace blink {

PaintLayerFilterInfoMap* PaintLayerFilterInfo::s_filterMap = 0;

PaintLayerFilterInfo* PaintLayerFilterInfo::filterInfoForLayer(const PaintLayer* layer)
{
    if (!s_filterMap)
        return 0;
    PaintLayerFilterInfoMap::iterator iter = s_filterMap->find(layer);
    return (iter != s_filterMap->end()) ? iter->value : 0;
}

PaintLayerFilterInfo* PaintLayerFilterInfo::createFilterInfoForLayerIfNeeded(PaintLayer* layer)
{
    if (!s_filterMap)
        s_filterMap = new PaintLayerFilterInfoMap();

    PaintLayerFilterInfoMap::iterator iter = s_filterMap->find(layer);
    if (iter != s_filterMap->end()) {
        ASSERT(layer->hasFilterInfo());
        return iter->value;
    }

    PaintLayerFilterInfo* filter = new PaintLayerFilterInfo(layer);
    s_filterMap->set(layer, filter);
    layer->setHasFilterInfo(true);
    return filter;
}

void PaintLayerFilterInfo::removeFilterInfoForLayer(PaintLayer* layer)
{
    if (!s_filterMap)
        return;
    PaintLayerFilterInfo* filter = s_filterMap->take(layer);
    if (s_filterMap->isEmpty()) {
        delete s_filterMap;
        s_filterMap = 0;
    }
    if (!filter) {
        ASSERT(!layer->hasFilterInfo());
        return;
    }
    layer->setHasFilterInfo(false);
    delete filter;
}

PaintLayerFilterInfo::PaintLayerFilterInfo(PaintLayer* layer)
    : m_layer(layer)
{
}

PaintLayerFilterInfo::~PaintLayerFilterInfo()
{
    clearFilterReferences();
}

void PaintLayerFilterInfo::setBuilder(PassRefPtrWillBeRawPtr<FilterEffectBuilder> builder)
{
    m_builder = builder;
}

void PaintLayerFilterInfo::updateReferenceFilterClients(const FilterOperations& operations)
{
    clearFilterReferences();
    addFilterReferences(operations, m_layer->layoutObject()->document());
}

void PaintLayerFilterInfo::filterNeedsInvalidation()
{
    m_layer->filterNeedsPaintInvalidation();
}

} // namespace blink


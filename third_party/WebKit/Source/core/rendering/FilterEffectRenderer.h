/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FilterEffectRenderer_h
#define FilterEffectRenderer_h

#include "core/svg/graphics/filters/SVGFilterBuilder.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntRectExtent.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "platform/graphics/filters/FilterOperations.h"
#include "platform/graphics/filters/SourceGraphic.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace blink {

class GraphicsContext;
class RenderLayer;
class RenderObject;

class FilterEffectRendererHelper {
public:
    FilterEffectRendererHelper(bool haveFilterEffect)
        : m_renderLayer(0)
        , m_haveFilterEffect(haveFilterEffect)
    {
    }

    bool haveFilterEffect() const { return m_haveFilterEffect; }

    bool prepareFilterEffect(RenderLayer*, const LayoutRect& filterBoxRect, const LayoutRect& dirtyRect);
    void beginFilterEffect(GraphicsContext*);
    void endFilterEffect(GraphicsContext*);
private:
    RenderLayer* m_renderLayer;

    FloatRect m_filterBoxRect;
    bool m_haveFilterEffect;
};

class FilterEffectRenderer final : public Filter
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<FilterEffectRenderer> create()
    {
        return adoptRef(new FilterEffectRenderer());
    }

    void setSourceImageRect(const IntRect& sourceImageRect)
    {
        m_sourceDrawingRegion = sourceImageRect;
    }
    virtual IntRect sourceImageRect() const override { return m_sourceDrawingRegion; }

    bool build(RenderObject* renderer, const FilterOperations&);
    void updateBackingStoreRect(const FloatRect& filterRect);
    void clearIntermediateResults();

    LayoutRect computeSourceImageRectForDirtyRect(const LayoutRect& filterBoxRect, const LayoutRect& dirtyRect);

    PassRefPtr<FilterEffect> lastEffect() const
    {
        return m_lastEffect;
    }
private:

    FilterEffectRenderer();
    virtual ~FilterEffectRenderer();

    IntRect m_sourceDrawingRegion;

    RefPtr<SourceGraphic> m_sourceGraphic;
    RefPtr<FilterEffect> m_lastEffect;
};

} // namespace blink


#endif // FilterEffectRenderer_h

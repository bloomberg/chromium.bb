/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/platform/graphics/DrawLooper.h"

#include "core/platform/graphics/Color.h"
#include "core/platform/graphics/FloatSize.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "third_party/skia/include/effects/SkBlurMaskFilter.h"
#include "third_party/skia/include/effects/SkDropShadowImageFilter.h"
#include "third_party/skia/include/effects/SkLayerDrawLooper.h"

namespace WebCore {

DrawLooper::DrawLooper() { }

DrawLooper::~DrawLooper() { }

SkDrawLooper* DrawLooper::skDrawLooper() const
{
    if (!m_cachedDrawLooper)
        buildCachedDrawLooper();
    return m_cachedDrawLooper.get();
}

SkImageFilter* DrawLooper::imageFilter() const
{
    if (!m_cachedImageFilter)
        buildCachedImageFilter();
    return m_cachedImageFilter.get();
}

void DrawLooper::clearCached()
{
    m_cachedDrawLooper.clear();
    m_cachedImageFilter.clear();
}

void DrawLooper::addUnmodifiedContent()
{
    DrawLooperLayerInfo info;
    info.m_layerType = UnmodifiedLayer;
    m_layerInfo.append(info);
    clearCached();
}

void DrawLooper::addShadow(const FloatSize& offset, float blur, const Color& color,
    ShadowTransformMode shadowTransformMode, ShadowAlphaMode shadowAlphaMode)
{
    // Detect when there's no effective shadow.
    if (!color.alpha())
        return;
    DrawLooperLayerInfo info;
    info.m_layerType = ShadowLayer;
    info.m_blur = blur;
    info.m_color = color;
    info.m_offset = offset;
    info.m_shadowAlphaMode = shadowAlphaMode;
    info.m_shadowTransformMode = shadowTransformMode;
    m_layerInfo.append(info);
    clearCached();
};

void DrawLooper::buildCachedDrawLooper() const
{
    m_cachedDrawLooper = adoptRef(new SkLayerDrawLooper);
    LayerVector::const_iterator info;
    for (info = m_layerInfo.begin(); info < m_layerInfo.end(); ++info) {
        if (info->m_layerType == ShadowLayer) {
            SkColor skColor = info->m_color.rgb();

            SkLayerDrawLooper::LayerInfo skInfo;

            switch (info->m_shadowAlphaMode) {
            case ShadowRespectsAlpha:
                skInfo.fColorMode = SkXfermode::kDst_Mode;
                break;
            case ShadowIgnoresAlpha:
                skInfo.fColorMode = SkXfermode::kSrc_Mode;
                break;
            default:
                ASSERT_NOT_REACHED();
            }

            if (info->m_blur)
                skInfo.fPaintBits |= SkLayerDrawLooper::kMaskFilter_Bit; // our blur
            skInfo.fPaintBits |= SkLayerDrawLooper::kColorFilter_Bit;
            skInfo.fOffset.set(info->m_offset.width(), info->m_offset.height());
            skInfo.fPostTranslate = (info->m_shadowTransformMode == ShadowIgnoresTransforms);

            SkPaint* paint = m_cachedDrawLooper->addLayerOnTop(skInfo);

            if (info->m_blur) {
                uint32_t mfFlags = SkBlurMaskFilter::kHighQuality_BlurFlag;
                if (info->m_shadowTransformMode == ShadowIgnoresTransforms)
                    mfFlags |= SkBlurMaskFilter::kIgnoreTransform_BlurFlag;
                RefPtr<SkMaskFilter> mf = adoptRef(SkBlurMaskFilter::Create(
                    (double)info->m_blur / 2.0, SkBlurMaskFilter::kNormal_BlurStyle, mfFlags));
                paint->setMaskFilter(mf.get());
            }

            RefPtr<SkColorFilter> cf = adoptRef(SkColorFilter::CreateModeFilter(skColor, SkXfermode::kSrcIn_Mode));
            paint->setColorFilter(cf.get());
        } else {
            // Unmodified layer
            SkLayerDrawLooper::LayerInfo skInfo;
            m_cachedDrawLooper->addLayerOnTop(skInfo);
        }
    }
}

void DrawLooper::buildCachedImageFilter() const
{
    ASSERT(m_layerInfo.size() == 2);
    ASSERT(m_layerInfo[0].m_layerType == ShadowLayer);
    ASSERT(m_layerInfo[1].m_layerType == UnmodifiedLayer);
    ASSERT(m_layerInfo[0].m_shadowAlphaMode == ShadowRespectsAlpha);
    ASSERT(m_layerInfo[0].m_shadowTransformMode == ShadowIgnoresTransforms);
    const float blurToSigmaFactor = 0.25;
    SkColor skColor = m_layerInfo[0].m_color.rgb();
    m_cachedImageFilter = adoptRef(new SkDropShadowImageFilter(m_layerInfo[0].m_offset.width(), m_layerInfo[0].m_offset.height(), m_layerInfo[0].m_blur * blurToSigmaFactor, skColor));
}

bool DrawLooper::shouldUseImageFilterToDrawBitmap(const SkBitmap& bitmap) const
{
    if (bitmap.isOpaque() || !m_layerInfo.size())
        return false;

#if !ASSERT_DISABLED
    // Verify that cases that require a mask filter to render correctly are of
    // a form that can be handled by DropShadowImageFilter.
    LayerVector::const_iterator info;
    int unmodifiedCount = 0;
    int shadowCount = 0;
    bool needsFilter = false;
    for (info = m_layerInfo.begin(); info < m_layerInfo.end(); ++info) {
        if (info->m_layerType == ShadowLayer) {
            needsFilter = needsFilter || (info->m_blur && info->m_shadowAlphaMode == ShadowRespectsAlpha);
            shadowCount++;
        } else {
            unmodifiedCount++;
        }

    }
    if (needsFilter) {
        // If any of the following assertions ever fire, it means that we are hitting
        // case that may not be rendered correclty by the DrawLooper and cannot be
        // handled by DropShadowImageFilter.
        ASSERT(shadowCount == 1);
        ASSERT(unmodifiedCount == 1);
        ASSERT(m_layerInfo[0].m_layerType == ShadowLayer);
        ASSERT(m_layerInfo[0].m_shadowTransformMode == ShadowIgnoresTransforms);
    }
#endif

    return m_layerInfo.size() == 2
        && m_layerInfo[0].m_layerType == ShadowLayer
        && m_layerInfo[0].m_shadowAlphaMode == ShadowRespectsAlpha
        && m_layerInfo[0].m_shadowTransformMode == ShadowIgnoresTransforms
        && m_layerInfo[0].m_blur
        && m_layerInfo[1].m_layerType == UnmodifiedLayer;
}

} // namespace WebCore

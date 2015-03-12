// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/html/canvas/CanvasRenderingContext2DState.h"

#include "core/css/CSSFontSelector.h"
#include "core/html/canvas/CanvasGradient.h"
#include "core/html/canvas/CanvasPattern.h"
#include "core/html/canvas/CanvasStyle.h"
#include "platform/graphics/skia/SkiaUtils.h"

static const char defaultFont[] = "10px sans-serif";

namespace blink {

CanvasRenderingContext2DState::CanvasRenderingContext2DState()
    : m_unrealizedSaveCount(0)
    , m_strokeStyle(CanvasStyle::createFromRGBA(Color::black))
    , m_fillStyle(CanvasStyle::createFromRGBA(Color::black))
    , m_lineWidth(1)
    , m_lineCap(ButtCap)
    , m_lineJoin(MiterJoin)
    , m_miterLimit(10)
    , m_shadowBlur(0)
    , m_shadowColor(Color::transparent)
    , m_globalAlpha(1)
    , m_globalComposite(SkXfermode::kSrcOver_Mode)
    , m_isTransformInvertible(true)
    , m_lineDashOffset(0)
    , m_imageSmoothingEnabled(true)
    , m_textAlign(StartTextAlign)
    , m_textBaseline(AlphabeticTextBaseline)
    , m_direction(DirectionInherit)
    , m_unparsedFont(defaultFont)
    , m_realizedFont(false)
    , m_hasClip(false)
    , m_hasComplexClip(false)
{
}

CanvasRenderingContext2DState::CanvasRenderingContext2DState(const CanvasRenderingContext2DState& other, ClipListCopyMode mode)
    : CSSFontSelectorClient()
    , m_unrealizedSaveCount(other.m_unrealizedSaveCount)
    , m_unparsedStrokeColor(other.m_unparsedStrokeColor)
    , m_unparsedFillColor(other.m_unparsedFillColor)
    , m_strokeStyle(other.m_strokeStyle)
    , m_fillStyle(other.m_fillStyle)
    , m_lineWidth(other.m_lineWidth)
    , m_lineCap(other.m_lineCap)
    , m_lineJoin(other.m_lineJoin)
    , m_miterLimit(other.m_miterLimit)
    , m_shadowOffset(other.m_shadowOffset)
    , m_shadowBlur(other.m_shadowBlur)
    , m_shadowColor(other.m_shadowColor)
    , m_globalAlpha(other.m_globalAlpha)
    , m_globalComposite(other.m_globalComposite)
    , m_transform(other.m_transform)
    , m_isTransformInvertible(other.m_isTransformInvertible)
    , m_lineDashOffset(other.m_lineDashOffset)
    , m_imageSmoothingEnabled(other.m_imageSmoothingEnabled)
    , m_textAlign(other.m_textAlign)
    , m_textBaseline(other.m_textBaseline)
    , m_direction(other.m_direction)
    , m_unparsedFont(other.m_unparsedFont)
    , m_font(other.m_font)
    , m_realizedFont(other.m_realizedFont)
    , m_hasClip(other.m_hasClip)
    , m_hasComplexClip(other.m_hasComplexClip)
{
    if (mode == CopyClipList) {
        m_clipList = other.m_clipList;
    }
    if (m_realizedFont)
        static_cast<CSSFontSelector*>(m_font.fontSelector())->registerForInvalidationCallbacks(this);
}

CanvasRenderingContext2DState& CanvasRenderingContext2DState::operator=(const CanvasRenderingContext2DState& other)
{
    if (this == &other)
        return *this;

#if !ENABLE(OILPAN)
    if (m_realizedFont)
        static_cast<CSSFontSelector*>(m_font.fontSelector())->unregisterForInvalidationCallbacks(this);
#endif

    m_unrealizedSaveCount = other.m_unrealizedSaveCount;
    m_unparsedStrokeColor = other.m_unparsedStrokeColor;
    m_unparsedFillColor = other.m_unparsedFillColor;
    m_strokeStyle = other.m_strokeStyle;
    m_fillStyle = other.m_fillStyle;
    m_lineWidth = other.m_lineWidth;
    m_lineCap = other.m_lineCap;
    m_lineJoin = other.m_lineJoin;
    m_miterLimit = other.m_miterLimit;
    m_shadowOffset = other.m_shadowOffset;
    m_shadowBlur = other.m_shadowBlur;
    m_shadowColor = other.m_shadowColor;
    m_globalAlpha = other.m_globalAlpha;
    m_globalComposite = other.m_globalComposite;
    m_transform = other.m_transform;
    m_isTransformInvertible = other.m_isTransformInvertible;
    m_imageSmoothingEnabled = other.m_imageSmoothingEnabled;
    m_textAlign = other.m_textAlign;
    m_textBaseline = other.m_textBaseline;
    m_direction = other.m_direction;
    m_unparsedFont = other.m_unparsedFont;
    m_font = other.m_font;
    m_realizedFont = other.m_realizedFont;
    m_hasClip = other.m_hasClip;
    m_hasComplexClip = other.m_hasComplexClip;
    m_clipList = other.m_clipList;

    if (m_realizedFont)
        static_cast<CSSFontSelector*>(m_font.fontSelector())->registerForInvalidationCallbacks(this);

    return *this;
}

CanvasRenderingContext2DState::~CanvasRenderingContext2DState()
{
#if !ENABLE(OILPAN)
    if (m_realizedFont)
        static_cast<CSSFontSelector*>(m_font.fontSelector())->unregisterForInvalidationCallbacks(this);
#endif
}

void CanvasRenderingContext2DState::fontsNeedUpdate(CSSFontSelector* fontSelector)
{
    ASSERT_ARG(fontSelector, fontSelector == m_font.fontSelector());
    ASSERT(m_realizedFont);

    m_font.update(fontSelector);
}

DEFINE_TRACE(CanvasRenderingContext2DState)
{
    visitor->trace(m_strokeStyle);
    visitor->trace(m_fillStyle);
    CSSFontSelectorClient::trace(visitor);
}

void CanvasRenderingContext2DState::setLineDash(const Vector<float>& dash)
{
    m_lineDash = dash;
    // Spec requires the concatenation of two copies the dash list when the
    // number of elements is odd
    if (dash.size() % 2)
        m_lineDash.appendVector(dash);
}

void CanvasRenderingContext2DState::clipPath(const SkPath& path, AntiAliasingMode antiAliasingMode)
{
    m_clipList.clipPath(path, antiAliasingMode, affineTransformToSkMatrix(m_transform));
    m_hasClip = true;
    if (!path.isRect(0))
        m_hasComplexClip = true;
}

void CanvasRenderingContext2DState::setFont(const Font& font, CSSFontSelector* selector)
{
#if !ENABLE(OILPAN)
    if (m_realizedFont)
        static_cast<CSSFontSelector*>(m_font.fontSelector())->unregisterForInvalidationCallbacks(this);
#endif
    m_font = font;
    m_font.update(selector);
    m_realizedFont = true;
    selector->registerForInvalidationCallbacks(this);
}

const Font& CanvasRenderingContext2DState::font() const
{
    ASSERT(m_realizedFont);
    return m_font;
}

void CanvasRenderingContext2DState::setTransform(const AffineTransform& transform)
{
    m_isTransformInvertible = transform.isInvertible();
    m_transform = transform;
}

void CanvasRenderingContext2DState::resetTransform()
{
    m_transform.makeIdentity();
    m_isTransformInvertible = true;
}

} // blink

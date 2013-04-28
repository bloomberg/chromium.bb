/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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

#include "config.h"
#include "core/platform/graphics/Gradient.h"

#include "SkColorShader.h"
#include "SkGradientShader.h"
#include "core/css/CSSParser.h"
#include "core/platform/graphics/Color.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/skia/SkiaUtils.h"
#include <wtf/HashFunctions.h>
#include <wtf/StringHasher.h>
#include <wtf/UnusedParam.h>

using WTF::pairIntHash;

namespace WebCore {

Gradient::Gradient(const FloatPoint& p0, const FloatPoint& p1)
    : m_radial(false)
    , m_p0(p0)
    , m_p1(p1)
    , m_r0(0)
    , m_r1(0)
    , m_aspectRatio(1)
    , m_stopsSorted(false)
    , m_lastStop(0)
    , m_spreadMethod(SpreadMethodPad)
    , m_cachedHash(0)
    , m_gradient(0)
{
}

Gradient::Gradient(const FloatPoint& p0, float r0, const FloatPoint& p1, float r1, float aspectRatio)
    : m_radial(true)
    , m_p0(p0)
    , m_p1(p1)
    , m_r0(r0)
    , m_r1(r1)
    , m_aspectRatio(aspectRatio)
    , m_stopsSorted(false)
    , m_lastStop(0)
    , m_spreadMethod(SpreadMethodPad)
    , m_cachedHash(0)
    , m_gradient(0)
{
}

Gradient::~Gradient()
{
    destroyShader();
}

void Gradient::destroyShader()
{
    SkSafeUnref(m_gradient);
    m_gradient = 0;
}

void Gradient::adjustParametersForTiledDrawing(IntSize& size, FloatRect& srcRect)
{
    if (m_radial)
        return;

    if (srcRect.isEmpty())
        return;

    if (m_p0.x() == m_p1.x()) {
        size.setWidth(1);
        srcRect.setWidth(1);
        srcRect.setX(0);
        return;
    }
    if (m_p0.y() != m_p1.y())
        return;

    size.setHeight(1);
    srcRect.setHeight(1);
    srcRect.setY(0);
}

void Gradient::addColorStop(float value, const Color& color)
{
    float r;
    float g;
    float b;
    float a;
    color.getRGBA(r, g, b, a);
    m_stops.append(ColorStop(value, r, g, b, a));

    m_stopsSorted = false;
    destroyShader();

    invalidateHash();
}

void Gradient::addColorStop(const Gradient::ColorStop& stop)
{
    m_stops.append(stop);

    m_stopsSorted = false;
    destroyShader();

    invalidateHash();
}

static inline bool compareStops(const Gradient::ColorStop& a, const Gradient::ColorStop& b)
{
    return a.stop < b.stop;
}

void Gradient::sortStopsIfNecessary()
{
    if (m_stopsSorted)
        return;

    m_stopsSorted = true;

    if (!m_stops.size())
        return;

    std::stable_sort(m_stops.begin(), m_stops.end(), compareStops);

    invalidateHash();
}

void Gradient::getColor(float value, float* r, float* g, float* b, float* a) const
{
    ASSERT(value >= 0);
    ASSERT(value <= 1);

    if (m_stops.isEmpty()) {
        *r = 0;
        *g = 0;
        *b = 0;
        *a = 0;
        return;
    }
    if (!m_stopsSorted) {
        if (m_stops.size())
            std::stable_sort(m_stops.begin(), m_stops.end(), compareStops);
        m_stopsSorted = true;
    }
    if (value <= 0 || value <= m_stops.first().stop) {
        *r = m_stops.first().red;
        *g = m_stops.first().green;
        *b = m_stops.first().blue;
        *a = m_stops.first().alpha;
        return;
    }
    if (value >= 1 || value >= m_stops.last().stop) {
        *r = m_stops.last().red;
        *g = m_stops.last().green;
        *b = m_stops.last().blue;
        *a = m_stops.last().alpha;
        return;
    }

    // Find stop before and stop after and interpolate.
    int stop = findStop(value);
    const ColorStop& lastStop = m_stops[stop];    
    const ColorStop& nextStop = m_stops[stop + 1];
    float stopFraction = (value - lastStop.stop) / (nextStop.stop - lastStop.stop);
    *r = lastStop.red + (nextStop.red - lastStop.red) * stopFraction;
    *g = lastStop.green + (nextStop.green - lastStop.green) * stopFraction;
    *b = lastStop.blue + (nextStop.blue - lastStop.blue) * stopFraction;
    *a = lastStop.alpha + (nextStop.alpha - lastStop.alpha) * stopFraction;
}

int Gradient::findStop(float value) const
{
    ASSERT(value >= 0);
    ASSERT(value <= 1);
    ASSERT(m_stopsSorted);

    int numStops = m_stops.size();
    ASSERT(numStops >= 2);
    ASSERT(m_lastStop < numStops - 1);

    int i = m_lastStop;
    if (value < m_stops[i].stop)
        i = 1;
    else
        i = m_lastStop + 1;

    for (; i < numStops - 1; ++i)
        if (value < m_stops[i].stop)
            break;

    m_lastStop = i - 1;
    return m_lastStop;
}

bool Gradient::hasAlpha() const
{
    for (size_t i = 0; i < m_stops.size(); i++) {
        if (m_stops[i].alpha < 1)
            return true;
    }

    return false;
}

void Gradient::setSpreadMethod(GradientSpreadMethod spreadMethod)
{
    // FIXME: Should it become necessary, allow calls to this method after m_gradient has been set.
    ASSERT(m_gradient == 0);

    if (m_spreadMethod == spreadMethod)
        return;

    m_spreadMethod = spreadMethod;

    invalidateHash();
}

void Gradient::setGradientSpaceTransform(const AffineTransform& gradientSpaceTransformation)
{
    if (m_gradientSpaceTransformation == gradientSpaceTransformation)
        return;

    m_gradientSpaceTransformation = gradientSpaceTransformation;
    if (m_gradient)
        m_gradient->setLocalMatrix(m_gradientSpaceTransformation);

    invalidateHash();
}

unsigned Gradient::hash() const
{
    if (m_cachedHash)
        return m_cachedHash;

    struct {
        AffineTransform gradientSpaceTransformation;
        FloatPoint p0;
        FloatPoint p1;
        float r0;
        float r1;
        float aspectRatio;
        int lastStop;
        GradientSpreadMethod spreadMethod;
        bool radial;
    } parameters;

    // StringHasher requires that the memory it hashes be a multiple of two in size.
    COMPILE_ASSERT(!(sizeof(parameters) % 2), Gradient_parameters_size_should_be_multiple_of_two);
    COMPILE_ASSERT(!(sizeof(ColorStop) % 2), Color_stop_size_should_be_multiple_of_two);
    
    // Ensure that any padding in the struct is zero-filled, so it will not affect the hash value.
    memset(&parameters, 0, sizeof(parameters));
    
    parameters.gradientSpaceTransformation = m_gradientSpaceTransformation;
    parameters.p0 = m_p0;
    parameters.p1 = m_p1;
    parameters.r0 = m_r0;
    parameters.r1 = m_r1;
    parameters.aspectRatio = m_aspectRatio;
    parameters.lastStop = m_lastStop;
    parameters.spreadMethod = m_spreadMethod;
    parameters.radial = m_radial;

    unsigned parametersHash = StringHasher::hashMemory(&parameters, sizeof(parameters));
    unsigned stopHash = StringHasher::hashMemory(m_stops.data(), m_stops.size() * sizeof(ColorStop));

    m_cachedHash = pairIntHash(parametersHash, stopHash);

    return m_cachedHash;
}

static inline U8CPU F2B(float x)
{
    return static_cast<int>(x * 255);
}

static SkColor makeSkColor(float a, float r, float g, float b)
{
    return SkColorSetARGB(F2B(a), F2B(r), F2B(g), F2B(b));
}

// Determine the total number of stops needed, including pseudo-stops at the
// ends as necessary.
static size_t totalStopsNeeded(const Gradient::ColorStop* stopData, size_t count)
{
    // N.B.: The tests in this function should kept in sync with the ones in
    // fillStops(), or badness happens.
    const Gradient::ColorStop* stop = stopData;
    size_t countUsed = count;
    if (count < 1 || stop->stop > 0.0)
        countUsed++;
    stop += count - 1;
    if (count < 1 || stop->stop < 1.0)
        countUsed++;
    return countUsed;
}

// Collect sorted stop position and color information into the pos and colors
// buffers, ensuring stops at both 0.0 and 1.0. The buffers must be large
// enough to hold information for all stops, including the new endpoints if
// stops at 0.0 and 1.0 aren't already included.
static void fillStops(const Gradient::ColorStop* stopData,
    size_t count, SkScalar* pos, SkColor* colors)
{
    const Gradient::ColorStop* stop = stopData;
    size_t start = 0;
    if (count < 1) {
        // A gradient with no stops must be transparent black.
        pos[0] = WebCoreFloatToSkScalar(0.0);
        colors[0] = makeSkColor(0.0, 0.0, 0.0, 0.0);
        start = 1;
    } else if (stop->stop > 0.0) {
        // Copy the first stop to 0.0. The first stop position may have a slight
        // rounding error, but we don't care in this float comparison, since
        // 0.0 comes through cleanly and people aren't likely to want a gradient
        // with a stop at (0 + epsilon).
        pos[0] = WebCoreFloatToSkScalar(0.0);
        colors[0] = makeSkColor(stop->alpha, stop->red, stop->green, stop->blue);
        start = 1;
    }

    for (size_t i = start; i < start + count; i++) {
        pos[i] = WebCoreFloatToSkScalar(stop->stop);
        colors[i] = makeSkColor(stop->alpha, stop->red, stop->green, stop->blue);
        ++stop;
    }

    // Copy the last stop to 1.0 if needed. See comment above about this float
    // comparison.
    if (count < 1 || (--stop)->stop < 1.0) {
        pos[start + count] = WebCoreFloatToSkScalar(1.0);
        colors[start + count] = colors[start + count - 1];
    }
}

SkShader* Gradient::shader()
{
    if (m_gradient)
        return m_gradient;

    sortStopsIfNecessary();
    ASSERT(m_stopsSorted);

    size_t countUsed = totalStopsNeeded(m_stops.data(), m_stops.size());
    ASSERT(countUsed >= 2);
    ASSERT(countUsed >= m_stops.size());

    // FIXME: Why is all this manual pointer math needed?!
    SkAutoMalloc storage(countUsed * (sizeof(SkColor) + sizeof(SkScalar)));
    SkColor* colors = (SkColor*)storage.get();
    SkScalar* pos = (SkScalar*)(colors + countUsed);

    fillStops(m_stops.data(), m_stops.size(), pos, colors);

    SkShader::TileMode tile = SkShader::kClamp_TileMode;
    switch (m_spreadMethod) {
    case SpreadMethodReflect:
        tile = SkShader::kMirror_TileMode;
        break;
    case SpreadMethodRepeat:
        tile = SkShader::kRepeat_TileMode;
        break;
    case SpreadMethodPad:
        tile = SkShader::kClamp_TileMode;
        break;
    }

    if (m_radial) {
        // Since the two-point radial gradient is slower than the plain radial,
        // only use it if we have to.
        if (m_p0 == m_p1 && m_r0 <= 0.0f)
            m_gradient = SkGradientShader::CreateRadial(m_p1, m_r1, colors, pos, static_cast<int>(countUsed), tile);
        else {
            // The radii we give to Skia must be positive. If we're given a
            // negative radius, ask for zero instead.
            SkScalar radius0 = m_r0 >= 0.0f ? WebCoreFloatToSkScalar(m_r0) : 0;
            SkScalar radius1 = m_r1 >= 0.0f ? WebCoreFloatToSkScalar(m_r1) : 0;
            m_gradient = SkGradientShader::CreateTwoPointConical(m_p0, radius0, m_p1, radius1, colors, pos, static_cast<int>(countUsed), tile);
        }

        if (aspectRatio() != 1) {
            // CSS3 elliptical gradients: apply the elliptical scaling at the
            // gradient center point.
            m_gradientSpaceTransformation.translate(m_p0.x(), m_p0.y());
            m_gradientSpaceTransformation.scale(1, 1 / aspectRatio());
            m_gradientSpaceTransformation.translate(-m_p0.x(), -m_p0.y());
            ASSERT(m_p0 == m_p1);
        }
    } else {
        SkPoint pts[2] = { m_p0, m_p1 };
        m_gradient = SkGradientShader::CreateLinear(pts, colors, pos, static_cast<int>(countUsed), tile);
    }

    if (!m_gradient)
        // use last color, since our "geometry" was degenerate (e.g. radius==0)
        m_gradient = new SkColorShader(colors[countUsed - 1]);
    else
        m_gradient->setLocalMatrix(m_gradientSpaceTransformation);
    return m_gradient;
}

void Gradient::fill(GraphicsContext* context, const FloatRect& rect)
{
    context->setFillGradient(this);
    context->fillRect(rect);
}

} //namespace

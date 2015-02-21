/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TopControls.h"

#include "core/frame/FrameHost.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "platform/geometry/FloatSize.h"
#include <algorithm> // for std::min and std::max

namespace blink {

TopControls::TopControls(const FrameHost& frameHost)
    : m_frameHost(frameHost)
    , m_height(0)
    , m_shownRatio(0)
    , m_baselineContentOffset(0)
    , m_accumulatedScrollDelta(0)
    , m_shrinkViewport(false)
    , m_permittedState(WebTopControlsBoth)
{
}

void TopControls::scrollBegin()
{
    resetBaseline();
}

FloatSize TopControls::scrollBy(FloatSize pendingDelta)
{
    if ((m_permittedState == WebTopControlsShown && pendingDelta.height() < 0) || (m_permittedState == WebTopControlsHidden && pendingDelta.height() > 0))
        return pendingDelta;

    if (m_height == 0)
        return pendingDelta;

    float oldOffset = contentOffset();

    // Update accumulated vertical scroll and apply it to top controls
    m_accumulatedScrollDelta += pendingDelta.height();
    setShownRatio((m_baselineContentOffset + m_accumulatedScrollDelta) / m_height);

    // Reset baseline when controls are fully visible
    if (m_shownRatio == 1)
        resetBaseline();

    FloatSize appliedDelta(0, contentOffset() - oldOffset);
    return pendingDelta - appliedDelta;
}

void TopControls::resetBaseline()
{
    m_accumulatedScrollDelta = 0;
    m_baselineContentOffset = contentOffset();
}

float TopControls::layoutHeight()
{
    return m_shrinkViewport ? m_height : 0;
}

float TopControls::contentOffset()
{
    return m_shownRatio * m_height;
}

void TopControls::setShownRatio(float shownRatio)
{
    shownRatio = std::min(shownRatio, 1.f);
    shownRatio = std::max(shownRatio, 0.f);

    if (m_shownRatio == shownRatio)
        return;

    m_shownRatio = shownRatio;
    m_frameHost.chrome().client().didUpdateTopControls();
}

void TopControls::updateConstraints(WebTopControlsState constraints)
{
    m_permittedState = constraints;
}

void TopControls::setHeight(float height, bool shrinkViewport)
{
    if (m_height == height && m_shrinkViewport == shrinkViewport)
        return;

    m_height = height;
    m_shrinkViewport = shrinkViewport;
    m_frameHost.chrome().client().didUpdateTopControls();
}

} // namespace blink

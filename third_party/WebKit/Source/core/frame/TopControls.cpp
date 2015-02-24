// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "TopControls.h"

#include "core/frame/FrameHost.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "platform/geometry/FloatSize.h"
#include <algorithm> // for std::min and std::max

namespace blink {

TopControls::TopControls(const FrameHost& frameHost)
    : m_frameHost(&frameHost)
    , m_height(0)
    , m_shownRatio(0)
    , m_baselineContentOffset(0)
    , m_accumulatedScrollDelta(0)
    , m_shrinkViewport(false)
    , m_permittedState(WebTopControlsBoth)
{
}

TopControls::~TopControls()
{
}

DEFINE_TRACE(TopControls)
{
    visitor->trace(m_frameHost);
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
    m_frameHost->chrome().client().didUpdateTopControls();
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
    m_frameHost->chrome().client().didUpdateTopControls();
}

} // namespace blink

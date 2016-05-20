// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorAnimationHost.h"

#include "cc/animation/scroll_offset_animations.h"

namespace blink {

CompositorAnimationHost::CompositorAnimationHost(cc::AnimationHost* host)
    : m_animationHost(host) {}

bool CompositorAnimationHost::isNull() const
{
    return !m_animationHost;
}

void CompositorAnimationHost::updateImplOnlyScrollOffsetAnimation(const gfx::Vector2dF& adjustment, cc::ElementId elementId)
{
    if (!m_animationHost)
        return;

    cc::ScrollOffsetAnimationUpdate update(cc::ScrollOffsetAnimationUpdate::Type::SCROLL_OFFSET_CHANGED, elementId);
    update.adjustment_ = adjustment;
    m_animationHost->scroll_offset_animations().AddUpdate(update);
}

} // namespace blink

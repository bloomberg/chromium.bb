// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/WebToCCAnimationDelegateAdapter.h"

#include "public/platform/WebCompositorAnimationDelegate.h"

namespace blink {

WebToCCAnimationDelegateAdapter::WebToCCAnimationDelegateAdapter(
    blink::WebCompositorAnimationDelegate* delegate)
    : m_delegate(delegate)
{
}

void WebToCCAnimationDelegateAdapter::NotifyAnimationStarted(
    base::TimeTicks monotonicTime,
    cc::Animation::TargetProperty targetProperty,
    int group)
{
    m_delegate->notifyAnimationStarted(
        (monotonicTime - base::TimeTicks()).InSecondsF(),
        group);
}

void WebToCCAnimationDelegateAdapter::NotifyAnimationFinished(
    base::TimeTicks monotonicTime,
    cc::Animation::TargetProperty targetProperty,
    int group)
{
    m_delegate->notifyAnimationFinished(
        (monotonicTime - base::TimeTicks()).InSecondsF(),
        group);
}

void WebToCCAnimationDelegateAdapter::NotifyAnimationAborted(
    base::TimeTicks monotonicTime,
    cc::Animation::TargetProperty targetProperty,
    int group)
{
    m_delegate->notifyAnimationAborted((monotonicTime - base::TimeTicks()).InSecondsF(), group);
}

} // namespace blink

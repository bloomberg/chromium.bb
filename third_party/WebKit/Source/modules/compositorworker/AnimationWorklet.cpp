// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimationWorklet.h"

#include "bindings/core/v8/V8Binding.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "modules/compositorworker/AnimationWorkletMessagingProxy.h"
#include "modules/compositorworker/AnimationWorkletThread.h"

namespace blink {

// static
AnimationWorklet* AnimationWorklet::create(LocalFrame* frame)
{
    AnimationWorkletThread::ensureSharedBackingThread();
    AnimationWorklet* worklet = new AnimationWorklet(frame);
    worklet->suspendIfNeeded();
    return worklet;
}

AnimationWorklet::AnimationWorklet(LocalFrame* frame)
    : Worklet(frame)
    , m_workletMessagingProxy(new AnimationWorkletMessagingProxy(frame->document()))
{
    m_workletMessagingProxy->initialize();
}

AnimationWorklet::~AnimationWorklet()
{
    m_workletMessagingProxy->parentObjectDestroyed();
}

WorkletGlobalScopeProxy* AnimationWorklet::workletGlobalScopeProxy() const
{
    return m_workletMessagingProxy;
}

DEFINE_TRACE(AnimationWorklet)
{
    Worklet::trace(visitor);
}

} // namespace blink

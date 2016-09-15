// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/compositorworker/AnimationWorklet.h"

#include "bindings/core/v8/V8Binding.h"
#include "core/frame/LocalFrame.h"
#include "core/workers/ThreadedWorkletGlobalScopeProxy.h"

namespace blink {

// static
AnimationWorklet* AnimationWorklet::create(LocalFrame* frame)
{
    AnimationWorklet* worklet = new AnimationWorklet(frame);
    worklet->suspendIfNeeded();
    return worklet;
}

AnimationWorklet::AnimationWorklet(LocalFrame* frame)
    : Worklet(frame)
    , m_workletGlobalScopeProxy(new ThreadedWorkletGlobalScopeProxy())
{
}

AnimationWorklet::~AnimationWorklet()
{
}

WorkletGlobalScopeProxy* AnimationWorklet::workletGlobalScopeProxy() const
{
    return m_workletGlobalScopeProxy.get();
}

DEFINE_TRACE(AnimationWorklet)
{
    Worklet::trace(visitor);
}

} // namespace blink

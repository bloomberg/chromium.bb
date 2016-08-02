// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationWorklet_h
#define AnimationWorklet_h

#include "modules/ModulesExport.h"
#include "modules/worklet/Worklet.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalFrame;
class ThreadedWorkletGlobalScopeProxy;
class WorkletGlobalScopeProxy;

class MODULES_EXPORT AnimationWorklet final : public Worklet {
    WTF_MAKE_NONCOPYABLE(AnimationWorklet);
public:
    static AnimationWorklet* create(LocalFrame*);
    ~AnimationWorklet() override;

    WorkletGlobalScopeProxy* workletGlobalScopeProxy() const final;

    DECLARE_VIRTUAL_TRACE();

private:
    explicit AnimationWorklet(LocalFrame*);

    // TODO(ikilpatrick): this will change to a raw ptr once we have a thread.
    std::unique_ptr<ThreadedWorkletGlobalScopeProxy> m_workletGlobalScopeProxy;
};

} // namespace blink

#endif // AnimationWorklet_h

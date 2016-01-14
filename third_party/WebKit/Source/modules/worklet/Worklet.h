// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Worklet_h
#define Worklet_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/worklet/WorkletGlobalScope.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExecutionContext;

class Worklet final : public GarbageCollectedFinalized<Worklet>, public ScriptWrappable {
    WTF_MAKE_NONCOPYABLE(Worklet);
    DEFINE_WRAPPERTYPEINFO();
public:
    // The ExecutionContext argument is the parent document of the Worklet. The
    // Worklet inherits the url and userAgent, from the document.
    static Worklet* create(ExecutionContext*);

    DECLARE_TRACE();

private:
    explicit Worklet(ExecutionContext*);

    RefPtrWillBeMember<WorkletGlobalScope> m_workletGlobalScope;
};

} // namespace blink

#endif // Worklet_h

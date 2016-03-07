// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Worklet_h
#define Worklet_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/workers/WorkerScriptLoader.h"
#include "modules/worklet/WorkletGlobalScope.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExecutionContext;
class ScriptPromiseResolver;
class WorkerScriptLoader;

class Worklet final : public GarbageCollectedFinalized<Worklet>, public ScriptWrappable, public ActiveDOMObject {
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(Worklet);
    WTF_MAKE_NONCOPYABLE(Worklet);
public:
    // The ExecutionContext argument is the parent document of the Worklet. The
    // Worklet inherits the url and userAgent, from the document.
    static Worklet* create(LocalFrame*, ExecutionContext*);

    ScriptPromise import(ScriptState*, const String& url);

    // ActiveDOMObject
    void stop() final;

    DECLARE_TRACE();

private:
    Worklet(LocalFrame*, ExecutionContext*);

    void onResponse();
    void onFinished(WorkerScriptLoader*, ScriptPromiseResolver*);

    RefPtrWillBeMember<WorkletGlobalScope> m_workletGlobalScope;
    Vector<RefPtr<WorkerScriptLoader>> m_scriptLoaders;
    HeapVector<Member<ScriptPromiseResolver>> m_resolvers;
};

} // namespace blink

#endif // Worklet_h

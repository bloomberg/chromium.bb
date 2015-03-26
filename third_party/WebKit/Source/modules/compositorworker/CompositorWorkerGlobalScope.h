// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorWorkerGlobalScope_h
#define CompositorWorkerGlobalScope_h

#include "core/dom/MessagePort.h"
#include "core/workers/WorkerGlobalScope.h"

namespace blink {

class CompositorWorkerThread;
class WorkerThreadStartupData;

class CompositorWorkerGlobalScope final : public WorkerGlobalScope {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<CompositorWorkerGlobalScope> create(CompositorWorkerThread*, PassOwnPtrWillBeRawPtr<WorkerThreadStartupData>, double timeOrigin);
    virtual ~CompositorWorkerGlobalScope();

    // EventTarget
    const AtomicString& interfaceName() const override;

    void postMessage(ExecutionContext*, PassRefPtr<SerializedScriptValue>, const MessagePortArray*, ExceptionState&);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(message);

    void executeCompositorFrameCallbacks(double monotonicTimeNow);

    // ExecutionContext:
    bool isCompositorWorkerGlobalScope() const override { return true; }

private:
    CompositorWorkerGlobalScope(const KURL&, const String& userAgent, CompositorWorkerThread*, double timeOrigin, const SecurityOrigin*, PassOwnPtrWillBeRawPtr<WorkerClients>);
    CompositorWorkerThread* thread() const;
};

} // namespace blink

#endif // CompositorWorkerGlobalScope_h

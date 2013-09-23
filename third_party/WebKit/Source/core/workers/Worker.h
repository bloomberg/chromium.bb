/*
 * Copyright (C) 2008, 2010 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef Worker_h
#define Worker_h

#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventListener.h"
#include "core/events/EventNames.h"
#include "core/events/EventTarget.h"
#include "core/dom/MessagePort.h"
#include "core/workers/AbstractWorker.h"
#include "core/workers/WorkerScriptLoaderClient.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/AtomicStringHash.h"

namespace WebCore {

class ExceptionState;
class ScriptExecutionContext;
class WorkerGlobalScopeProxy;
class WorkerScriptLoader;

class Worker : public AbstractWorker, public ScriptWrappable, private WorkerScriptLoaderClient {
public:
    static PassRefPtr<Worker> create(ScriptExecutionContext*, const String& url, ExceptionState&);
    virtual ~Worker();

    virtual const AtomicString& interfaceName() const OVERRIDE;

    void postMessage(PassRefPtr<SerializedScriptValue> message, const MessagePortArray*, ExceptionState&);

    void terminate();

    virtual bool canSuspend() const OVERRIDE;
    virtual void stop() OVERRIDE;
    virtual bool hasPendingActivity() const OVERRIDE;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(message);

private:
    explicit Worker(ScriptExecutionContext*);

    // WorkerScriptLoaderClient callbacks
    virtual void didReceiveResponse(unsigned long identifier, const ResourceResponse&) OVERRIDE;
    virtual void notifyFinished() OVERRIDE;

    virtual void refEventTarget() OVERRIDE { ref(); }
    virtual void derefEventTarget() OVERRIDE { deref(); }

    RefPtr<WorkerScriptLoader> m_scriptLoader;
    WorkerGlobalScopeProxy* m_contextProxy; // The proxy outlives the worker to perform thread shutdown.
};

} // namespace WebCore

#endif // Worker_h

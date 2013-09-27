/*
 * Copyright (C) 2007-2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "bindings/v8/ScheduledAction.h"

#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8GCController.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "core/dom/Document.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/page/Frame.h"
#include "core/platform/chromium/TraceEvent.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"

namespace WebCore {

ScheduledAction::ScheduledAction(v8::Handle<v8::Context> context, v8::Handle<v8::Function> function, int argc, v8::Handle<v8::Value> argv[], v8::Isolate* isolate)
    : m_context(isolate, context)
    , m_function(isolate, function)
    , m_code(String(), KURL(), TextPosition::belowRangePosition())
    , m_isolate(isolate)
{
    m_args.reserveCapacity(argc);
    for (int i = 0; i < argc; ++i)
        m_args.append(UnsafePersistent<v8::Value>(m_isolate, argv[i]));
}

ScheduledAction::ScheduledAction(v8::Handle<v8::Context> context, const String& code, const KURL& url, v8::Isolate* isolate)
    : m_context(isolate, context)
    , m_code(code, url)
    , m_isolate(isolate)
{
}

ScheduledAction::~ScheduledAction()
{
    for (size_t i = 0; i < m_args.size(); ++i)
        m_args[i].dispose();
}

void ScheduledAction::execute(ScriptExecutionContext* context)
{
    if (context->isDocument()) {
        Frame* frame = toDocument(context)->frame();
        if (!frame)
            return;
        if (!frame->script()->canExecuteScripts(AboutToExecuteScript))
            return;
        execute(frame);
    } else {
        execute(toWorkerGlobalScope(context));
    }
}

void ScheduledAction::execute(Frame* frame)
{
    v8::HandleScope handleScope(m_isolate);

    v8::Handle<v8::Context> context = m_context.newLocal(m_isolate);
    if (context.IsEmpty())
        return;

    TRACE_EVENT0("v8", "ScheduledAction::execute");

    if (!m_function.isEmpty()) {
        v8::Context::Scope scope(context);
        Vector<v8::Handle<v8::Value> > args;
        createLocalHandlesForArgs(&args);
        frame->script()->callFunction(m_function.newLocal(m_isolate), context->Global(), args.size(), args.data());
    } else {
        frame->script()->executeScriptAndReturnValue(context, ScriptSourceCode(m_code));
    }

    // The frame might be invalid at this point because JavaScript could have released it.
}

void ScheduledAction::execute(WorkerGlobalScope* worker)
{
    ASSERT(worker->thread()->isCurrentThread());
    v8::HandleScope handleScope(m_isolate);
    v8::Handle<v8::Context> context = m_context.newLocal(m_isolate);
    ASSERT(!context.IsEmpty());
    v8::Context::Scope scope(context);
    if (!m_function.isEmpty()) {
        Vector<v8::Handle<v8::Value> > args;
        createLocalHandlesForArgs(&args);
        V8ScriptRunner::callFunction(m_function.newLocal(m_isolate), worker, context->Global(), args.size(), args.data(), m_isolate);
    } else
        worker->script()->evaluate(m_code);
}

void ScheduledAction::createLocalHandlesForArgs(Vector<v8::Handle<v8::Value> >* handles)
{
    handles->reserveCapacity(m_args.size());
    for (size_t i = 0; i < m_args.size(); ++i)
        handles->append(m_args[i].newLocal(m_isolate));
}

} // namespace WebCore

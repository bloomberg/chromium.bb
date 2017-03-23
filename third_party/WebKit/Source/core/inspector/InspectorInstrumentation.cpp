/*
* Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "core/inspector/InspectorInstrumentation.h"

#include "core/InspectorInstrumentationAgents.h"
#include "core/events/Event.h"
#include "core/events/EventTarget.h"
#include "core/frame/FrameHost.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorDOMDebuggerAgent.h"
#include "core/inspector/InspectorNetworkAgent.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorSession.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/inspector/ThreadDebugger.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/page/Page.h"
#include "core/workers/MainThreadWorkletGlobalScope.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/loader/fetch/FetchInitiatorInfo.h"

namespace blink {

namespace probe {

double ProbeBase::captureStartTime() const {
  if (!m_startTime)
    m_startTime = monotonicallyIncreasingTime();
  return m_startTime;
}

double ProbeBase::captureEndTime() const {
  if (!m_endTime)
    m_endTime = monotonicallyIncreasingTime();
  return m_endTime;
}

double ProbeBase::duration() const {
  DCHECK(m_startTime);
  return captureEndTime() - m_startTime;
}

AsyncTask::AsyncTask(ExecutionContext* context,
                     void* task,
                     const char* step,
                     bool enabled)
    : m_debugger(enabled ? ThreadDebugger::from(toIsolate(context)) : nullptr),
      m_task(task),
      m_recurring(step) {
  if (m_recurring) {
    TRACE_EVENT_FLOW_STEP0("devtools.timeline.async", "AsyncTask",
                           TRACE_ID_LOCAL(reinterpret_cast<uintptr_t>(task)),
                           step ? step : "");
  } else {
    TRACE_EVENT_FLOW_END0("devtools.timeline.async", "AsyncTask",
                          TRACE_ID_LOCAL(reinterpret_cast<uintptr_t>(task)));
  }
  if (m_debugger)
    m_debugger->asyncTaskStarted(m_task);
}

AsyncTask::~AsyncTask() {
  if (m_debugger) {
    m_debugger->asyncTaskFinished(m_task);
    if (!m_recurring)
      m_debugger->asyncTaskCanceled(m_task);
  }
}

void asyncTaskScheduled(ExecutionContext* context,
                        const String& name,
                        void* task) {
  TRACE_EVENT_FLOW_BEGIN1("devtools.timeline.async", "AsyncTask",
                          TRACE_ID_LOCAL(reinterpret_cast<uintptr_t>(task)),
                          "data", InspectorAsyncTask::data(name));
  if (ThreadDebugger* debugger = ThreadDebugger::from(toIsolate(context)))
    debugger->asyncTaskScheduled(name, task, true);
}

void asyncTaskScheduledBreakable(ExecutionContext* context,
                                 const char* name,
                                 void* task) {
  asyncTaskScheduled(context, name, task);
  breakableLocation(context, name);
}

void asyncTaskCanceled(ExecutionContext* context, void* task) {
  if (ThreadDebugger* debugger = ThreadDebugger::from(toIsolate(context)))
    debugger->asyncTaskCanceled(task);
  TRACE_EVENT_FLOW_END0("devtools.timeline.async", "AsyncTask",
                        TRACE_ID_LOCAL(reinterpret_cast<uintptr_t>(task)));
}

void asyncTaskCanceledBreakable(ExecutionContext* context,
                                const char* name,
                                void* task) {
  asyncTaskCanceled(context, task);
  breakableLocation(context, name);
}

void allAsyncTasksCanceled(ExecutionContext* context) {
  if (ThreadDebugger* debugger = ThreadDebugger::from(toIsolate(context)))
    debugger->allAsyncTasksCanceled();
}

void didReceiveResourceResponseButCanceled(LocalFrame* frame,
                                           DocumentLoader* loader,
                                           unsigned long identifier,
                                           const ResourceResponse& r,
                                           Resource* resource) {
  didReceiveResourceResponse(frame, identifier, loader, r, resource);
}

void canceledAfterReceivedResourceResponse(LocalFrame* frame,
                                           DocumentLoader* loader,
                                           unsigned long identifier,
                                           const ResourceResponse& r,
                                           Resource* resource) {
  didReceiveResourceResponseButCanceled(frame, loader, identifier, r, resource);
}

void continueWithPolicyIgnore(LocalFrame* frame,
                              DocumentLoader* loader,
                              unsigned long identifier,
                              const ResourceResponse& r,
                              Resource* resource) {
  didReceiveResourceResponseButCanceled(frame, loader, identifier, r, resource);
}

InspectorInstrumentationAgents* instrumentingAgentsFor(
    WorkerGlobalScope* workerGlobalScope) {
  if (!workerGlobalScope)
    return nullptr;
  if (WorkerInspectorController* controller =
          workerGlobalScope->thread()->workerInspectorController())
    return controller->instrumentingAgents();
  return nullptr;
}

InspectorInstrumentationAgents* instrumentingAgentsForNonDocumentContext(
    ExecutionContext* context) {
  if (context->isWorkerGlobalScope())
    return instrumentingAgentsFor(toWorkerGlobalScope(context));
  if (context->isMainThreadWorkletGlobalScope())
    return instrumentingAgentsFor(
        toMainThreadWorkletGlobalScope(context)->frame());
  return nullptr;
}

}  // namespace InspectorInstrumentation

}  // namespace blink

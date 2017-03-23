/*
* Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef InspectorInstrumentation_h
#define InspectorInstrumentation_h

#include "core/CoreExport.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/frame/LocalFrame.h"
#include "core/page/ChromeClient.h"

namespace blink {

class InspectorInstrumentationAgents;
class Resource;
class ThreadDebugger;
class WorkerGlobalScope;

namespace probe {

class CORE_EXPORT ProbeBase {
  STACK_ALLOCATED()

 public:
  double captureStartTime() const;
  double captureEndTime() const;
  double duration() const;

 private:
  mutable double m_startTime = 0;
  mutable double m_endTime = 0;
};

class CORE_EXPORT AsyncTask {
  STACK_ALLOCATED();

 public:
  AsyncTask(ExecutionContext*,
            void* task,
            const char* step = nullptr,
            bool enabled = true);
  ~AsyncTask();

 private:
  ThreadDebugger* m_debugger;
  void* m_task;
  bool m_recurring;
};

// Called from generated instrumentation code.
CORE_EXPORT InspectorInstrumentationAgents* instrumentingAgentsFor(
    WorkerGlobalScope*);
CORE_EXPORT InspectorInstrumentationAgents*
instrumentingAgentsForNonDocumentContext(ExecutionContext*);

inline InspectorInstrumentationAgents* instrumentingAgentsFor(
    LocalFrame* frame) {
  return frame ? frame->instrumentingAgents() : nullptr;
}

inline InspectorInstrumentationAgents* instrumentingAgentsFor(
    Document& document) {
  LocalFrame* frame = document.frame();
  if (!frame && document.templateDocumentHost())
    frame = document.templateDocumentHost()->frame();
  return instrumentingAgentsFor(frame);
}

inline InspectorInstrumentationAgents* instrumentingAgentsFor(
    Document* document) {
  return document ? instrumentingAgentsFor(*document) : nullptr;
}

inline InspectorInstrumentationAgents* instrumentingAgentsFor(
    ExecutionContext* context) {
  if (!context)
    return nullptr;
  return context->isDocument()
             ? instrumentingAgentsFor(*toDocument(context))
             : instrumentingAgentsForNonDocumentContext(context);
}

inline InspectorInstrumentationAgents* instrumentingAgentsFor(Node* node) {
  return node ? instrumentingAgentsFor(node->document()) : nullptr;
}

inline InspectorInstrumentationAgents* instrumentingAgentsFor(
    EventTarget* eventTarget) {
  return eventTarget
             ? instrumentingAgentsFor(eventTarget->getExecutionContext())
             : nullptr;
}

CORE_EXPORT void asyncTaskScheduled(ExecutionContext*,
                                    const String& name,
                                    void*);
CORE_EXPORT void asyncTaskScheduledBreakable(ExecutionContext*,
                                             const char* name,
                                             void*);
CORE_EXPORT void asyncTaskCanceled(ExecutionContext*, void*);
CORE_EXPORT void asyncTaskCanceledBreakable(ExecutionContext*,
                                            const char* name,
                                            void*);

CORE_EXPORT void allAsyncTasksCanceled(ExecutionContext*);
CORE_EXPORT void canceledAfterReceivedResourceResponse(LocalFrame*,
                                                       DocumentLoader*,
                                                       unsigned long identifier,
                                                       const ResourceResponse&,
                                                       Resource*);
CORE_EXPORT void continueWithPolicyIgnore(LocalFrame*,
                                          DocumentLoader*,
                                          unsigned long identifier,
                                          const ResourceResponse&,
                                          Resource*);

}  // namespace probe
}  // namespace blink

#include "core/InspectorInstrumentationInl.h"
#include "core/InspectorOverridesInl.h"

#endif  // !defined(InspectorInstrumentation_h)

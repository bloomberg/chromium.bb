/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WorkerReportingProxy_h
#define WorkerReportingProxy_h

#include <memory>
#include "core/CoreExport.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleTypes.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"

namespace blink {

class SourceLocation;
class WorkerOrWorkletGlobalScope;

// APIs used by workers to report console and worker activity.
class CORE_EXPORT WorkerReportingProxy {
 public:
  virtual ~WorkerReportingProxy() {}

  virtual void CountFeature(WebFeature) {}
  virtual void CountDeprecation(WebFeature) {}
  virtual void ReportException(const String& error_message,
                               std::unique_ptr<SourceLocation>,
                               int exception_id) {}
  virtual void ReportConsoleMessage(MessageSource,
                                    MessageLevel,
                                    const String& message,
                                    SourceLocation*) {}
  virtual void PostMessageToPageInspector(int session_id, const String&) {}

  // Invoked when the new WorkerGlobalScope is created. This is called after
  // didLoadWorkerScript().
  virtual void DidCreateWorkerGlobalScope(WorkerOrWorkletGlobalScope*) {}

  // Invoked when the WorkerGlobalScope is initialized. This is called after
  // didCreateWorkerGlobalScope().
  virtual void DidInitializeWorkerContext() {}

  // Invoked when the worker script is about to be evaluated. This is called
  // after didInitializeWorkerContext().
  virtual void WillEvaluateWorkerScript(size_t script_size,
                                        size_t cached_metadata_size) {}

  // Invoked when an imported script is about to be evaluated. This is called
  // after willEvaluateWorkerScript().
  virtual void WillEvaluateImportedScript(size_t script_size,
                                          size_t cached_metadata_size) {}

  // Invoked when the worker script is evaluated. |success| is true if the
  // evaluation completed with no uncaught exception.
  virtual void DidEvaluateWorkerScript(bool success) {}

  // Invoked when close() is invoked on the worker context.
  virtual void DidCloseWorkerGlobalScope() {}

  // Invoked when the thread is about to be stopped and WorkerGlobalScope
  // is to be destructed. When this is called, it is guaranteed that
  // WorkerGlobalScope is still alive.
  virtual void WillDestroyWorkerGlobalScope() {}

  // Invoked when the thread is stopped and WorkerGlobalScope is being
  // destructed. This is the last method that is called on this interface.
  virtual void DidTerminateWorkerThread() {}
};

}  // namespace blink

#endif  // WorkerReportingProxy_h

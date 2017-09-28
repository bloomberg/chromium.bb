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
#include "bindings/core/v8/SourceLocation.h"
#include "core/CoreExport.h"
#include "core/frame/WebFeatureForward.h"
#include "core/inspector/ConsoleTypes.h"
#include "platform/heap/Handle.h"
#include "platform/network/ContentSecurityPolicyResponseHeaders.h"
#include "platform/wtf/Forward.h"

namespace blink {

class WorkerOrWorkletGlobalScope;

// APIs used by workers to report console and worker activity. Some functions
// are called only for classic scripts and some of others are called only for
// module scripts. They're annotated with [classic script only] or [module
// script only].
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

  // Invoked when the new WorkerGlobalScope is created on
  // WorkerThread::InitializeOnWorkerThread.
  virtual void DidCreateWorkerGlobalScope(WorkerOrWorkletGlobalScope*) {}

  // Invoked when the WorkerGlobalScope is initialized on
  // WorkerThread::InitializeOnWorkerThread.
  virtual void DidInitializeWorkerContext() {}

  // Invoked when the worker's main script is loaded on
  // WorkerThread::InitializeOnWorkerThread(). Only invoked when the script was
  // loaded on the worker thread, i.e., via InstalledScriptsManager rather than
  // via ResourceLoader. ContentSecurityPolicy and ReferrerPolicy are read from
  // the response header of the main script.
  // This may block until CSP/ReferrerPolicy are set on the main thread
  // since they are required for script evaluation, which happens soon after
  // this function is called.
  // Called before WillEvaluateWorkerScript().
  virtual void DidLoadInstalledScript(
      const ContentSecurityPolicyResponseHeaders&,
      const String& referrer_policy_on_worker_thread) {}

  // [classic script only]
  // Invoked when the worker script is about to be evaluated on
  // WorkerThread::InitializeOnWorkerThread.
  virtual void WillEvaluateWorkerScript(size_t script_size,
                                        size_t cached_metadata_size) {}

  // [classic script only]
  // Invoked when an imported script is about to be evaluated.
  virtual void WillEvaluateImportedScript(size_t script_size,
                                          size_t cached_metadata_size) {}

  // [classic script only]
  // Invoked when the worker script is evaluated on
  // WorkerThread::InitializeOnWorkerThread. |success| is true if the evaluation
  // completed with no uncaught exception.
  virtual void DidEvaluateWorkerScript(bool success) {}

  // [module script only]
  // Invoked when the module script is evaluated. |success| is true if the
  // evaluation completed with no uncaught exception.
  virtual void DidEvaluateModuleScript(bool success) {}

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

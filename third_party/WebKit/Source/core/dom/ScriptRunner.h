/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScriptRunner_h
#define ScriptRunner_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

class Document;
class ScriptLoader;
class WebTaskRunner;

class CORE_EXPORT ScriptRunner final
    : public GarbageCollectedFinalized<ScriptRunner> {
  WTF_MAKE_NONCOPYABLE(ScriptRunner);

 public:
  static ScriptRunner* Create(Document* document) {
    return new ScriptRunner(document);
  }

  // Async scripts may either execute asynchronously (as their load
  // completes), or 'in order'. See
  // http://www.html5rocks.com/en/tutorials/speed/script-loading/ for more
  // information.
  enum AsyncExecutionType { kNone, kAsync, kInOrder };
  void QueueScriptForExecution(ScriptLoader*, AsyncExecutionType);
  bool HasPendingScripts() const {
    return !pending_in_order_scripts_.IsEmpty() ||
           !pending_async_scripts_.IsEmpty();
  }
  void Suspend();
  void Resume();
  void NotifyScriptReady(ScriptLoader*, AsyncExecutionType);
  void NotifyScriptStreamerFinished();

  static void MovePendingScript(Document&, Document&, ScriptLoader*);

  DECLARE_TRACE();

 private:
  class Task;

  explicit ScriptRunner(Document*);

  void MovePendingScript(ScriptRunner*, ScriptLoader*);
  bool RemovePendingInOrderScript(ScriptLoader*);
  void ScheduleReadyInOrderScripts();

  void PostTask(const WebTraceLocation&);

  // Execute the first task in in_order_scripts_to_execute_soon_.
  // Returns true if task was run, and false otherwise.
  bool ExecuteInOrderTask();
  // Execute any task in async_scripts_to_execute_soon_.
  // Returns true if task was run, and false otherwise.
  bool ExecuteAsyncTask();

  void ExecuteTask();

  // Try to start streaming a specific script or any available script.
  void TryStream(ScriptLoader*);
  void TryStreamAny();
  bool DoTryStream(ScriptLoader*);  // Implementation for both Try* methods.

  Member<Document> document_;

  HeapDeque<Member<ScriptLoader>> pending_in_order_scripts_;
  HeapHashSet<Member<ScriptLoader>> pending_async_scripts_;

  // http://www.whatwg.org/specs/web-apps/current-work/#set-of-scripts-that-will-execute-as-soon-as-possible
  HeapDeque<Member<ScriptLoader>> async_scripts_to_execute_soon_;
  HeapDeque<Member<ScriptLoader>> in_order_scripts_to_execute_soon_;

  RefPtr<WebTaskRunner> task_runner_;

  int number_of_in_order_scripts_with_pending_notification_;

  bool is_suspended_;

#ifndef NDEBUG
  // We expect to have one posted task in flight for each script in either
  // .._to_be_executed_soon_ queue. This invariant will be temporarily violated
  // when the ScriptRunner is suspended, or when we take a Script out the
  // async_scripts_to_be_executed_soon_ queue for streaming. We'll use this
  // variable to account & check this invariant for debugging.
  int number_of_extra_tasks_;
#endif
};

}  // namespace blink

#endif  // ScriptRunner_h

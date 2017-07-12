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

#include "core/dom/ScriptRunner.h"

#include <algorithm>
#include "core/dom/Document.h"
#include "core/dom/ScriptLoader.h"
#include "core/dom/TaskRunnerHelper.h"
#include "platform/heap/Handle.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"

namespace blink {

ScriptRunner::ScriptRunner(Document* document)
    : document_(document),
      task_runner_(TaskRunnerHelper::Get(TaskType::kNetworking, document)),
      number_of_in_order_scripts_with_pending_notification_(0),
      is_suspended_(false) {
  DCHECK(document);
#ifndef NDEBUG
  has_ever_been_suspended_ = false;
#endif
}

void ScriptRunner::QueueScriptForExecution(ScriptLoader* script_loader,
                                           AsyncExecutionType execution_type) {
  DCHECK(script_loader);
  document_->IncrementLoadEventDelayCount();
  switch (execution_type) {
    case kAsync:
      pending_async_scripts_.insert(script_loader);
      break;

    case kInOrder:
      pending_in_order_scripts_.push_back(script_loader);
      number_of_in_order_scripts_with_pending_notification_++;
      break;
    case kNone:
      NOTREACHED();
      break;
  }
}

void ScriptRunner::PostTask(const WebTraceLocation& web_trace_location) {
  task_runner_->PostTask(
      web_trace_location,
      WTF::Bind(&ScriptRunner::ExecuteTask, WrapWeakPersistent(this)));
}

void ScriptRunner::Suspend() {
#ifndef NDEBUG
  has_ever_been_suspended_ = true;
#endif

  is_suspended_ = true;
}

void ScriptRunner::Resume() {
  DCHECK(is_suspended_);

  is_suspended_ = false;

  for (size_t i = 0; i < async_scripts_to_execute_soon_.size(); ++i) {
    PostTask(BLINK_FROM_HERE);
  }
  for (size_t i = 0; i < in_order_scripts_to_execute_soon_.size(); ++i) {
    PostTask(BLINK_FROM_HERE);
  }
}

void ScriptRunner::ScheduleReadyInOrderScripts() {
  while (!pending_in_order_scripts_.IsEmpty() &&
         pending_in_order_scripts_.front()->IsReady()) {
    in_order_scripts_to_execute_soon_.push_back(
        pending_in_order_scripts_.TakeFirst());
    PostTask(BLINK_FROM_HERE);
  }
}

void ScriptRunner::NotifyScriptReady(ScriptLoader* script_loader,
                                     AsyncExecutionType execution_type) {
  SECURITY_CHECK(script_loader);
  switch (execution_type) {
    case kAsync:
      // SECURITY_CHECK() makes us crash in a controlled way in error cases
      // where the ScriptLoader is associated with the wrong ScriptRunner
      // (otherwise we'd cause a use-after-free in ~ScriptRunner when it tries
      // to detach).
      SECURITY_CHECK(pending_async_scripts_.Contains(script_loader));

      pending_async_scripts_.erase(script_loader);
      async_scripts_to_execute_soon_.push_back(script_loader);

      PostTask(BLINK_FROM_HERE);

      break;

    case kInOrder:
      SECURITY_CHECK(number_of_in_order_scripts_with_pending_notification_ > 0);
      number_of_in_order_scripts_with_pending_notification_--;

      ScheduleReadyInOrderScripts();

      break;
    case kNone:
      NOTREACHED();
      break;
  }
}

bool ScriptRunner::RemovePendingInOrderScript(ScriptLoader* script_loader) {
  auto it = std::find(pending_in_order_scripts_.begin(),
                      pending_in_order_scripts_.end(), script_loader);
  if (it == pending_in_order_scripts_.end())
    return false;
  pending_in_order_scripts_.erase(it);
  SECURITY_CHECK(number_of_in_order_scripts_with_pending_notification_ > 0);
  number_of_in_order_scripts_with_pending_notification_--;
  return true;
}

void ScriptRunner::MovePendingScript(Document& old_document,
                                     Document& new_document,
                                     ScriptLoader* script_loader) {
  Document* new_context_document = new_document.ContextDocument();
  if (!new_context_document) {
    // Document's contextDocument() method will return no Document if the
    // following conditions both hold:
    //
    //   - The Document wasn't created with an explicit context document
    //     and that document is otherwise kept alive.
    //   - The Document itself is detached from its frame.
    //
    // The script element's loader is in that case moved to document() and
    // its script runner, which is the non-null Document that contextDocument()
    // would return if not detached.
    DCHECK(!new_document.GetFrame());
    new_context_document = &new_document;
  }
  Document* old_context_document = old_document.ContextDocument();
  if (!old_context_document) {
    DCHECK(!old_document.GetFrame());
    old_context_document = &old_document;
  }
  if (old_context_document != new_context_document)
    old_context_document->GetScriptRunner()->MovePendingScript(
        new_context_document->GetScriptRunner(), script_loader);
}

void ScriptRunner::MovePendingScript(ScriptRunner* new_runner,
                                     ScriptLoader* script_loader) {
  auto it = pending_async_scripts_.find(script_loader);
  if (it != pending_async_scripts_.end()) {
    new_runner->QueueScriptForExecution(script_loader, kAsync);
    pending_async_scripts_.erase(it);
    document_->DecrementLoadEventDelayCount();
    return;
  }
  if (RemovePendingInOrderScript(script_loader)) {
    new_runner->QueueScriptForExecution(script_loader, kInOrder);
    document_->DecrementLoadEventDelayCount();
  }
}

// Returns true if task was run, and false otherwise.
bool ScriptRunner::ExecuteTaskFromQueue(
    HeapDeque<Member<ScriptLoader>>* task_queue) {
  if (task_queue->IsEmpty())
    return false;
  task_queue->TakeFirst()->Execute();

  document_->DecrementLoadEventDelayCount();
  return true;
}

void ScriptRunner::ExecuteTask() {
  if (is_suspended_)
    return;

  if (ExecuteTaskFromQueue(&async_scripts_to_execute_soon_))
    return;

  if (ExecuteTaskFromQueue(&in_order_scripts_to_execute_soon_))
    return;

#ifndef NDEBUG
  // Extra tasks should be posted only when we resume after suspending.
  DCHECK(has_ever_been_suspended_);
#endif
}

DEFINE_TRACE(ScriptRunner) {
  visitor->Trace(document_);
  visitor->Trace(pending_in_order_scripts_);
  visitor->Trace(pending_async_scripts_);
  visitor->Trace(async_scripts_to_execute_soon_);
  visitor->Trace(in_order_scripts_to_execute_soon_);
}

}  // namespace blink

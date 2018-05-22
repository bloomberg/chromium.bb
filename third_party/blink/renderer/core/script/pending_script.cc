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

#include "third_party/blink/renderer/core/script/pending_script.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/script/script_element_base.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

namespace {
WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
    ScriptElementBase* element) {
  if (!element || !element->GetDocument().GetFrame())
    return WebScopedVirtualTimePauser();
  return element->GetDocument()
      .GetFrame()
      ->GetFrameScheduler()
      ->CreateWebScopedVirtualTimePauser(
          "PendingScript",
          WebScopedVirtualTimePauser::VirtualTaskDuration::kInstant);
}
}  // namespace

PendingScript::PendingScript(ScriptElementBase* element,
                             const TextPosition& starting_position)
    : element_(element),
      starting_position_(starting_position),
      parser_blocking_load_start_time_(0),
      virtual_time_pauser_(CreateWebScopedVirtualTimePauser(element)),
      client_(nullptr),
      original_context_document_(element->GetDocument().ContextDocument()) {}

PendingScript::~PendingScript() {}

void PendingScript::Dispose() {
  StopWatchingForLoad();
  DCHECK(!Client());
  DCHECK(!IsWatchingForLoad());

  starting_position_ = TextPosition::BelowRangePosition();
  parser_blocking_load_start_time_ = 0;

  DisposeInternal();
  element_ = nullptr;
}

void PendingScript::WatchForLoad(PendingScriptClient* client) {
  CheckState();

  DCHECK(!IsWatchingForLoad());
  DCHECK(client);
  // addClient() will call streamingFinished() if the load is complete. Callers
  // who do not expect to be re-entered from this call should not call
  // watchForLoad for a PendingScript which isReady. We also need to set
  // m_watchingForLoad early, since addClient() can result in calling
  // notifyFinished and further stopWatchingForLoad().
  client_ = client;
  if (IsReady()) {
    client_->PendingScriptFinished(this);
  } else {
    virtual_time_pauser_.PauseVirtualTime();
  }
}

void PendingScript::StopWatchingForLoad() {
  if (!IsWatchingForLoad())
    return;
  CheckState();
  DCHECK(IsExternalOrModule());
  client_ = nullptr;
  virtual_time_pauser_.UnpauseVirtualTime();
}

ScriptElementBase* PendingScript::GetElement() const {
  // As mentioned in the comment at |m_element| declaration,
  // |m_element|  must point to the corresponding ScriptLoader's
  // client.
  CHECK(element_);
  return element_.Get();
}

void PendingScript::MarkParserBlockingLoadStartTime() {
  DCHECK_EQ(parser_blocking_load_start_time_, 0.0);
  parser_blocking_load_start_time_ = CurrentTimeTicksInSeconds();
}

void PendingScript::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_);
  visitor->Trace(client_);
  visitor->Trace(original_context_document_);
}

bool PendingScript::IsControlledByScriptRunner() const {
  switch (scheduling_type_) {
    case ScriptSchedulingType::kNotSet:
      NOTREACHED();
      return false;

    case ScriptSchedulingType::kDefer:
    case ScriptSchedulingType::kParserBlocking:
    case ScriptSchedulingType::kParserBlockingInline:
    case ScriptSchedulingType::kImmediate:
      return false;

    case ScriptSchedulingType::kInOrder:
    case ScriptSchedulingType::kAsync:
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace blink

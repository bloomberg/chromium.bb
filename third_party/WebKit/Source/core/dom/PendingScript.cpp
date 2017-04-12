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

#include "core/dom/PendingScript.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/ClassicScript.h"
#include "core/dom/Document.h"
#include "core/dom/ScriptElementBase.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/SubresourceIntegrity.h"
#include "platform/SharedBuffer.h"
#include "platform/wtf/CurrentTime.h"

namespace blink {

PendingScript* PendingScript::Create(ScriptElementBase* element,
                                     ScriptResource* resource) {
  return new PendingScript(element, resource, TextPosition());
}

PendingScript* PendingScript::Create(ScriptElementBase* element,
                                     const TextPosition& starting_position) {
  return new PendingScript(element, nullptr, starting_position);
}

PendingScript* PendingScript::CreateForTesting(ScriptResource* resource) {
  return new PendingScript(nullptr, resource, TextPosition(), true);
}

PendingScript::PendingScript(ScriptElementBase* element,
                             ScriptResource* resource,
                             const TextPosition& starting_position,
                             bool is_for_testing)
    : watching_for_load_(false),
      element_(element),
      starting_position_(starting_position),
      integrity_failure_(false),
      parser_blocking_load_start_time_(0),
      client_(nullptr),
      is_for_testing_(is_for_testing) {
  CheckState();
  SetResource(resource);
  MemoryCoordinator::Instance().RegisterClient(this);
}

PendingScript::~PendingScript() {}

NOINLINE void PendingScript::CheckState() const {
  // TODO(hiroshige): Turn these CHECK()s into DCHECK() before going to beta.
  CHECK(is_for_testing_ || element_);
  CHECK(GetResource() || !streamer_);
  CHECK(!streamer_ || streamer_->GetResource() == GetResource());
}

void PendingScript::Dispose() {
  StopWatchingForLoad();
  DCHECK(!client_);
  DCHECK(!watching_for_load_);

  MemoryCoordinator::Instance().UnregisterClient(this);
  SetResource(nullptr);
  starting_position_ = TextPosition::BelowRangePosition();
  integrity_failure_ = false;
  parser_blocking_load_start_time_ = 0;
  if (streamer_)
    streamer_->Cancel();
  streamer_ = nullptr;
  element_ = nullptr;
}

void PendingScript::WatchForLoad(PendingScriptClient* client) {
  CheckState();

  DCHECK(!watching_for_load_);
  // addClient() will call streamingFinished() if the load is complete. Callers
  // who do not expect to be re-entered from this call should not call
  // watchForLoad for a PendingScript which isReady. We also need to set
  // m_watchingForLoad early, since addClient() can result in calling
  // notifyFinished and further stopWatchingForLoad().
  watching_for_load_ = true;
  client_ = client;
  if (IsReady())
    client_->PendingScriptFinished(this);
}

void PendingScript::StopWatchingForLoad() {
  if (!watching_for_load_)
    return;
  CheckState();
  DCHECK(GetResource());
  client_ = nullptr;
  watching_for_load_ = false;
}

ScriptElementBase* PendingScript::GetElement() const {
  // As mentioned in the comment at |m_element| declaration,
  // |m_element|  must point to the corresponding ScriptLoader's
  // client.
  CHECK(element_);
  return element_.Get();
}

void PendingScript::StreamingFinished() {
  CheckState();
  DCHECK(GetResource());
  if (client_)
    client_->PendingScriptFinished(this);
}

void PendingScript::MarkParserBlockingLoadStartTime() {
  DCHECK_EQ(parser_blocking_load_start_time_, 0.0);
  parser_blocking_load_start_time_ = MonotonicallyIncreasingTime();
}

// Returns true if SRI check passed.
static bool CheckScriptResourceIntegrity(Resource* resource,
                                         ScriptElementBase* element) {
  DCHECK_EQ(resource->GetType(), Resource::kScript);
  ScriptResource* script_resource = ToScriptResource(resource);
  String integrity_attr = element->IntegrityAttributeValue();

  // It is possible to get back a script resource with integrity metadata
  // for a request with an empty integrity attribute. In that case, the
  // integrity check should be skipped, so this check ensures that the
  // integrity attribute isn't empty in addition to checking if the
  // resource has empty integrity metadata.
  if (integrity_attr.IsEmpty() ||
      script_resource->IntegrityMetadata().IsEmpty())
    return true;

  switch (script_resource->IntegrityDisposition()) {
    case ResourceIntegrityDisposition::kPassed:
      return true;

    case ResourceIntegrityDisposition::kFailed:
      // TODO(jww): This should probably also generate a console
      // message identical to the one produced by
      // CheckSubresourceIntegrity below. See https://crbug.com/585267.
      return false;

    case ResourceIntegrityDisposition::kNotChecked: {
      if (!resource->ResourceBuffer())
        return true;

      bool passed = SubresourceIntegrity::CheckSubresourceIntegrity(
          script_resource->IntegrityMetadata(), element->GetDocument(),
          resource->ResourceBuffer()->Data(),
          resource->ResourceBuffer()->size(), resource->Url(), *resource);
      script_resource->SetIntegrityDisposition(
          passed ? ResourceIntegrityDisposition::kPassed
                 : ResourceIntegrityDisposition::kFailed);
      return passed;
    }
  }

  NOTREACHED();
  return true;
}

void PendingScript::NotifyFinished(Resource* resource) {
  // The following SRI checks need to be here because, unfortunately, fetches
  // are not done purely according to the Fetch spec. In particular,
  // different requests for the same resource do not have different
  // responses; the memory cache can (and will) return the exact same
  // Resource object.
  //
  // For different requests, the same Resource object will be returned and
  // will not be associated with the particular request.  Therefore, when the
  // body of the response comes in, there's no way to validate the integrity
  // of the Resource object against a particular request (since there may be
  // several pending requests all tied to the identical object, and the
  // actual requests are not stored).
  //
  // In order to simulate the correct behavior, Blink explicitly does the SRI
  // checks here, when a PendingScript tied to a particular request is
  // finished (and in the case of a StyleSheet, at the point of execution),
  // while having proper Fetch checks in the fetch module for use in the
  // fetch JavaScript API. In a future world where the ResourceFetcher uses
  // the Fetch algorithm, this should be fixed by having separate Response
  // objects (perhaps attached to identical Resource objects) per request.
  //
  // See https://crbug.com/500701 for more information.
  CheckState();
  if (element_) {
    integrity_failure_ = !CheckScriptResourceIntegrity(resource, element_);
  }

  // If script streaming is in use, the client will be notified in
  // streamingFinished.
  if (streamer_)
    streamer_->NotifyFinished(resource);
  else if (client_)
    client_->PendingScriptFinished(this);
}

void PendingScript::NotifyAppendData(ScriptResource* resource) {
  if (streamer_)
    streamer_->NotifyAppendData(resource);
}

DEFINE_TRACE(PendingScript) {
  visitor->Trace(element_);
  visitor->Trace(streamer_);
  visitor->Trace(client_);
  ResourceOwner<ScriptResource>::Trace(visitor);
  MemoryCoordinatorClient::Trace(visitor);
}

ClassicScript* PendingScript::GetSource(const KURL& document_url,
                                        bool& error_occurred) const {
  CheckState();

  error_occurred = this->ErrorOccurred();
  if (GetResource()) {
    DCHECK(GetResource()->IsLoaded());
    if (streamer_ && !streamer_->StreamingSuppressed())
      return ClassicScript::Create(ScriptSourceCode(streamer_, GetResource()));
    return ClassicScript::Create(ScriptSourceCode(GetResource()));
  }

  return ClassicScript::Create(ScriptSourceCode(
      element_->TextContent(), document_url, StartingPosition()));
}

void PendingScript::SetStreamer(ScriptStreamer* streamer) {
  DCHECK(!streamer_);
  DCHECK(!watching_for_load_);
  streamer_ = streamer;
  CheckState();
}

bool PendingScript::IsReady() const {
  CheckState();
  if (GetResource()) {
    return GetResource()->IsLoaded() && (!streamer_ || streamer_->IsFinished());
  }

  return true;
}

bool PendingScript::ErrorOccurred() const {
  CheckState();
  if (GetResource())
    return GetResource()->ErrorOccurred() || integrity_failure_;

  return false;
}

void PendingScript::OnPurgeMemory() {
  CheckState();
  if (!streamer_)
    return;
  streamer_->Cancel();
  streamer_ = nullptr;
}

void PendingScript::StartStreamingIfPossible(
    Document* document,
    ScriptStreamer::Type streamer_type) {
  if (!document->GetFrame())
    return;

  ScriptState* script_state = ToScriptStateForMainWorld(document->GetFrame());
  if (!script_state)
    return;

  ScriptStreamer::StartStreaming(
      this, streamer_type, document->GetFrame()->GetSettings(), script_state,
      TaskRunnerHelper::Get(TaskType::kNetworking, document));
}

}  // namespace blink

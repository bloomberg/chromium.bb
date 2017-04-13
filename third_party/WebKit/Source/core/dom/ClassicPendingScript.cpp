// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ClassicPendingScript.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/SubresourceIntegrity.h"
#include "platform/loader/fetch/MemoryCache.h"

namespace blink {

ClassicPendingScript* ClassicPendingScript::Create(ScriptElementBase* element,
                                                   ScriptResource* resource) {
  return new ClassicPendingScript(element, resource, TextPosition());
}

ClassicPendingScript* ClassicPendingScript::Create(
    ScriptElementBase* element,
    const TextPosition& starting_position) {
  return new ClassicPendingScript(element, nullptr, starting_position);
}

ClassicPendingScript* ClassicPendingScript::CreateForTesting(
    ScriptResource* resource) {
  return new ClassicPendingScript(nullptr, resource, TextPosition(), true);
}

ClassicPendingScript::ClassicPendingScript(
    ScriptElementBase* element,
    ScriptResource* resource,
    const TextPosition& starting_position,
    bool is_for_testing)
    : PendingScript(element, starting_position),
      integrity_failure_(false),
      is_for_testing_(is_for_testing) {
  CheckState();
  SetResource(resource);
  MemoryCoordinator::Instance().RegisterClient(this);
}

ClassicPendingScript::~ClassicPendingScript() {}

NOINLINE void ClassicPendingScript::CheckState() const {
  // TODO(hiroshige): Turn these CHECK()s into DCHECK() before going to beta.
  CHECK(is_for_testing_ || GetElement());
  CHECK(GetResource() || !streamer_);
  CHECK(!streamer_ || streamer_->GetResource() == GetResource());
}

void ClassicPendingScript::DisposeInternal() {
  MemoryCoordinator::Instance().UnregisterClient(this);
  SetResource(nullptr);
  integrity_failure_ = false;
  if (streamer_)
    streamer_->Cancel();
  streamer_ = nullptr;
}

void ClassicPendingScript::StreamingFinished() {
  CheckState();
  DCHECK(GetResource());
  if (Client())
    Client()->PendingScriptFinished(this);
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

void ClassicPendingScript::NotifyFinished(Resource* resource) {
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
  if (!is_for_testing_ && GetElement()) {
    integrity_failure_ = !CheckScriptResourceIntegrity(resource, GetElement());
  }

  // If script streaming is in use, the client will be notified in
  // streamingFinished.
  if (streamer_)
    streamer_->NotifyFinished(resource);
  else if (Client())
    Client()->PendingScriptFinished(this);
}

void ClassicPendingScript::NotifyAppendData(ScriptResource* resource) {
  if (streamer_)
    streamer_->NotifyAppendData(resource);
}

DEFINE_TRACE(ClassicPendingScript) {
  visitor->Trace(streamer_);
  ResourceOwner<ScriptResource>::Trace(visitor);
  MemoryCoordinatorClient::Trace(visitor);
  PendingScript::Trace(visitor);
}

ClassicScript* ClassicPendingScript::GetSource(const KURL& document_url,
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
      GetElement()->TextContent(), document_url, StartingPosition()));
}

void ClassicPendingScript::SetStreamer(ScriptStreamer* streamer) {
  DCHECK(!streamer_);
  DCHECK(!IsWatchingForLoad());
  streamer_ = streamer;
  CheckState();
}

bool ClassicPendingScript::IsReady() const {
  CheckState();
  if (GetResource()) {
    return GetResource()->IsLoaded() && (!streamer_ || streamer_->IsFinished());
  }

  return true;
}

bool ClassicPendingScript::ErrorOccurred() const {
  CheckState();
  if (GetResource())
    return GetResource()->ErrorOccurred() || integrity_failure_;

  return false;
}

void ClassicPendingScript::OnPurgeMemory() {
  CheckState();
  if (!streamer_)
    return;
  streamer_->Cancel();
  streamer_ = nullptr;
}

void ClassicPendingScript::StartStreamingIfPossible(
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

bool ClassicPendingScript::WasCanceled() const {
  return GetResource()->WasCanceled();
}

KURL ClassicPendingScript::Url() const {
  return GetResource()->Url();
}

void ClassicPendingScript::RemoveFromMemoryCache() {
  GetMemoryCache()->Remove(GetResource());
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ClassicPendingScript.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/ScriptStreamer.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentWriteIntervention.h"
#include "core/dom/ScriptLoader.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/SubresourceIntegrityHelper.h"
#include "core/loader/resource/ScriptResource.h"
#include "platform/bindings/ScriptState.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "public/platform/TaskType.h"

namespace blink {

ClassicPendingScript* ClassicPendingScript::Fetch(
    const KURL& url,
    Document& element_document,
    const ScriptFetchOptions& options,
    const WTF::TextEncoding& encoding,
    ScriptElementBase* element,
    FetchParameters::DeferOption defer) {
  FetchParameters params = options.CreateFetchParameters(
      url, element_document.GetSecurityOrigin(), encoding, defer);

  ClassicPendingScript* pending_script = new ClassicPendingScript(
      element, TextPosition(), options, true /* is_external */);

  // [Intervention]
  // For users on slow connections, we want to avoid blocking the parser in
  // the main frame on script loads inserted via document.write, since it
  // can add significant delays before page content is displayed on the
  // screen.
  pending_script->intervened_ =
      MaybeDisallowFetchForDocWrittenScript(params, element_document);

  // https://html.spec.whatwg.org/#fetch-a-classic-script
  // Step 2. Set request's client to settings object. [spec text]
  //
  // Note: |element_document| corresponds to the settings object.
  ScriptResource* resource =
      ScriptResource::Fetch(params, element_document.Fetcher());
  if (!resource)
    return nullptr;
  pending_script->SetResource(resource);
  pending_script->CheckState();
  return pending_script;
}

ClassicPendingScript* ClassicPendingScript::CreateExternalForTest(
    ScriptElementBase* element,
    ScriptResource* resource) {
  DCHECK(resource);
  ClassicPendingScript* pending_script = new ClassicPendingScript(
      element, TextPosition(), ScriptFetchOptions(), true /* is_external */);
  pending_script->SetResource(resource);
  pending_script->CheckState();
  return pending_script;
}

ClassicPendingScript* ClassicPendingScript::CreateInline(
    ScriptElementBase* element,
    const TextPosition& starting_position,
    const ScriptFetchOptions& options) {
  ClassicPendingScript* pending_script = new ClassicPendingScript(
      element, starting_position, options, false /* is_external */);
  pending_script->CheckState();
  return pending_script;
}

ClassicPendingScript::ClassicPendingScript(
    ScriptElementBase* element,
    const TextPosition& starting_position,
    const ScriptFetchOptions& options,
    bool is_external)
    : PendingScript(element, starting_position),
      options_(options),
      is_external_(is_external),
      ready_state_(is_external ? kWaitingForResource : kReady),
      integrity_failure_(false),
      is_currently_streaming_(false) {
  CHECK(GetElement());
  MemoryCoordinator::Instance().RegisterClient(this);
}

ClassicPendingScript::~ClassicPendingScript() {}

NOINLINE void ClassicPendingScript::CheckState() const {
  // TODO(hiroshige): Turn these CHECK()s into DCHECK() before going to beta.
  CHECK(!prefinalizer_called_);
  CHECK(GetElement());
  CHECK_EQ(is_external_, !!GetResource());
  CHECK(GetResource() || !streamer_);
  CHECK(!streamer_ || streamer_->GetResource() == GetResource());
}

void ClassicPendingScript::Prefinalize() {
  // TODO(hiroshige): Consider moving this to ScriptStreamer's prefinalizer.
  // https://crbug.com/715309
  CancelStreaming();
  prefinalizer_called_ = true;
}

void ClassicPendingScript::DisposeInternal() {
  MemoryCoordinator::Instance().UnregisterClient(this);
  SetResource(nullptr);
  integrity_failure_ = false;
  CancelStreaming();
}

void ClassicPendingScript::StreamingFinished() {
  CheckState();
  DCHECK(streamer_);  // Should only be called by ScriptStreamer.
  DCHECK(IsCurrentlyStreaming());

  if (ready_state_ == kWaitingForStreaming) {
    FinishWaitingForStreaming();
  } else if (ready_state_ == kReadyStreaming) {
    FinishReadyStreaming();
  } else {
    NOTREACHED();
  }

  DCHECK(!IsCurrentlyStreaming());
}

void ClassicPendingScript::FinishWaitingForStreaming() {
  CheckState();
  DCHECK(GetResource());
  DCHECK_EQ(ready_state_, kWaitingForStreaming);

  bool error_occurred = GetResource()->ErrorOccurred() || integrity_failure_;
  AdvanceReadyState(error_occurred ? kErrorOccurred : kReady);
}

void ClassicPendingScript::FinishReadyStreaming() {
  CheckState();
  DCHECK(GetResource());
  DCHECK_EQ(ready_state_, kReadyStreaming);
  AdvanceReadyState(kReady);
}

void ClassicPendingScript::CancelStreaming() {
  if (!streamer_)
    return;

  streamer_->Cancel();
  streamer_ = nullptr;
  streamer_done_.Reset();
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
  ScriptElementBase* element = GetElement();
  if (element) {
    SubresourceIntegrityHelper::DoReport(element->GetDocument(),
                                         GetResource()->IntegrityReportInfo());

    // It is possible to get back a script resource with integrity metadata
    // for a request with an empty integrity attribute. In that case, the
    // integrity check should be skipped, so this check ensures that the
    // integrity attribute isn't empty in addition to checking if the
    // resource has empty integrity metadata.
    if (!element->IntegrityAttributeValue().IsEmpty()) {
      integrity_failure_ = GetResource()->IntegrityDisposition() !=
                           ResourceIntegrityDisposition::kPassed;
    }
  }

  if (intervened_) {
    PossiblyFetchBlockedDocWriteScript(resource, element->GetDocument(),
                                       options_);
  }

  // We are now waiting for script streaming to finish.
  // If there is no script streamer, this step completes immediately.
  AdvanceReadyState(kWaitingForStreaming);
  if (streamer_)
    streamer_->NotifyFinished(resource);
  else
    FinishWaitingForStreaming();
}

void ClassicPendingScript::NotifyAppendData(ScriptResource* resource) {
  if (streamer_)
    streamer_->NotifyAppendData(resource);
}

void ClassicPendingScript::Trace(blink::Visitor* visitor) {
  visitor->Trace(streamer_);
  ResourceOwner<ScriptResource>::Trace(visitor);
  MemoryCoordinatorClient::Trace(visitor);
  PendingScript::Trace(visitor);
}

ClassicScript* ClassicPendingScript::GetSource(const KURL& document_url,
                                               bool& error_occurred) const {
  CheckState();
  DCHECK(IsReady());

  error_occurred = ErrorOccurred();
  if (!is_external_) {
    ScriptSourceCode source_code(GetElement()->TextFromChildren(), document_url,
                                 StartingPosition());
    return ClassicScript::Create(source_code, options_);
  }

  DCHECK(GetResource()->IsLoaded());
  bool streamer_ready = (ready_state_ == kReady) && streamer_ &&
                        !streamer_->StreamingSuppressed();
  ScriptSourceCode source_code(streamer_ready ? streamer_ : nullptr,
                               GetResource());
  return ClassicScript::Create(source_code, options_);
}

void ClassicPendingScript::SetStreamer(ScriptStreamer* streamer) {
  DCHECK(streamer);
  DCHECK(!streamer_);
  DCHECK(!IsWatchingForLoad() || ready_state_ != kWaitingForResource);
  DCHECK(!streamer->IsFinished());
  DCHECK(ready_state_ == kWaitingForResource || ready_state_ == kReady);

  streamer_ = streamer;
  is_currently_streaming_ = true;
  if (streamer && ready_state_ == kReady)
    AdvanceReadyState(kReadyStreaming);

  CheckState();
}

bool ClassicPendingScript::IsReady() const {
  CheckState();
  return ready_state_ >= kReady;
}

bool ClassicPendingScript::ErrorOccurred() const {
  CheckState();
  return ready_state_ == kErrorOccurred;
}

void ClassicPendingScript::AdvanceReadyState(ReadyState new_ready_state) {
  // We will allow exactly these state transitions:
  //
  // kWaitingForResource -> kWaitingForStreaming -> [kReady, kErrorOccurred]
  // kReady -> kReadyStreaming -> kReady
  switch (ready_state_) {
    case kWaitingForResource:
      CHECK_EQ(new_ready_state, kWaitingForStreaming);
      break;
    case kWaitingForStreaming:
      CHECK(new_ready_state == kReady || new_ready_state == kErrorOccurred);
      break;
    case kReady:
      CHECK_EQ(new_ready_state, kReadyStreaming);
      break;
    case kReadyStreaming:
      CHECK_EQ(new_ready_state, kReady);
      break;
    case kErrorOccurred:
      NOTREACHED();
      break;
  }

  bool old_is_ready = IsReady();
  ready_state_ = new_ready_state;

  // Did we transition into a 'ready' state?
  if (IsReady() && !old_is_ready && IsWatchingForLoad())
    Client()->PendingScriptFinished(this);

  // Did we finish streaming?
  if (IsCurrentlyStreaming()) {
    if (ready_state_ == kReady || ready_state_ == kErrorOccurred) {
      // Call the streamer_done_ callback. Ensure that is_currently_streaming_
      // is reset only after the callback returns, to prevent accidentally
      // start streaming by work done within the callback. (crbug.com/754360)
      WTF::Closure done = std::move(streamer_done_);
      if (done)
        std::move(done).Run();
      is_currently_streaming_ = false;
    }
  }

  // Streaming-related post conditions:

  // IsCurrentlyStreaming should match what streamer_ thinks.
  DCHECK_EQ(IsCurrentlyStreaming(), streamer_ && !streamer_->IsFinished());
  // IsCurrentlyStreaming should match the ready_state_.
  DCHECK_EQ(IsCurrentlyStreaming(),
            ready_state_ == kReadyStreaming ||
                (streamer_ && (ready_state_ == kWaitingForResource ||
                               ready_state_ == kWaitingForStreaming)));
  // We can only have a streamer_done_ callback if we are actually streaming.
  DCHECK(IsCurrentlyStreaming() || !streamer_done_);
}

void ClassicPendingScript::OnPurgeMemory() {
  CheckState();
  CancelStreaming();
}

bool ClassicPendingScript::StartStreamingIfPossible(
    ScriptStreamer::Type streamer_type,
    WTF::Closure done) {
  if (IsCurrentlyStreaming())
    return false;

  // We can start streaming in two states: While still loading
  // (kWaitingForResource), or after having loaded (kReady).
  if (ready_state_ != kWaitingForResource && ready_state_ != kReady)
    return false;

  Document* document = &GetElement()->GetDocument();
  if (!document || !document->GetFrame())
    return false;

  ScriptState* script_state = ToScriptStateForMainWorld(document->GetFrame());
  if (!script_state)
    return false;

  // To support streaming re-try, we'll clear the existing streamer if
  // it exists; it claims to be finished; but it's finished because streaming
  // has been suppressed.
  if (streamer_ && streamer_->StreamingSuppressed() &&
      streamer_->IsFinished()) {
    DCHECK_EQ(ready_state_, kReady);
    DCHECK(!streamer_done_);
    DCHECK(!IsCurrentlyStreaming());
    streamer_.Clear();
  }

  if (streamer_)
    return false;

  // The two checks above should imply that we're not presently streaming.
  DCHECK(!IsCurrentlyStreaming());

  // Parser blocking scripts tend to do a lot of work in the 'finished'
  // callbacks, while async + in-order scripts all do control-like activities
  // (like posting new tasks). Use the 'control' queue only for control tasks.
  // (More details in discussion for cl 500147.)
  auto task_type = streamer_type == ScriptStreamer::kParsingBlocking
                       ? TaskType::kNetworking
                       : TaskType::kNetworkingControl;

  DCHECK(!streamer_);
  DCHECK(!IsCurrentlyStreaming());
  DCHECK(!streamer_done_);
  ScriptStreamer::StartStreaming(
      this, streamer_type, document->GetFrame()->GetSettings(), script_state,
      document->GetTaskRunner(task_type));
  bool success = streamer_ && !streamer_->IsStreamingFinished();

  // If we have successfully started streaming, we are required to call the
  // callback.
  DCHECK_EQ(success, IsCurrentlyStreaming());
  if (success)
    streamer_done_ = std::move(done);
  return success;
}

bool ClassicPendingScript::IsCurrentlyStreaming() const {
  return is_currently_streaming_;
}

bool ClassicPendingScript::WasCanceled() const {
  if (!is_external_)
    return false;
  return GetResource()->WasCanceled();
}

KURL ClassicPendingScript::UrlForTracing() const {
  if (!is_external_ || !GetResource())
    return NullURL();

  return GetResource()->Url();
}

}  // namespace blink

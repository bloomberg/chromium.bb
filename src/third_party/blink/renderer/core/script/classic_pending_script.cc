// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/classic_pending_script.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/core/script/document_write_intervention.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/allowed_by_nosniff.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/detachable_use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/source_keyed_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// <specdef href="https://html.spec.whatwg.org/C/#fetch-a-classic-script">
ClassicPendingScript* ClassicPendingScript::Fetch(
    const KURL& url,
    Document& element_document,
    const ScriptFetchOptions& options,
    CrossOriginAttributeValue cross_origin,
    const WTF::TextEncoding& encoding,
    ScriptElementBase* element,
    FetchParameters::DeferOption defer) {
  FetchParameters params(
      options.CreateFetchParameters(url, element_document.GetSecurityOrigin(),
                                    cross_origin, encoding, defer));

  ClassicPendingScript* pending_script =
      MakeGarbageCollected<ClassicPendingScript>(
          element, TextPosition(), KURL(), String(),
          ScriptSourceLocationType::kExternalFile, options,
          true /* is_external */);

  // [Intervention]
  // For users on slow connections, we want to avoid blocking the parser in
  // the main frame on script loads inserted via document.write, since it
  // can add significant delays before page content is displayed on the
  // screen.
  pending_script->intervened_ =
      MaybeDisallowFetchForDocWrittenScript(params, element_document);

  // <spec step="2">Set request's client to settings object.</spec>
  //
  // Note: |element_document| corresponds to the settings object.
  //
  // We allow streaming, as WatchForLoad() is always called when the script
  // needs to execute and the ScriptResource is not finished, so
  // SetClientIsWaitingForFinished is always set on the resource.
  ScriptResource::Fetch(params, element_document.Fetcher(), pending_script,
                        ScriptResource::kAllowStreaming);
  pending_script->CheckState();
  return pending_script;
}

ClassicPendingScript* ClassicPendingScript::CreateInline(
    ScriptElementBase* element,
    const TextPosition& starting_position,
    const KURL& base_url,
    const String& source_text,
    ScriptSourceLocationType source_location_type,
    const ScriptFetchOptions& options) {
  ClassicPendingScript* pending_script =
      MakeGarbageCollected<ClassicPendingScript>(
          element, starting_position, base_url, source_text,
          source_location_type, options, false /* is_external */);
  pending_script->CheckState();
  return pending_script;
}

ClassicPendingScript::ClassicPendingScript(
    ScriptElementBase* element,
    const TextPosition& starting_position,
    const KURL& base_url_for_inline_script,
    const String& source_text_for_inline_script,
    ScriptSourceLocationType source_location_type,
    const ScriptFetchOptions& options,
    bool is_external)
    : PendingScript(element, starting_position),
      options_(options),
      base_url_for_inline_script_(base_url_for_inline_script),
      source_text_for_inline_script_(source_text_for_inline_script),
      source_location_type_(source_location_type),
      is_external_(is_external),
      ready_state_(is_external ? kWaitingForResource : kReady),
      integrity_failure_(false) {
  CHECK(GetElement());

  if (is_external_) {
    DCHECK(base_url_for_inline_script_.IsNull());
    DCHECK(source_text_for_inline_script_.IsNull());
  } else {
    DCHECK(!base_url_for_inline_script_.IsNull());
    DCHECK(!source_text_for_inline_script_.IsNull());
  }

  MemoryPressureListenerRegistry::Instance().RegisterClient(this);
}

ClassicPendingScript::~ClassicPendingScript() {}

NOINLINE void ClassicPendingScript::CheckState() const {
  // TODO(hiroshige): Turn these CHECK()s into DCHECK() before going to beta.
  CHECK(GetElement());
  CHECK_EQ(is_external_, !!GetResource());
}

namespace {

enum class StreamedBoolean {
  // Must match BooleanStreamed in enums.xml.
  kNotStreamed = 0,
  kStreamed = 1,
  kMaxValue = kStreamed
};

void RecordStartedStreamingHistogram(ScriptSchedulingType type,
                                     bool did_use_streamer) {
  StreamedBoolean streamed = did_use_streamer ? StreamedBoolean::kStreamed
                                              : StreamedBoolean::kNotStreamed;
  switch (type) {
    case ScriptSchedulingType::kParserBlocking: {
      UMA_HISTOGRAM_ENUMERATION(
          "WebCore.Scripts.ParsingBlocking.StartedStreaming", streamed);
      break;
    }
    case ScriptSchedulingType::kDefer: {
      UMA_HISTOGRAM_ENUMERATION("WebCore.Scripts.Deferred.StartedStreaming",
                                streamed);
      break;
    }
    case ScriptSchedulingType::kAsync: {
      UMA_HISTOGRAM_ENUMERATION("WebCore.Scripts.Async.StartedStreaming",
                                streamed);
      break;
    }
    default: {
      UMA_HISTOGRAM_ENUMERATION("WebCore.Scripts.Other.StartedStreaming",
                                streamed);
      break;
    }
  }
}

void RecordNotStreamingReasonHistogram(
    ScriptSchedulingType type,
    ScriptStreamer::NotStreamingReason reason) {
  switch (type) {
    case ScriptSchedulingType::kParserBlocking: {
      UMA_HISTOGRAM_ENUMERATION(
          "WebCore.Scripts.ParsingBlocking.NotStreamingReason", reason,
          ScriptStreamer::NotStreamingReason::kCount);
      break;
    }
    case ScriptSchedulingType::kDefer: {
      UMA_HISTOGRAM_ENUMERATION("WebCore.Scripts.Deferred.NotStreamingReason",
                                reason,
                                ScriptStreamer::NotStreamingReason::kCount);
      break;
    }
    case ScriptSchedulingType::kAsync: {
      UMA_HISTOGRAM_ENUMERATION("WebCore.Scripts.Async.NotStreamingReason",
                                reason,
                                ScriptStreamer::NotStreamingReason::kCount);
      break;
    }
    default: {
      UMA_HISTOGRAM_ENUMERATION("WebCore.Scripts.Other.NotStreamingReason",
                                reason,
                                ScriptStreamer::NotStreamingReason::kCount);
      break;
    }
  }
}

}  // namespace

void ClassicPendingScript::RecordStreamingHistogram(
    ScriptSchedulingType type,
    bool can_use_streamer,
    ScriptStreamer::NotStreamingReason reason) {
  RecordStartedStreamingHistogram(type, can_use_streamer);
  if (!can_use_streamer) {
    DCHECK_NE(ScriptStreamer::kInvalid, reason);
    RecordNotStreamingReasonHistogram(type, reason);
  }
}

void ClassicPendingScript::DisposeInternal() {
  MemoryPressureListenerRegistry::Instance().UnregisterClient(this);
  ClearResource();
  integrity_failure_ = false;
}

void ClassicPendingScript::WatchForLoad(PendingScriptClient* client) {
  if (is_external_) {
    // Once we are watching the ClassicPendingScript for load, we won't ever
    // try to start streaming this resource via this ClassicPendingScript, so
    // we mark the resource to instead get a finished notification when loading
    // (rather than streaming) completes.
    //
    // Do this in a task rather than directly to make sure the IsReady state
    // of PendingScript does not change during this call.
    GetElement()
        ->GetDocument()
        .GetTaskRunner(TaskType::kNetworking)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&ScriptResource::SetClientIsWaitingForFinished,
                             WrapPersistent(ToScriptResource(GetResource()))));
  }
  PendingScript::WatchForLoad(client);
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
  DCHECK(GetResource());
  ScriptElementBase* element = GetElement();
  if (element) {
    SubresourceIntegrityHelper::DoReport(*element->GetExecutionContext(),
                                         GetResource()->IntegrityReportInfo());

    // It is possible to get back a script resource with integrity metadata
    // for a request with an empty integrity attribute. In that case, the
    // integrity check should be skipped, as the integrity may not have been
    // "meant" for this specific request. If the resource is being served from
    // the preload cache however, we know any associated integrity metadata and
    // checks were destined for this request, so we cannot skip the integrity
    // check.
    if (!element->IntegrityAttributeValue().IsEmpty() ||
        GetResource()->IsLinkPreload()) {
      integrity_failure_ = GetResource()->IntegrityDisposition() !=
                           ResourceIntegrityDisposition::kPassed;
    }
  }

  if (intervened_) {
    CrossOriginAttributeValue cross_origin =
        GetCrossOriginAttributeValue(element->CrossOriginAttributeValue());
    PossiblyFetchBlockedDocWriteScript(resource, element->GetDocument(),
                                       options_, cross_origin);
  }

  TRACE_EVENT_WITH_FLOW1(
      TRACE_DISABLED_BY_DEFAULT("v8.compile"),
      "ClassicPendingScript::NotifyFinished", this, TRACE_EVENT_FLAG_FLOW_OUT,
      "data",
      inspector_parse_script_event::Data(GetResource()->InspectorId(),
                                         GetResource()->Url().GetString()));

  bool error_occurred = GetResource()->ErrorOccurred() || integrity_failure_;
  AdvanceReadyState(error_occurred ? kErrorOccurred : kReady);
}

void ClassicPendingScript::Trace(Visitor* visitor) {
  ResourceClient::Trace(visitor);
  MemoryPressureListener::Trace(visitor);
  PendingScript::Trace(visitor);
}

static SingleCachedMetadataHandler* GetInlineCacheHandler(const String& source,
                                                          Document& document) {
  if (!RuntimeEnabledFeatures::CacheInlineScriptCodeEnabled())
    return nullptr;

  ScriptableDocumentParser* scriptable_parser =
      document.GetScriptableDocumentParser();
  if (!scriptable_parser)
    return nullptr;

  SourceKeyedCachedMetadataHandler* document_cache_handler =
      scriptable_parser->GetInlineScriptCacheHandler();

  if (!document_cache_handler)
    return nullptr;

  return document_cache_handler->HandlerForSource(source);
}

ClassicScript* ClassicPendingScript::GetSource(const KURL& document_url) const {
  CheckState();
  DCHECK(IsReady());

  if (ready_state_ == kErrorOccurred)
    return nullptr;

  TRACE_EVENT0("blink", "ClassicPendingScript::GetSource");
  if (!is_external_) {
    SingleCachedMetadataHandler* cache_handler = nullptr;
    // We only create an inline cache handler for html-embedded scripts, not
    // for scripts produced by document.write, or not parser-inserted. This is
    // because we expect those to be too dynamic to benefit from caching.
    // TODO(leszeks): ScriptSourceLocationType was previously only used for UMA,
    // so it's a bit of a layer violation to use it for affecting cache
    // behaviour. We should decide whether it is ok for this parameter to be
    // used for behavioural changes (and if yes, update its documentation), or
    // otherwise trigger this behaviour differently.
    if (source_location_type_ == ScriptSourceLocationType::kInline) {
      cache_handler = GetInlineCacheHandler(source_text_for_inline_script_,
                                            GetElement()->GetDocument());
    }

    DCHECK(!GetResource());
    RecordStreamingHistogram(GetSchedulingType(), false,
                             ScriptStreamer::kInlineScript);

    ScriptSourceCode source_code(source_text_for_inline_script_,
                                 source_location_type_, cache_handler,
                                 document_url, StartingPosition());
    return MakeGarbageCollected<ClassicScript>(
        source_code, base_url_for_inline_script_, options_,
        SanitizeScriptErrors::kDoNotSanitize);
  }

  DCHECK(GetResource()->IsLoaded());
  ScriptResource* resource = ToScriptResource(GetResource());

  auto* fetcher = GetElement()->GetDocument().ContextDocument()->Fetcher();
  // If the MIME check fails, which is considered as load failure.
  if (!AllowedByNosniff::MimeTypeAsScript(
          fetcher->GetUseCounter(), &fetcher->GetConsoleLogger(),
          resource->GetResponse(),
          AllowedByNosniff::MimeTypeCheck::kLaxForElement)) {
    return nullptr;
  }

  // Check if we can use the script streamer.
  bool streamer_ready = false;
  ScriptStreamer::NotStreamingReason not_streamed_reason =
      resource->NoStreamerReason();
  ScriptStreamer* streamer = resource->TakeStreamer();
  if (streamer) {
    DCHECK_EQ(not_streamed_reason, ScriptStreamer::kInvalid);
    if (streamer->StreamingSuppressed()) {
      not_streamed_reason = streamer->StreamingSuppressedReason();
    } else if (ready_state_ == kErrorOccurred) {
      not_streamed_reason = ScriptStreamer::kErrorOccurred;
    } else {
      // Streamer can be used to compile script.
      CHECK_EQ(ready_state_, kReady);
      not_streamed_reason = ScriptStreamer::kInvalid;
      streamer_ready = true;
    }
  }
  RecordStreamingHistogram(GetSchedulingType(), streamer_ready,
                           not_streamed_reason);

  TRACE_EVENT_WITH_FLOW1(TRACE_DISABLED_BY_DEFAULT("v8.compile"),
                         "ClassicPendingScript::GetSource", this,
                         TRACE_EVENT_FLAG_FLOW_IN, "not_streamed_reason",
                         not_streamed_reason);

  ScriptSourceCode source_code(streamer_ready ? streamer : nullptr, resource,
                               not_streamed_reason);
  // The base URL for external classic script is
  //
  // <spec href="https://html.spec.whatwg.org/C/#concept-script-base-url">
  // ... the URL from which the script was obtained, ...</spec>
  const KURL& base_url = source_code.Url();
  return MakeGarbageCollected<ClassicScript>(
      source_code, base_url, options_,
      resource->GetResponse().IsCorsSameOrigin()
          ? SanitizeScriptErrors::kDoNotSanitize
          : SanitizeScriptErrors::kSanitize);
}

bool ClassicPendingScript::IsReady() const {
  CheckState();
  return ready_state_ >= kReady;
}

void ClassicPendingScript::AdvanceReadyState(ReadyState new_ready_state) {
  // We will allow exactly these state transitions:
  //
  // kWaitingForResource -> [kReady, kErrorOccurred]
  switch (ready_state_) {
    case kWaitingForResource:
      CHECK(new_ready_state == kReady || new_ready_state == kErrorOccurred);
      break;
    case kReady:
    case kErrorOccurred:
      NOTREACHED();
      break;
  }

  bool old_is_ready = IsReady();
  ready_state_ = new_ready_state;

  // Did we transition into a 'ready' state?
  if (IsReady() && !old_is_ready && IsWatchingForLoad())
    PendingScriptFinished();
}

void ClassicPendingScript::OnPurgeMemory() {
  CheckState();
  // TODO(crbug.com/846951): the implementation of CancelStreaming() is
  // currently incorrect and consequently a call to this method was removed from
  // here.
}

void ClassicPendingScript::StartStreamingIfPossible() {
  // We can start streaming in two states: While still loading
  // (kWaitingForResource), or after having loaded (kReady).
  if (ready_state_ != kWaitingForResource && ready_state_ != kReady)
    return;

  Document* document = &GetElement()->GetDocument();
  if (!document || !document->GetFrame())
    return;

  // Parser blocking scripts tend to do a lot of work in the 'finished'
  // callbacks, while async + in-order scripts all do control-like activities
  // (like posting new tasks). Use the 'control' queue only for control tasks.
  // (More details in discussion for cl 500147.)
  auto task_type = GetSchedulingType() == ScriptSchedulingType::kParserBlocking
                       ? TaskType::kNetworking
                       : TaskType::kNetworkingControl;

  ReadyState ready_state_before_stream = ready_state_;
  ToScriptResource(GetResource())
      ->StartStreaming(document->GetTaskRunner(task_type));
  // We have to make sure that the ready state is not changed by starting
  // streaming, just in case we're relying on IsReady being false.
  CHECK_EQ(ready_state_before_stream, ready_state_);

  return;
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

/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style
    sheets and html pages from the web. It has a memory cache for these objects.
*/

#include "third_party/blink/renderer/core/loader/resource/script_resource.h"

#include <utility>

#include "services/network/public/mojom/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_memory_allocator_dump.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_process_memory_dump.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client_walker.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"

namespace blink {

namespace {

// Returns true if the given request context is a script-like destination
// defined in the Fetch spec:
// https://fetch.spec.whatwg.org/#request-destination-script-like
bool IsRequestContextSupported(mojom::RequestContextType request_context) {
  // TODO(nhiroki): Support |kRequestContextSharedWorker| for module loading for
  // shared workers (https://crbug.com/824646).
  // TODO(nhiroki): Support "audioworklet" and "paintworklet" destinations.
  switch (request_context) {
    case mojom::RequestContextType::SCRIPT:
    case mojom::RequestContextType::WORKER:
    case mojom::RequestContextType::SERVICE_WORKER:
      return true;
    default:
      break;
  }
  NOTREACHED() << "Incompatible request context type: " << request_context;
  return false;
}

}  // namespace

ScriptResource* ScriptResource::Fetch(FetchParameters& params,
                                      ResourceFetcher* fetcher,
                                      ResourceClient* client,
                                      StreamingAllowed streaming_allowed) {
  DCHECK_EQ(params.GetResourceRequest().GetFrameType(),
            network::mojom::RequestContextFrameType::kNone);
  DCHECK(IsRequestContextSupported(
      params.GetResourceRequest().GetRequestContext()));
  ScriptResource* resource = ToScriptResource(
      fetcher->RequestResource(params, ScriptResourceFactory(), client));

  if (streaming_allowed == kAllowStreaming) {
    // Start streaming the script as soon as we get it.
    if (RuntimeEnabledFeatures::ScriptStreamingOnPreloadEnabled()) {
      resource->StartStreaming(fetcher->Context().GetLoadingTaskRunner());
    }
  } else {
    // Advance the |streaming_state_| to kStreamingNotAllowed by calling
    // SetClientIsWaitingForFinished unless it is explicitly allowed.'
    //
    // Do this in a task rather than directly to make sure that we don't call
    // the finished callbacks of other clients synchronously.

    // TODO(leszeks): Previous behaviour, without script streaming, was to
    // synchronously notify the given client, with the assumption that other
    // clients were already finished. If this behaviour becomes necessary, we
    // would have to either check that streaming wasn't started (if that would
    // be a logic error), or cancel any existing streaming.
    fetcher->Context().GetLoadingTaskRunner()->PostTask(
        FROM_HERE, WTF::Bind(&ScriptResource::SetClientIsWaitingForFinished,
                             WrapWeakPersistent(resource)));
  }

  return resource;
}

ScriptResource::ScriptResource(
    const ResourceRequest& resource_request,
    const ResourceLoaderOptions& options,
    const TextResourceDecoderOptions& decoder_options)
    : TextResource(resource_request,
                   ResourceType::kScript,
                   options,
                   decoder_options) {}

ScriptResource::~ScriptResource() = default;

void ScriptResource::Trace(blink::Visitor* visitor) {
  visitor->Trace(streamer_);
  TextResource::Trace(visitor);
}

void ScriptResource::OnMemoryDump(WebMemoryDumpLevelOfDetail level_of_detail,
                                  WebProcessMemoryDump* memory_dump) const {
  Resource::OnMemoryDump(level_of_detail, memory_dump);
  const String name = GetMemoryDumpName() + "/decoded_script";
  auto* dump = memory_dump->CreateMemoryAllocatorDump(name);
  dump->AddScalar("size", "bytes", source_text_.CharactersSizeInBytes());
  memory_dump->AddSuballocation(
      dump->Guid(), String(WTF::Partitions::kAllocatedObjectPoolName));
}

const ParkableString& ScriptResource::SourceText() {
  CHECK(IsFinishedInternal());

  if (source_text_.IsNull() && Data()) {
    String source_text = DecodedText();
    ClearData();
    SetDecodedSize(source_text.CharactersSizeInBytes());
    source_text_ = ParkableString(source_text.ReleaseImpl());
  }

  return source_text_;
}

SingleCachedMetadataHandler* ScriptResource::CacheHandler() {
  return static_cast<SingleCachedMetadataHandler*>(Resource::CacheHandler());
}

CachedMetadataHandler* ScriptResource::CreateCachedMetadataHandler(
    std::unique_ptr<CachedMetadataSender> send_callback) {
  return MakeGarbageCollected<ScriptCachedMetadataHandler>(
      Encoding(), std::move(send_callback));
}

void ScriptResource::SetSerializedCachedMetadata(const char* data,
                                                 size_t size) {
  Resource::SetSerializedCachedMetadata(data, size);
  ScriptCachedMetadataHandler* cache_handler =
      static_cast<ScriptCachedMetadataHandler*>(Resource::CacheHandler());
  if (cache_handler) {
    cache_handler->SetSerializedCachedMetadata(data, size);
  }
}

void ScriptResource::DestroyDecodedDataForFailedRevalidation() {
  source_text_ = ParkableString();
  // Make sure there's no streaming.
  DCHECK(!streamer_);
  DCHECK_EQ(streaming_state_, StreamingState::kStreamingNotAllowed);
  SetDecodedSize(0);
}

void ScriptResource::SetRevalidatingRequest(const ResourceRequest& request) {
  CHECK_EQ(streaming_state_, StreamingState::kFinishedNotificationSent);
  if (streamer_) {
    CHECK(streamer_->IsStreamingFinished());
    streamer_ = nullptr;
  }
  // Revalidation requests don't actually load the current Resource, so disable
  // streaming.
  not_streaming_reason_ = ScriptStreamer::kRevalidate;
  streaming_state_ = StreamingState::kStreamingNotAllowed;
  CheckStreamingState();

  TextResource::SetRevalidatingRequest(request);
}

bool ScriptResource::CanUseCacheValidator() const {
  // Do not revalidate until ClassicPendingScript is removed, i.e. the script
  // content is retrieved in ScriptLoader::ExecuteScriptBlock().
  // crbug.com/692856
  if (HasClientsOrObservers())
    return false;

  // Do not revalidate until streaming is complete.
  if (!IsFinishedInternal())
    return false;

  return Resource::CanUseCacheValidator();
}

void ScriptResource::NotifyDataReceived(const char* data, size_t size) {
  CheckStreamingState();
  if (streamer_) {
    DCHECK_EQ(streaming_state_, StreamingState::kStreaming);
    streamer_->NotifyAppendData();
  }
  TextResource::NotifyDataReceived(data, size);
}

void ScriptResource::NotifyFinished() {
  DCHECK(IsLoaded());
  switch (streaming_state_) {
    case StreamingState::kCanStartStreaming:
      // Do nothing, expect either a StartStreaming() call to transition us to
      // kStreaming, or an SetClientIsWaitingForFinished() call to transition us
      // into kStreamingNotAllowed. These will then transition again since
      // IsLoaded will be true.
      break;
    case StreamingState::kStreaming:
      AdvanceStreamingState(StreamingState::kWaitingForStreamingToEnd);
      DCHECK(streamer_);
      streamer_->NotifyFinished();
      // Don't call the base NotifyFinished until streaming finishes too (which
      // might happen immediately in the above ScriptStreamer::NotifyFinished
      // call)
      break;
    case StreamingState::kStreamingNotAllowed:
      AdvanceStreamingState(StreamingState::kFinishedNotificationSent);
      TextResource::NotifyFinished();
      break;
    case StreamingState::kWaitingForStreamingToEnd:
    case StreamingState::kFinishedNotificationSent:
      // Not possible.
      CHECK(false);
      break;
  }
}

bool ScriptResource::IsFinishedInternal() const {
  CheckStreamingState();
  return streaming_state_ == StreamingState::kFinishedNotificationSent;
}

void ScriptResource::StreamingFinished() {
  CHECK(streamer_);
  CHECK_EQ(streaming_state_, StreamingState::kWaitingForStreamingToEnd);
  AdvanceStreamingState(StreamingState::kFinishedNotificationSent);
  TextResource::NotifyFinished();
}

bool ScriptResource::StartStreaming(
    scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner) {
  CheckStreamingState();

  if (streamer_) {
    return !streamer_->IsStreamingFinished();
  }

  if (streaming_state_ != StreamingState::kCanStartStreaming) {
    return false;
  }

  // Don't bother streaming if there was an error, it won't work anyway.
  if (ErrorOccurred()) {
    return false;
  }

  CHECK(!IsCacheValidator());

  streamer_ =
      ScriptStreamer::Create(this, loading_task_runner, &not_streaming_reason_);
  if (streamer_) {
    AdvanceStreamingState(StreamingState::kStreaming);

    // If there is any data already, send it to the streamer.
    if (Data()) {
      // Note that we don't need to iterate through the segments of the data, as
      // the streamer will do that itself.
      CHECK_GT(Data()->size(), 0u);
      streamer_->NotifyAppendData();
    }
    // If the we're is already loaded, notify the streamer about that too.
    if (IsLoaded()) {
      AdvanceStreamingState(StreamingState::kWaitingForStreamingToEnd);

      // Do this in a task rather than directly to make sure that we don't call
      // the finished callback in the same stack as starting streaming -- this
      // can cause issues with the client expecting to be not finished when
      // starting streaming (e.g. ClassicPendingScript::IsReady == false), but
      // ending up finished by the end of this method.
      loading_task_runner->PostTask(FROM_HERE,
                                    WTF::Bind(&ScriptStreamer::NotifyFinished,
                                              WrapPersistent(streamer_.Get())));
    }
  }

  CheckStreamingState();
  return streamer_ && !streamer_->IsStreamingFinished();
}

void ScriptResource::SetClientIsWaitingForFinished() {
  // No-op if streaming already started or finished.
  CheckStreamingState();
  if (streaming_state_ != StreamingState::kCanStartStreaming)
    return;

  AdvanceStreamingState(StreamingState::kStreamingNotAllowed);
  not_streaming_reason_ = ScriptStreamer::kStreamingDisabled;
  // Trigger the finished notification if needed.
  if (IsLoaded()) {
    AdvanceStreamingState(StreamingState::kFinishedNotificationSent);
    TextResource::NotifyFinished();
  }
}

ScriptStreamer* ScriptResource::TakeStreamer() {
  CHECK(IsFinishedInternal());
  if (!streamer_)
    return nullptr;

  ScriptStreamer* streamer = streamer_;
  streamer_ = nullptr;
  not_streaming_reason_ = ScriptStreamer::kSecondScriptResourceUse;
  return streamer;
}

void ScriptResource::AdvanceStreamingState(StreamingState new_state) {
  switch (streaming_state_) {
    case StreamingState::kCanStartStreaming:
      CHECK(new_state == StreamingState::kStreaming ||
            new_state == StreamingState::kStreamingNotAllowed);
      break;
    case StreamingState::kStreaming:
      CHECK(streamer_);
      CHECK_EQ(new_state, StreamingState::kWaitingForStreamingToEnd);
      break;
    case StreamingState::kWaitingForStreamingToEnd:
      CHECK(streamer_);
      CHECK_EQ(new_state, StreamingState::kFinishedNotificationSent);
      break;
    case StreamingState::kStreamingNotAllowed:
      CHECK_EQ(new_state, StreamingState::kFinishedNotificationSent);
      break;
    case StreamingState::kFinishedNotificationSent:
      CHECK(false);
      break;
  }

  streaming_state_ = new_state;
  CheckStreamingState();
}

void ScriptResource::CheckStreamingState() const {
  // TODO(leszeks): Eventually convert these CHECKs into DCHECKs once the logic
  // is a bit more baked in.
  switch (streaming_state_) {
    case StreamingState::kCanStartStreaming:
      CHECK(!streamer_);
      break;
    case StreamingState::kStreaming:
      CHECK(streamer_);
      CHECK(!streamer_->IsFinished());
      // kStreaming can be entered both when loading (if streaming is started
      // before load completes) or when loaded (if streaming is started after
      // load completes). In the latter case, the state will almost immediately
      // advance to kWaitingForStreamingToEnd.
      CHECK(IsLoaded() || IsLoading());
      break;
    case StreamingState::kWaitingForStreamingToEnd:
      CHECK(streamer_);
      CHECK(!streamer_->IsFinished());
      CHECK(IsLoaded());
      break;
    case StreamingState::kStreamingNotAllowed:
      CHECK(!streamer_);
      // TODO(leszeks): We could CHECK(!IsLoaded()) if not for the immediate
      // kCanStartStreaming -> kStreamingNotAllowed -> kFinishedNotificationSent
      // transition in SetClientIsWaitingForFinished when IsLoaded.
      break;
    case StreamingState::kFinishedNotificationSent:
      CHECK(!streamer_ || streamer_->IsFinished());
      CHECK(IsLoaded());
      break;
  }
}

}  // namespace blink

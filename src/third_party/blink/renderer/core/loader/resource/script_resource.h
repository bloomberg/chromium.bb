/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_SCRIPT_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_SCRIPT_RESOURCE_H_

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/resource/text_resource.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"

namespace blink {

class FetchParameters;
class KURL;
class ResourceFetcher;
class SingleCachedMetadataHandler;

// ScriptResource is a resource representing a JavaScript, either a classic or
// module script.
//
// In addition to loading the script, a ScriptResource can optionally stream the
// script to the JavaScript parser/compiler, using a ScriptStreamer. In this
// case, clients of the ScriptResource will not receive the finished
// notification until the streaming completes.
// Note: ScriptStreamer is only used for "classic" scripts, i.e. not modules.
//
// See also:
// https://docs.google.com/document/d/143GOPl_XVgLPFfO-31b_MdBcnjklLEX2OIg_6eN6fQ4
class CORE_EXPORT ScriptResource final : public TextResource {
 public:
  // The script resource will always try to start streaming if kAllowStreaming
  // is passed in.
  enum StreamingAllowed { kNoStreaming, kAllowStreaming };

  static ScriptResource* Fetch(FetchParameters&,
                               ResourceFetcher*,
                               ResourceClient*,
                               StreamingAllowed);

  // Public for testing
  static ScriptResource* CreateForTest(const KURL& url,
                                       const WTF::TextEncoding& encoding);

  ScriptResource(const ResourceRequest&,
                 const ResourceLoaderOptions&,
                 const TextResourceDecoderOptions&,
                 StreamingAllowed);
  ~ScriptResource() override;

  void ResponseBodyReceived(
      ResponseBodyLoaderDrainableInterface& body_loader,
      scoped_refptr<base::SingleThreadTaskRunner> loader_task_runner) override;

  void Trace(Visitor*) const override;

  void OnMemoryDump(WebMemoryDumpLevelOfDetail,
                    WebProcessMemoryDump*) const override;

  void SetSerializedCachedMetadata(mojo_base::BigBuffer data) override;

  const ParkableString& SourceText();

  // Get the resource's current text. This can return partial data, so should
  // not be used outside of the inspector.
  String TextForInspector() const;

  SingleCachedMetadataHandler* CacheHandler();

  // Gets the script streamer from the ScriptResource, clearing the resource's
  // streamer so that it cannot be used twice.
  ScriptStreamer* TakeStreamer();

  ScriptStreamer::NotStreamingReason NoStreamerReason() const {
    return no_streamer_reason_;
  }

  // Used in DCHECKs
  bool HasStreamer() { return !!streamer_; }
  bool HasRunningStreamer() { return streamer_ && !streamer_->IsFinished(); }
  bool HasFinishedStreamer() { return streamer_ && streamer_->IsFinished(); }

  // Visible for tests.
  void SetRevalidatingRequest(const ResourceRequestHead&) override;

 protected:
  CachedMetadataHandler* CreateCachedMetadataHandler(
      std::unique_ptr<CachedMetadataSender> send_callback) override;

  void DestroyDecodedDataForFailedRevalidation() override;

  // ScriptResources are considered finished when either:
  //   1. Loading + streaming completes, or
  //   2. Loading completes + streaming is disabled.
  void NotifyFinished() override;

 private:
  // Valid state transitions:
  //
  //            kWaitingForDataPipe          DisableStreaming()
  //                    |---------------------------.
  //                    |                           |
  //                    v                           v
  //               kStreaming -----------------> kStreamingDisabled
  //
  enum class StreamingState {
    // Streaming is allowed on this resource, but we're waiting to receive a
    // data pipe.
    kWaitingForDataPipe,
    // The script streamer is active, and has the data pipe.
    kStreaming,

    // Streaming was disabled, either manually or because we got a body with
    // no data-pipe.
    kStreamingDisabled,
  };

  class ScriptResourceFactory : public ResourceFactory {
   public:
    explicit ScriptResourceFactory(StreamingAllowed streaming_allowed)
        : ResourceFactory(ResourceType::kScript,
                          TextResourceDecoderOptions::kPlainTextContent),
          streaming_allowed_(streaming_allowed) {}

    Resource* Create(
        const ResourceRequest& request,
        const ResourceLoaderOptions& options,
        const TextResourceDecoderOptions& decoder_options) const override {
      return MakeGarbageCollected<ScriptResource>(
          request, options, decoder_options, streaming_allowed_);
    }

   private:
    StreamingAllowed streaming_allowed_;
  };

  bool CanUseCacheValidator() const override;

  void DisableStreaming(ScriptStreamer::NotStreamingReason no_streamer_reason);

  void AdvanceStreamingState(StreamingState new_state);

  // Check that invariants for the state hold.
  void CheckStreamingState() const;

  void OnDataPipeReadable(MojoResult result,
                          const mojo::HandleSignalsState& state);

  ParkableString source_text_;

  Member<ScriptStreamer> streamer_;
  ScriptStreamer::NotStreamingReason no_streamer_reason_ =
      ScriptStreamer::NotStreamingReason::kInvalid;
  StreamingState streaming_state_ = StreamingState::kWaitingForDataPipe;
};

DEFINE_RESOURCE_TYPE_CASTS(Script);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_SCRIPT_RESOURCE_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClassicPendingScript_h
#define ClassicPendingScript_h

#include "bindings/core/v8/ScriptSourceLocationType.h"
#include "bindings/core/v8/ScriptStreamer.h"
#include "core/loader/resource/ScriptResource.h"
#include "core/script/ClassicScript.h"
#include "core/script/PendingScript.h"
#include "platform/MemoryCoordinator.h"
#include "platform/loader/fetch/FetchParameters.h"

namespace blink {

// PendingScript for a classic script
// https://html.spec.whatwg.org/#classic-script.
//
// TODO(kochi): The comment below is from pre-oilpan age and may not be correct
// now.
// A RefPtr alone does not prevent the underlying Resource from purging its data
// buffer. This class holds a dummy client open for its lifetime in order to
// guarantee that the data buffer will not be purged.
class CORE_EXPORT ClassicPendingScript final : public PendingScript,
                                               public ResourceClient,
                                               public MemoryCoordinatorClient {
  USING_GARBAGE_COLLECTED_MIXIN(ClassicPendingScript);
  USING_PRE_FINALIZER(ClassicPendingScript, Prefinalize);

 public:
  // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-classic-script
  //
  // For a script from an external file, calls ScriptResource::Fetch() and
  // creates ClassicPendingScript. Returns nullptr if Fetch() returns nullptr.
  static ClassicPendingScript* Fetch(const KURL&,
                                     Document&,
                                     const ScriptFetchOptions&,
                                     const WTF::TextEncoding&,
                                     ScriptElementBase*,
                                     FetchParameters::DeferOption);

  // For an inline script.
  static ClassicPendingScript* CreateInline(ScriptElementBase*,
                                            const TextPosition&,
                                            ScriptSourceLocationType,
                                            const ScriptFetchOptions&);

  ~ClassicPendingScript() override;

  // ScriptStreamer callbacks.
  void SetStreamer(ScriptStreamer*);
  void StreamingFinished();

  void Trace(blink::Visitor*);

  blink::ScriptType GetScriptType() const override {
    return blink::ScriptType::kClassic;
  }

  bool CheckMIMETypeBeforeRunScript(Document* context_document) const override;
  ClassicScript* GetSource(const KURL& document_url,
                           bool& error_occurred) const override;
  bool IsReady() const override;
  bool IsExternal() const override { return is_external_; }
  bool ErrorOccurred() const override;
  bool WasCanceled() const override;
  bool StartStreamingIfPossible(ScriptStreamer::Type,
                                base::OnceClosure) override;
  bool IsCurrentlyStreaming() const override;
  KURL UrlForTracing() const override;
  void DisposeInternal() override;

  void Prefinalize();

 private:
  // See AdvanceReadyState implementation for valid state transitions.
  enum ReadyState {
    // These states are considered "not ready".
    kWaitingForResource,
    kWaitingForStreaming,
    // These states are considered "ready".
    kReady,
    kReadyStreaming,
    kErrorOccurred,
  };

  ClassicPendingScript(ScriptElementBase*,
                       const TextPosition&,
                       ScriptSourceLocationType,
                       const ScriptFetchOptions&,
                       bool is_external);
  ClassicPendingScript() = delete;

  // Advances the current state of the script, reporting to the client if
  // appropriate.
  void AdvanceReadyState(ReadyState);

  // Handle the end of streaming.
  void FinishWaitingForStreaming();
  void FinishReadyStreaming();
  void CancelStreaming();

  void CheckState() const override;

  // ResourceClient
  void NotifyFinished(Resource*) override;
  String DebugName() const override { return "PendingScript"; }
  void DataReceived(Resource*, const char*, size_t) override;

  // MemoryCoordinatorClient
  void OnPurgeMemory() override;

  const ScriptFetchOptions options_;

  // "base url" snapshot taken at #prepare-a-script timing.
  // https://html.spec.whatwg.org/#prepare-a-script
  // which will eventually be used as #concept-script-base-url.
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-base-url
  const KURL base_url_for_inline_script_;

  const ScriptSourceLocationType source_location_type_;
  const bool is_external_;
  ReadyState ready_state_;
  bool integrity_failure_;

  // The request is intervened by document.write() intervention.
  bool intervened_ = false;

  Member<ScriptStreamer> streamer_;
  base::OnceClosure streamer_done_;

  // This flag tracks whether streamer_ is currently streaming. It is used
  // mainly to prevent re-streaming a script while it is being streamed.
  //
  // ReadyState unfortunately doesn't contain this information, because
  // 1, the WaitingFor* states can occur with or without streaming, and
  // 2, during the state transition, we need to first transition ready_state_,
  //    then run callbacks, and only then consider the streaming done. So
  //    during AdvanceReadyState and callback processing, the ready state
  //    and is_currently_streaming_ are temporarily different. (They must
  //    be consistent before and after AdvanceReadyState.)
  //
  // (See also: crbug.com/754360)
  bool is_currently_streaming_;

  // This is a temporary flag to confirm that ClassicPendingScript is not
  // touched after its refinalizer call and thus https://crbug.com/715309
  // doesn't break assumptions.
  // TODO(hiroshige): Check the state in more general way.
  bool prefinalizer_called_ = false;
};

}  // namespace blink

#endif  // PendingScript_h

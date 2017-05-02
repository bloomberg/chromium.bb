// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClassicPendingScript_h
#define ClassicPendingScript_h

#include "bindings/core/v8/ScriptStreamer.h"
#include "core/dom/ClassicScript.h"
#include "core/dom/PendingScript.h"
#include "core/loader/resource/ScriptResource.h"
#include "platform/MemoryCoordinator.h"
#include "platform/loader/fetch/ResourceOwner.h"

namespace blink {

// PendingScript for a classic script
// https://html.spec.whatwg.org/#classic-script.
//
// TODO(kochi): The comment below is from pre-oilpan age and may not be correct
// now.
// A RefPtr alone does not prevent the underlying Resource from purging its data
// buffer. This class holds a dummy client open for its lifetime in order to
// guarantee that the data buffer will not be purged.
class CORE_EXPORT ClassicPendingScript final
    : public PendingScript,
      public ResourceOwner<ScriptResource>,
      public MemoryCoordinatorClient {
  USING_GARBAGE_COLLECTED_MIXIN(ClassicPendingScript);
  USING_PRE_FINALIZER(ClassicPendingScript, Prefinalize);

 public:
  // For script from an external file.
  static ClassicPendingScript* Create(ScriptElementBase*, ScriptResource*);
  // For inline script.
  static ClassicPendingScript* Create(ScriptElementBase*, const TextPosition&);

  ~ClassicPendingScript() override;

  void SetStreamer(ScriptStreamer*);
  void StreamingFinished();

  DECLARE_TRACE();

  blink::ScriptType GetScriptType() const override {
    return blink::ScriptType::kClassic;
  }

  ClassicScript* GetSource(const KURL& document_url,
                           bool& error_occurred) const override;
  bool IsReady() const override;
  bool IsExternal() const override { return GetResource(); }
  bool ErrorOccurred() const override;
  bool WasCanceled() const override;
  void StartStreamingIfPossible(Document*, ScriptStreamer::Type) override;
  KURL UrlForClassicScript() const override;
  void RemoveFromMemoryCache() override;
  void DisposeInternal() override;

  void Prefinalize();

 private:
  enum ReadyState {
    // These states are considered "not ready".
    kWaitingForResource,
    kWaitingForStreaming,
    // These states are considered "ready".
    kReady,
    kErrorOccurred,
  };

  ClassicPendingScript(ScriptElementBase*,
                       ScriptResource*,
                       const TextPosition&);
  ClassicPendingScript() = delete;

  // Advances the current state of the script, reporting to the client if
  // appropriate.
  void AdvanceReadyState(ReadyState);

  void CheckState() const override;

  // ScriptResourceClient
  void NotifyFinished(Resource*) override;
  String DebugName() const override { return "PendingScript"; }
  void NotifyAppendData(ScriptResource*) override;

  // MemoryCoordinatorClient
  void OnPurgeMemory() override;

  ReadyState ready_state_;
  bool integrity_failure_;
  Member<ScriptStreamer> streamer_;

  // This is a temporary flag to confirm that ClassicPendingScript is not
  // touched after its refinalizer call and thus https://crbug.com/715309
  // doesn't break assumptions.
  // TODO(hiroshige): Check the state in more general way.
  bool prefinalizer_called_ = false;
};

}  // namespace blink

#endif  // PendingScript_h

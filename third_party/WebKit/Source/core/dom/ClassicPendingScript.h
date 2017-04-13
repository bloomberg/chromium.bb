// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClassicPendingScript_h
#define ClassicPendingScript_h

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

 public:
  // For script from an external file.
  static ClassicPendingScript* Create(ScriptElementBase*, ScriptResource*);
  // For inline script.
  static ClassicPendingScript* Create(ScriptElementBase*, const TextPosition&);

  static ClassicPendingScript* CreateForTesting(ScriptResource*);

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
  KURL Url() const override;
  bool IsExternal() const override { return GetResource(); }
  bool ErrorOccurred() const override;
  bool WasCanceled() const override;
  void StartStreamingIfPossible(Document*, ScriptStreamer::Type) override;
  void RemoveFromMemoryCache() override;
  void DisposeInternal() override;

 private:
  ClassicPendingScript(ScriptElementBase*,
                       ScriptResource*,
                       const TextPosition&,
                       bool is_for_testing = false);
  ClassicPendingScript() = delete;

  void CheckState() const override;

  // ScriptResourceClient
  void NotifyFinished(Resource*) override;
  String DebugName() const override { return "PendingScript"; }
  void NotifyAppendData(ScriptResource*) override;

  // MemoryCoordinatorClient
  void OnPurgeMemory() override;

  bool integrity_failure_;

  Member<ScriptStreamer> streamer_;

  // This flag is used to skip non-null checks of |m_element| in unit
  // tests, because |m_element| can be null in unit tests.
  const bool is_for_testing_;
};

}  // namespace blink

#endif  // PendingScript_h

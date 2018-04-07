/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTORTRACINGAGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTORTRACINGAGENT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/InspectorBaseAgent.h"
#include "third_party/blink/renderer/core/inspector/protocol/Tracing.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class InspectedFrames;
class WorkerInspectorProxy;

class CORE_EXPORT InspectorTracingAgent final
    : public InspectorBaseAgent<protocol::Tracing::Metainfo> {
 public:
  class Client {
   public:
    virtual ~Client() = default;

    virtual void ShowReloadingBlanket() = 0;
    virtual void HideReloadingBlanket() = 0;
  };

  InspectorTracingAgent(Client*, InspectedFrames*);
  ~InspectorTracingAgent() override;

  void Trace(blink::Visitor*) override;

  // Base agent methods.
  void Restore() override;
  protocol::Response disable() override;

  // InspectorInstrumentation methods
  void FrameStartedLoading(LocalFrame*, FrameLoadType);
  void FrameStoppedLoading(LocalFrame*);
  void DidStartWorker(WorkerInspectorProxy*, bool);

  // Protocol method implementations.
  void start(protocol::Maybe<String> categories,
             protocol::Maybe<String> options,
             protocol::Maybe<double> buffer_usage_reporting_interval,
             protocol::Maybe<String> transfer_mode,
             protocol::Maybe<String> transfer_compression,
             protocol::Maybe<protocol::Tracing::TraceConfig>,
             std::unique_ptr<StartCallback>) override;
  void end(std::unique_ptr<EndCallback>) override;

  // Methods for other agents to use.
  void RootLayerCleared();

 private:
  void EmitMetadataEvents();
  void InnerDisable();
  bool IsStarted() const;

  Client* client_;
  String session_id_;
  Member<InspectedFrames> inspected_frames_;

  DISALLOW_COPY_AND_ASSIGN(InspectorTracingAgent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTORTRACINGAGENT_H_

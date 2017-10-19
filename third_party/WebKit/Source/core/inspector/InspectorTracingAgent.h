/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef InspectorTracingAgent_h
#define InspectorTracingAgent_h

#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Tracing.h"
#include "core/loader/FrameLoaderTypes.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class InspectedFrames;
class InspectorWorkerAgent;

class CORE_EXPORT InspectorTracingAgent final
    : public InspectorBaseAgent<protocol::Tracing::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorTracingAgent);

 public:
  class Client {
   public:
    virtual ~Client() {}

    virtual void EnableTracing(const String& category_filter) = 0;
    virtual void DisableTracing() = 0;
    virtual void ShowReloadingBlanket() = 0;
    virtual void HideReloadingBlanket() = 0;
  };

  static InspectorTracingAgent* Create(Client* client,
                                       InspectorWorkerAgent* worker_agent,
                                       InspectedFrames* inspected_frames) {
    return new InspectorTracingAgent(client, worker_agent, inspected_frames);
  }

  virtual void Trace(blink::Visitor*);

  // Base agent methods.
  void Restore() override;
  protocol::Response disable() override;

  // InspectorInstrumentation methods
  void FrameStartedLoading(LocalFrame*, FrameLoadType);
  void FrameStoppedLoading(LocalFrame*);

  // Protocol method implementations.
  void start(protocol::Maybe<String> categories,
             protocol::Maybe<String> options,
             protocol::Maybe<double> buffer_usage_reporting_interval,
             protocol::Maybe<String> transfer_mode,
             protocol::Maybe<protocol::Tracing::TraceConfig>,
             std::unique_ptr<StartCallback>) override;
  void end(std::unique_ptr<EndCallback>) override;

  // Methods for other agents to use.
  void SetLayerTreeId(int);
  void RootLayerCleared();

 private:
  InspectorTracingAgent(Client*, InspectorWorkerAgent*, InspectedFrames*);

  void EmitMetadataEvents();
  void InnerDisable();
  String SessionId() const;
  bool IsStarted() const;

  int layer_tree_id_;
  Client* client_;
  Member<InspectorWorkerAgent> worker_agent_;
  Member<InspectedFrames> inspected_frames_;
};

}  // namespace blink

#endif  // InspectorTracingAgent_h

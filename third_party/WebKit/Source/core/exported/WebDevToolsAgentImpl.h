/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebDevToolsAgentImpl_h
#define WebDevToolsAgentImpl_h

#include <memory>

#include "core/CoreExport.h"
#include "core/inspector/InspectorLayerTreeAgent.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorTracingAgent.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebThread.h"
#include "public/web/devtools_agent.mojom-blink.h"

namespace blink {

class ClientMessageLoopAdapter;
class GraphicsLayer;
class InspectedFrames;
class InspectorResourceContainer;
class InspectorResourceContentLoader;
class LocalFrame;
class WebLocalFrameImpl;

class CORE_EXPORT WebDevToolsAgentImpl final
    : public GarbageCollectedFinalized<WebDevToolsAgentImpl>,
      public mojom::blink::DevToolsAgent,
      public InspectorTracingAgent::Client,
      public InspectorPageAgent::Client,
      public InspectorLayerTreeAgent::Client,
      private WebThread::TaskObserver {
 public:
  class WorkerClient {
   public:
    virtual ~WorkerClient() {}
    virtual void ResumeStartup() = 0;
  };

  static WebDevToolsAgentImpl* CreateForFrame(WebLocalFrameImpl*);
  static WebDevToolsAgentImpl* CreateForWorker(WebLocalFrameImpl*,
                                               WorkerClient*);
  ~WebDevToolsAgentImpl() override;
  virtual void Trace(blink::Visitor*);

  void WillBeDestroyed();
  void FlushProtocolNotifications();
  void PaintOverlay();
  void LayoutOverlay();
  bool HandleInputEvent(const WebInputEvent&);
  void DispatchBufferedTouchEvents();
  void BindRequest(mojom::blink::DevToolsAgentAssociatedRequest);

  // Instrumentation from web/ layer.
  void DidCommitLoadForLocalFrame(LocalFrame*);
  void DidStartProvisionalLoad(LocalFrame*);
  bool ScreencastEnabled();
  void RootLayerCleared();
  String NavigationInitiatorInfo(LocalFrame*);
  String EvaluateInOverlayForTesting(const String& script);

 private:
  friend class ClientMessageLoopAdapter;
  class Session;

  WebDevToolsAgentImpl(WebLocalFrameImpl*,
                       bool include_view_agents,
                       WorkerClient*);

  // mojom::blink::DevToolsAgent implementation.
  void AttachDevToolsSession(
      mojom::blink::DevToolsSessionHostAssociatedPtrInfo,
      mojom::blink::DevToolsSessionAssociatedRequest main_session,
      mojom::blink::DevToolsSessionRequest io_session,
      const String& reattach_state) override;
  void InspectElement(const WebPoint& point_in_local_root) override;

  // InspectorTracingAgent::Client implementation.
  void ShowReloadingBlanket() override;
  void HideReloadingBlanket() override;

  // InspectorPageAgent::Client implementation.
  void PageLayoutInvalidated(bool resized) override;

  // InspectorLayerTreeAgent::Client implementation.
  bool IsInspectorLayer(GraphicsLayer*) override;

  // WebThread::TaskObserver implementation.
  void WillProcessTask() override;
  void DidProcessTask() override;

  void DetachSession(Session*);

  mojo::AssociatedBinding<mojom::blink::DevToolsAgent> binding_;
  HeapHashSet<Member<Session>> sessions_;
  WorkerClient* worker_client_;
  Member<WebLocalFrameImpl> web_local_frame_impl_;
  Member<CoreProbeSink> probe_sink_;
  Member<InspectorResourceContentLoader> resource_content_loader_;
  Member<InspectedFrames> inspected_frames_;
  Member<InspectorResourceContainer> resource_container_;
  Member<Node> node_to_inspect_;
  bool include_view_agents_;
};

}  // namespace blink

#endif

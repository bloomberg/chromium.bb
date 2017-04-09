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

#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorSession.h"
#include "core/inspector/InspectorTracingAgent.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Vector.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebThread.h"
#include "public/web/WebDevToolsAgent.h"
#include "web/InspectorEmulationAgent.h"

namespace blink {

class GraphicsLayer;
class InspectedFrames;
class InspectorOverlay;
class InspectorResourceContainer;
class InspectorResourceContentLoader;
class InspectorTraceEvents;
class LocalFrame;
class WebDevToolsAgentClient;
class WebLayerTreeView;
class WebLocalFrameImpl;
class WebString;

class WebDevToolsAgentImpl final
    : public GarbageCollectedFinalized<WebDevToolsAgentImpl>,
      public WebDevToolsAgent,
      public InspectorEmulationAgent::Client,
      public InspectorTracingAgent::Client,
      public InspectorPageAgent::Client,
      public InspectorSession::Client,
      private WebThread::TaskObserver {
 public:
  static WebDevToolsAgentImpl* Create(WebLocalFrameImpl*,
                                      WebDevToolsAgentClient*);
  ~WebDevToolsAgentImpl() override;
  DECLARE_VIRTUAL_TRACE();

  void WillBeDestroyed();
  WebDevToolsAgentClient* Client() { return client_; }
  InspectorOverlay* Overlay() const { return overlay_.Get(); }
  void FlushProtocolNotifications();

  // Instrumentation from web/ layer.
  void DidCommitLoadForLocalFrame(LocalFrame*);
  void DidStartProvisionalLoad(LocalFrame*);
  bool ScreencastEnabled();
  void WillAddPageOverlay(const GraphicsLayer*);
  void DidRemovePageOverlay(const GraphicsLayer*);
  void LayerTreeViewChanged(WebLayerTreeView*);
  void RootLayerCleared();

  // WebDevToolsAgent implementation.
  void Attach(const WebString& host_id, int session_id) override;
  void Reattach(const WebString& host_id,
                int session_id,
                const WebString& saved_state) override;
  void Detach() override;
  void ContinueProgram() override;
  void DispatchOnInspectorBackend(int session_id,
                                  int call_id,
                                  const WebString& method,
                                  const WebString& message) override;
  void InspectElementAt(int session_id, const WebPoint&) override;
  void FailedToRequestDevTools() override;
  WebString EvaluateInWebInspectorOverlay(const WebString& script) override;
  bool CacheDisabled() override;

 private:
  WebDevToolsAgentImpl(WebLocalFrameImpl*,
                       WebDevToolsAgentClient*,
                       InspectorOverlay*,
                       bool include_view_agents);

  // InspectorTracingAgent::Client implementation.
  void EnableTracing(const WTF::String& category_filter) override;
  void DisableTracing() override;
  void ShowReloadingBlanket() override;
  void HideReloadingBlanket() override;

  // InspectorEmulationAgent::Client implementation.
  void SetCPUThrottlingRate(double) override;

  // InspectorPageAgent::Client implementation.
  void PageLayoutInvalidated(bool resized) override;
  void ConfigureOverlay(bool suspended, const String& message) override;
  void WaitForCreateWindow(LocalFrame*) override;

  // InspectorSession::Client implementation.
  void SendProtocolMessage(int session_id,
                           int call_id,
                           const String& response,
                           const String& state) override;

  // WebThread::TaskObserver implementation.
  void WillProcessTask() override;
  void DidProcessTask() override;

  void InitializeSession(int session_id, const String& host_id, String* state);
  void DestroySession();
  void DispatchMessageFromFrontend(int session_id,
                                   const String& method,
                                   const String& message);

  friend class WebDevToolsAgent;
  static void RunDebuggerTask(
      int session_id,
      std::unique_ptr<WebDevToolsAgent::MessageDescriptor>);

  bool Attached() const { return session_.Get(); }

  WebDevToolsAgentClient* client_;
  Member<WebLocalFrameImpl> web_local_frame_impl_;

  Member<CoreProbeSink> instrumenting_agents_;
  Member<InspectorResourceContentLoader> resource_content_loader_;
  Member<InspectorOverlay> overlay_;
  Member<InspectedFrames> inspected_frames_;
  Member<InspectorResourceContainer> resource_container_;

  Member<InspectorDOMAgent> dom_agent_;
  Member<InspectorPageAgent> page_agent_;
  Member<InspectorNetworkAgent> network_agent_;
  Member<InspectorLayerTreeAgent> layer_tree_agent_;
  Member<InspectorTracingAgent> tracing_agent_;
  Member<InspectorTraceEvents> trace_events_agent_;

  Member<InspectorSession> session_;
  bool include_view_agents_;
  int layer_tree_id_;
};

}  // namespace blink

#endif

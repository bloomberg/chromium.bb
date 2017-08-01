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
#include "core/inspector/InspectorEmulationAgent.h"
#include "core/inspector/InspectorLayerTreeAgent.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorSession.h"
#include "core/inspector/InspectorTracingAgent.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Vector.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebThread.h"
#include "public/web/WebDevToolsAgent.h"

namespace blink {

class GraphicsLayer;
class InspectedFrames;
class InspectorOverlayAgent;
class InspectorResourceContainer;
class InspectorResourceContentLoader;
class InspectorTraceEvents;
class LocalFrame;
class WebDevToolsAgentClient;
class WebLayerTreeView;
class WebLocalFrameImpl;
class WebString;

class CORE_EXPORT WebDevToolsAgentImpl final
    : public GarbageCollectedFinalized<WebDevToolsAgentImpl>,
      public NON_EXPORTED_BASE(WebDevToolsAgent),
      public NON_EXPORTED_BASE(InspectorEmulationAgent::Client),
      public NON_EXPORTED_BASE(InspectorTracingAgent::Client),
      public NON_EXPORTED_BASE(InspectorPageAgent::Client),
      public NON_EXPORTED_BASE(InspectorSession::Client),
      public NON_EXPORTED_BASE(InspectorLayerTreeAgent::Client),
      private WebThread::TaskObserver {
 public:
  static WebDevToolsAgentImpl* Create(WebLocalFrameImpl*,
                                      WebDevToolsAgentClient*);
  ~WebDevToolsAgentImpl() override;
  DECLARE_VIRTUAL_TRACE();

  void WillBeDestroyed();
  WebDevToolsAgentClient* Client() { return client_; }
  void FlushProtocolNotifications();
  void PaintOverlay();
  void LayoutOverlay();
  bool HandleInputEvent(const WebInputEvent&);

  // Instrumentation from web/ layer.
  void DidCommitLoadForLocalFrame(LocalFrame*);
  void DidStartProvisionalLoad(LocalFrame*);
  bool ScreencastEnabled();
  void LayerTreeViewChanged(WebLayerTreeView*);
  void RootLayerCleared();
  bool CacheDisabled() override;

  // WebDevToolsAgent implementation.
  void Attach(const WebString& host_id, int session_id) override;
  void Reattach(const WebString& host_id,
                int session_id,
                const WebString& saved_state) override;
  void Detach(int session_id) override;
  void ContinueProgram() override;
  void DispatchOnInspectorBackend(int session_id,
                                  int call_id,
                                  const WebString& method,
                                  const WebString& message) override;
  void InspectElementAt(int session_id, const WebPoint&) override;
  void FailedToRequestDevTools() override;
  WebString EvaluateInWebInspectorOverlay(const WebString& script) override;

 private:
  WebDevToolsAgentImpl(WebLocalFrameImpl*,
                       WebDevToolsAgentClient*,
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
  void WaitForCreateWindow(LocalFrame*) override;

  // InspectorLayerTreeAgent::Client implementation.
  bool IsInspectorLayer(GraphicsLayer*) override;

  // InspectorSession::Client implementation.
  void SendProtocolMessage(int session_id,
                           int call_id,
                           const String& response,
                           const String& state) override;

  // WebThread::TaskObserver implementation.
  void WillProcessTask() override;
  void DidProcessTask() override;

  InspectorSession* InitializeSession(int session_id,
                                      const String& host_id,
                                      String* state);
  void DestroySession(int session_id);
  void DispatchMessageFromFrontend(int session_id,
                                   const String& method,
                                   const String& message);

  friend class WebDevToolsAgent;
  static void RunDebuggerTask(
      int session_id,
      std::unique_ptr<WebDevToolsAgent::MessageDescriptor>);

  bool Attached() const { return !!sessions_.size(); }

  WebDevToolsAgentClient* client_;
  Member<WebLocalFrameImpl> web_local_frame_impl_;

  Member<CoreProbeSink> probe_sink_;
  Member<InspectorResourceContentLoader> resource_content_loader_;
  Member<InspectedFrames> inspected_frames_;
  Member<InspectorResourceContainer> resource_container_;
  Member<InspectorTraceEvents> trace_events_;

  HeapHashMap<int, Member<InspectorPageAgent>> page_agents_;
  HeapHashMap<int, Member<InspectorNetworkAgent>> network_agents_;
  HeapHashMap<int, Member<InspectorTracingAgent>> tracing_agents_;
  HeapHashMap<int, Member<InspectorOverlayAgent>> overlay_agents_;

  HeapHashMap<int, Member<InspectorSession>> sessions_;
  bool include_view_agents_;
  int layer_tree_id_;
};

}  // namespace blink

#endif

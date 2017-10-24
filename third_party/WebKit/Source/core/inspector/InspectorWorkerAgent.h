/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef InspectorWorkerAgent_h
#define InspectorWorkerAgent_h

#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Target.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashMap.h"

namespace blink {
class InspectedFrames;
class WorkerInspectorProxy;

class CORE_EXPORT InspectorWorkerAgent final
    : public InspectorBaseAgent<protocol::Target::Metainfo>,
      public WorkerInspectorProxy::PageInspector {
  WTF_MAKE_NONCOPYABLE(InspectorWorkerAgent);

 public:
  explicit InspectorWorkerAgent(InspectedFrames*);
  ~InspectorWorkerAgent() override;
  void Trace(blink::Visitor*) override;

  protocol::Response disable() override;
  void Restore() override;
  void DidCommitLoadForLocalFrame(LocalFrame*) override;

  // Probes
  void ShouldWaitForDebuggerOnWorkerStart(bool* result);
  void DidStartWorker(WorkerInspectorProxy*, bool waiting_for_debugger);
  void WorkerTerminated(WorkerInspectorProxy*);

  // Called from Dispatcher
  protocol::Response setAutoAttach(bool auto_attach,
                                   bool wait_for_debugger_on_start) override;
  protocol::Response setAttachToFrames(bool attach) override;
  protocol::Response sendMessageToTarget(
      const String& message,
      protocol::Maybe<String> session_id,
      protocol::Maybe<String> target_id) override;

  void SetTracingSessionId(const String&);

 private:
  bool AutoAttachEnabled();
  void ConnectToAllProxies();
  void DisconnectFromAllProxies(bool report_to_frontend);
  void ConnectToProxy(WorkerInspectorProxy*, bool waiting_for_debugger);
  protocol::DictionaryValue* AttachedSessionIds();

  // WorkerInspectorProxy::PageInspector implementation.
  void DispatchMessageFromWorker(WorkerInspectorProxy*,
                                 int connection,
                                 const String& message) override;

  Member<InspectedFrames> inspected_frames_;
  HeapHashMap<int, Member<WorkerInspectorProxy>> connected_proxies_;
  HashMap<int, String> connection_to_session_id_;
  HashMap<String, int> session_id_to_connection_;
  String tracing_session_id_;
  static int s_last_connection_;
};

}  // namespace blink

#endif  // !defined(InspectorWorkerAgent_h)

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_DEVTOOLS_SESSION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_DEVTOOLS_SESSION_H_

#include <memory>
#include "base/callback.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/inspector_session_state.h"
#include "third_party/blink/renderer/core/inspector/protocol/forward.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8-inspector-protocol.h"

namespace blink {

class DevToolsAgent;
class Document;
class DocumentLoader;
class InspectorAgent;
class LocalFrame;

class CORE_EXPORT DevToolsSession : public GarbageCollected<DevToolsSession>,
                                    public mojom::blink::DevToolsSession,
                                    public protocol::FrontendChannel,
                                    public v8_inspector::V8Inspector::Channel {
 public:
  DevToolsSession(
      DevToolsAgent*,
      mojo::PendingAssociatedRemote<mojom::blink::DevToolsSessionHost>
          host_remote,
      mojo::PendingAssociatedReceiver<mojom::blink::DevToolsSession>
          main_receiver,
      mojo::PendingReceiver<mojom::blink::DevToolsSession> io_receiver,
      mojom::blink::DevToolsSessionStatePtr reattach_session_state,
      bool client_expects_binary_responses,
      const String& session_id,
      scoped_refptr<base::SequencedTaskRunner> mojo_task_runner);
  DevToolsSession(const DevToolsSession&) = delete;
  DevToolsSession& operator=(const DevToolsSession&) = delete;
  ~DevToolsSession() override;

  void ConnectToV8(v8_inspector::V8Inspector*, int context_group_id);
  v8_inspector::V8InspectorSession* V8Session() { return v8_session_.get(); }

  void Append(InspectorAgent*);
  void Detach();
  void Trace(Visitor*) const;

  // protocol::FrontendChannel implementation.
  void FlushProtocolNotifications() override;

  // Core probes.
  void DidStartProvisionalLoad(LocalFrame*);
  void DidFailProvisionalLoad(LocalFrame*);
  void DidCommitLoad(LocalFrame*, DocumentLoader*);
  void PaintTiming(Document* document, const char* name, double timestamp);
  void DomContentLoadedEventFired(LocalFrame*);

 private:
  class IOSession;

  // mojom::blink::DevToolsSession implementation.
  void DispatchProtocolCommand(int call_id,
                               const String& method,
                               base::span<const uint8_t> message) override;
  void DispatchProtocolCommandImpl(int call_id,
                                   const String& method,
                                   base::span<const uint8_t> message);

  // protocol::FrontendChannel implementation.
  void SendProtocolResponse(
      int call_id,
      std::unique_ptr<protocol::Serializable> message) override;
  void SendProtocolNotification(
      std::unique_ptr<protocol::Serializable> message) override;
  void FallThrough(int call_id,
                   crdtp::span<uint8_t> method,
                   crdtp::span<uint8_t> message) override;

  // v8_inspector::V8Inspector::Channel implementation.
  void sendResponse(
      int call_id,
      std::unique_ptr<v8_inspector::StringBuffer> message) override;
  void sendNotification(
      std::unique_ptr<v8_inspector::StringBuffer> message) override;
  void flushProtocolNotifications() override;

  bool IsDetached();
  void SendProtocolResponse(int call_id, std::vector<uint8_t> message);

  // Converts to JSON if requested by the client.
  blink::mojom::blink::DevToolsMessagePtr FinalizeMessage(
      std::vector<uint8_t> message) const;

  Member<DevToolsAgent> agent_;
  // DevToolsSession is not tied to ExecutionContext
  HeapMojoAssociatedReceiver<mojom::blink::DevToolsSession, DevToolsSession>
      receiver_{this, nullptr};
  // DevToolsSession is not tied to ExecutionContext
  HeapMojoAssociatedRemote<mojom::blink::DevToolsSessionHost> host_remote_{
      nullptr};
  IOSession* io_session_;
  std::unique_ptr<v8_inspector::V8InspectorSession> v8_session_;
  std::unique_ptr<protocol::UberDispatcher> inspector_backend_dispatcher_;
  InspectorSessionState session_state_;
  HeapVector<Member<InspectorAgent>> agents_;
  // Notifications are lazily serialized to shift the serialization overhead
  // from performance measurements. We may want to revisit this.
  // See https://bugs.chromium.org/p/chromium/issues/detail?id=1044989#c8
  Vector<base::OnceCallback<std::vector<uint8_t>()>> notification_queue_;
  const bool client_expects_binary_responses_;
  InspectorAgentState v8_session_state_;
  InspectorAgentState::Bytes v8_session_state_cbor_;
  const String session_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_DEVTOOLS_SESSION_H_

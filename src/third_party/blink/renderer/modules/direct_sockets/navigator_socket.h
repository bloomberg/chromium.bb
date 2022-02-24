// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_NAVIGATOR_SOCKET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_NAVIGATOR_SOCKET_H_

#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/modules/direct_sockets/tcp_socket.h"
#include "third_party/blink/renderer/modules/direct_sockets/udp_socket.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExceptionState;
class LocalDOMWindow;
class Navigator;
class ScriptState;
class SocketOptions;
class TCPSocketOptions;
class UDPSocketOptions;

class MODULES_EXPORT NavigatorSocket final
    : public GarbageCollected<NavigatorSocket>,
      public Supplement<ExecutionContext>,
      public ExecutionContextLifecycleStateObserver {
 public:
  static const char kSupplementName[];

  enum class ProtocolType { kTcp, kUdp };

  explicit NavigatorSocket(ExecutionContext*);
  ~NavigatorSocket() override = default;

  NavigatorSocket(const NavigatorSocket&) = delete;
  NavigatorSocket& operator=(const NavigatorSocket&) = delete;

  // Gets, or creates, NavigatorSocket supplement on ExecutionContext.
  // See platform/Supplementable.h
  static NavigatorSocket& From(ScriptState*);

  // Navigator partial interface
  static ScriptPromise openTCPSocket(ScriptState*,
                                     Navigator&,
                                     const TCPSocketOptions*,
                                     ExceptionState&);

  static ScriptPromise openUDPSocket(ScriptState*,
                                     Navigator&,
                                     const UDPSocketOptions*,
                                     ExceptionState&);

  // ExecutionContextLifecycleStateObserver:
  void ContextDestroyed() override;
  void ContextLifecycleStateChanged(mojom::blink::FrameLifecycleState) override;

  void Trace(Visitor*) const override;

 private:
  // Binds service_remote_ if not already bound.
  void EnsureServiceConnected(LocalDOMWindow&);

  static mojom::blink::DirectSocketOptionsPtr CreateSocketOptions(
      const SocketOptions* options,
      NavigatorSocket::ProtocolType socket_type,
      ExceptionState& exception_state);

  ScriptPromise openTCPSocket(ScriptState*,
                              const TCPSocketOptions*,
                              ExceptionState&);

  ScriptPromise openUDPSocket(ScriptState*,
                              const UDPSocketOptions*,
                              ExceptionState&);

  // Updates exception state whenever returning false.
  bool OpenSocketPermitted(ScriptState*, const SocketOptions*, ExceptionState&);

  void OnTcpOpen(TCPSocket* socket,
                 int32_t result,
                 const absl::optional<net::IPEndPoint>& local_addr,
                 const absl::optional<net::IPEndPoint>& peer_addr,
                 mojo::ScopedDataPipeConsumerHandle receive_stream,
                 mojo::ScopedDataPipeProducerHandle send_stream);

  void OnUdpOpen(UDPSocket* socket,
                 int32_t result,
                 const absl::optional<net::IPEndPoint>& local_addr,
                 const absl::optional<net::IPEndPoint>& peer_addr);

  void OnConnectionError();

  HeapMojoRemote<blink::mojom::blink::DirectSocketsService> service_remote_{
      nullptr};

  HeapHashSet<Member<TCPSocket>> pending_tcp_;
  HeapHashSet<Member<UDPSocket>> pending_udp_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DIRECT_SOCKETS_NAVIGATOR_SOCKET_H_

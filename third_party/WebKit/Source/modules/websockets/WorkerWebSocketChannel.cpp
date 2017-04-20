/*
 * Copyright (C) 2011, 2012 Google Inc.  All rights reserved.
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

#include "modules/websockets/WorkerWebSocketChannel.h"

#include <memory>
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/loader/ThreadableLoadingContext.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerThread.h"
#include "modules/websockets/DocumentWebSocketChannel.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/heap/SafePoint.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/Platform.h"

namespace blink {

typedef WorkerWebSocketChannel::Bridge Bridge;
typedef WorkerWebSocketChannel::Peer Peer;

// Created and destroyed on the worker thread. All setters of this class are
// called on the main thread, while all getters are called on the worker
// thread. signalWorkerThread() must be called before any getters are called.
class WebSocketChannelSyncHelper {
 public:
  WebSocketChannelSyncHelper() {}
  ~WebSocketChannelSyncHelper() {}

  // All setters are called on the main thread.
  void SetConnectRequestResult(bool connect_request_result) {
    DCHECK(IsMainThread());
    connect_request_result_ = connect_request_result;
  }

  // All getters are called on the worker thread.
  bool ConnectRequestResult() const {
    DCHECK(!IsMainThread());
    return connect_request_result_;
  }

  // This should be called after all setters are called and before any
  // getters are called.
  void SignalWorkerThread() {
    DCHECK(IsMainThread());
    event_.Signal();
  }

  void Wait() {
    DCHECK(!IsMainThread());
    event_.Wait();
  }

 private:
  WaitableEvent event_;
  bool connect_request_result_ = false;
};

WorkerWebSocketChannel::WorkerWebSocketChannel(
    WorkerGlobalScope& worker_global_scope,
    WebSocketChannelClient* client,
    std::unique_ptr<SourceLocation> location)
    : bridge_(new Bridge(client, worker_global_scope)),
      location_at_connection_(std::move(location)) {}

WorkerWebSocketChannel::~WorkerWebSocketChannel() {
  DCHECK(!bridge_);
}

bool WorkerWebSocketChannel::Connect(const KURL& url, const String& protocol) {
  DCHECK(bridge_);
  return bridge_->Connect(location_at_connection_->Clone(), url, protocol);
}

void WorkerWebSocketChannel::Send(const CString& message) {
  DCHECK(bridge_);
  bridge_->Send(message);
}

void WorkerWebSocketChannel::Send(const DOMArrayBuffer& binary_data,
                                  unsigned byte_offset,
                                  unsigned byte_length) {
  DCHECK(bridge_);
  bridge_->Send(binary_data, byte_offset, byte_length);
}

void WorkerWebSocketChannel::Send(PassRefPtr<BlobDataHandle> blob_data) {
  DCHECK(bridge_);
  bridge_->Send(std::move(blob_data));
}

void WorkerWebSocketChannel::Close(int code, const String& reason) {
  DCHECK(bridge_);
  bridge_->Close(code, reason);
}

void WorkerWebSocketChannel::Fail(const String& reason,
                                  MessageLevel level,
                                  std::unique_ptr<SourceLocation> location) {
  if (!bridge_)
    return;

  std::unique_ptr<SourceLocation> captured_location = SourceLocation::Capture();
  if (!captured_location->IsUnknown()) {
    // If we are in JavaScript context, use the current location instead
    // of passed one - it's more precise.
    bridge_->Fail(reason, level, std::move(captured_location));
  } else if (location->IsUnknown()) {
    // No information is specified by the caller - use the url
    // and the line number at the connection.
    bridge_->Fail(reason, level, location_at_connection_->Clone());
  } else {
    // Use the specified information.
    bridge_->Fail(reason, level, std::move(location));
  }
}

void WorkerWebSocketChannel::Disconnect() {
  bridge_->Disconnect();
  bridge_.Clear();
}

DEFINE_TRACE(WorkerWebSocketChannel) {
  visitor->Trace(bridge_);
  WebSocketChannel::Trace(visitor);
}

Peer::Peer(Bridge* bridge,
           PassRefPtr<WorkerLoaderProxy> loader_proxy,
           WorkerThreadLifecycleContext* worker_thread_lifecycle_context)
    : WorkerThreadLifecycleObserver(worker_thread_lifecycle_context),
      bridge_(bridge),
      loader_proxy_(std::move(loader_proxy)),
      main_web_socket_channel_(nullptr) {
  DCHECK(IsMainThread());
}

Peer::~Peer() {
  DCHECK(IsMainThread());
}

bool Peer::Initialize(std::unique_ptr<SourceLocation> location,
                      ThreadableLoadingContext* loading_context) {
  DCHECK(IsMainThread());
  if (WasContextDestroyedBeforeObserverCreation())
    return false;
  main_web_socket_channel_ = DocumentWebSocketChannel::Create(
      loading_context, this, std::move(location));
  return true;
}

bool Peer::Connect(const KURL& url, const String& protocol) {
  DCHECK(IsMainThread());
  if (!main_web_socket_channel_)
    return false;
  return main_web_socket_channel_->Connect(url, protocol);
}

void Peer::SendTextAsCharVector(std::unique_ptr<Vector<char>> data) {
  DCHECK(IsMainThread());
  if (main_web_socket_channel_)
    main_web_socket_channel_->SendTextAsCharVector(std::move(data));
}

void Peer::SendBinaryAsCharVector(std::unique_ptr<Vector<char>> data) {
  DCHECK(IsMainThread());
  if (main_web_socket_channel_)
    main_web_socket_channel_->SendBinaryAsCharVector(std::move(data));
}

void Peer::SendBlob(PassRefPtr<BlobDataHandle> blob_data) {
  DCHECK(IsMainThread());
  if (main_web_socket_channel_)
    main_web_socket_channel_->Send(std::move(blob_data));
}

void Peer::Close(int code, const String& reason) {
  DCHECK(IsMainThread());
  if (!main_web_socket_channel_)
    return;
  main_web_socket_channel_->Close(code, reason);
}

void Peer::Fail(const String& reason,
                MessageLevel level,
                std::unique_ptr<SourceLocation> location) {
  DCHECK(IsMainThread());
  if (!main_web_socket_channel_)
    return;
  main_web_socket_channel_->Fail(reason, level, std::move(location));
}

void Peer::Disconnect() {
  DCHECK(IsMainThread());
  if (!main_web_socket_channel_)
    return;
  main_web_socket_channel_->Disconnect();
  main_web_socket_channel_ = nullptr;
}

static void WorkerGlobalScopeDidConnect(Bridge* bridge,
                                        const String& subprotocol,
                                        const String& extensions) {
  if (bridge && bridge->Client())
    bridge->Client()->DidConnect(subprotocol, extensions);
}

void Peer::DidConnect(const String& subprotocol, const String& extensions) {
  DCHECK(IsMainThread());
  loader_proxy_->PostTaskToWorkerGlobalScope(
      BLINK_FROM_HERE, CrossThreadBind(&WorkerGlobalScopeDidConnect, bridge_,
                                       subprotocol, extensions));
}

static void WorkerGlobalScopeDidReceiveTextMessage(Bridge* bridge,
                                                   const String& payload) {
  if (bridge && bridge->Client())
    bridge->Client()->DidReceiveTextMessage(payload);
}

void Peer::DidReceiveTextMessage(const String& payload) {
  DCHECK(IsMainThread());
  loader_proxy_->PostTaskToWorkerGlobalScope(
      BLINK_FROM_HERE, CrossThreadBind(&WorkerGlobalScopeDidReceiveTextMessage,
                                       bridge_, payload));
}

static void WorkerGlobalScopeDidReceiveBinaryMessage(
    Bridge* bridge,
    std::unique_ptr<Vector<char>> payload) {
  if (bridge && bridge->Client())
    bridge->Client()->DidReceiveBinaryMessage(std::move(payload));
}

void Peer::DidReceiveBinaryMessage(std::unique_ptr<Vector<char>> payload) {
  DCHECK(IsMainThread());
  loader_proxy_->PostTaskToWorkerGlobalScope(
      BLINK_FROM_HERE,
      CrossThreadBind(&WorkerGlobalScopeDidReceiveBinaryMessage, bridge_,
                      WTF::Passed(std::move(payload))));
}

static void WorkerGlobalScopeDidConsumeBufferedAmount(Bridge* bridge,
                                                      uint64_t consumed) {
  if (bridge && bridge->Client())
    bridge->Client()->DidConsumeBufferedAmount(consumed);
}

void Peer::DidConsumeBufferedAmount(uint64_t consumed) {
  DCHECK(IsMainThread());
  loader_proxy_->PostTaskToWorkerGlobalScope(
      BLINK_FROM_HERE,
      CrossThreadBind(&WorkerGlobalScopeDidConsumeBufferedAmount, bridge_,
                      consumed));
}

static void WorkerGlobalScopeDidStartClosingHandshake(Bridge* bridge) {
  if (bridge && bridge->Client())
    bridge->Client()->DidStartClosingHandshake();
}

void Peer::DidStartClosingHandshake() {
  DCHECK(IsMainThread());
  loader_proxy_->PostTaskToWorkerGlobalScope(
      BLINK_FROM_HERE,
      CrossThreadBind(&WorkerGlobalScopeDidStartClosingHandshake, bridge_));
}

static void WorkerGlobalScopeDidClose(
    Bridge* bridge,
    WebSocketChannelClient::ClosingHandshakeCompletionStatus
        closing_handshake_completion,
    unsigned short code,
    const String& reason) {
  if (bridge && bridge->Client())
    bridge->Client()->DidClose(closing_handshake_completion, code, reason);
}

void Peer::DidClose(
    ClosingHandshakeCompletionStatus closing_handshake_completion,
    unsigned short code,
    const String& reason) {
  DCHECK(IsMainThread());
  if (main_web_socket_channel_) {
    main_web_socket_channel_->Disconnect();
    main_web_socket_channel_ = nullptr;
  }
  loader_proxy_->PostTaskToWorkerGlobalScope(
      BLINK_FROM_HERE,
      CrossThreadBind(&WorkerGlobalScopeDidClose, bridge_,
                      closing_handshake_completion, code, reason));
}

static void WorkerGlobalScopeDidError(Bridge* bridge) {
  if (bridge && bridge->Client())
    bridge->Client()->DidError();
}

void Peer::DidError() {
  DCHECK(IsMainThread());
  loader_proxy_->PostTaskToWorkerGlobalScope(
      BLINK_FROM_HERE, CrossThreadBind(&WorkerGlobalScopeDidError, bridge_));
}

void Peer::ContextDestroyed(WorkerThreadLifecycleContext*) {
  DCHECK(IsMainThread());
  if (main_web_socket_channel_) {
    main_web_socket_channel_->Disconnect();
    main_web_socket_channel_ = nullptr;
  }
  bridge_ = nullptr;
}

DEFINE_TRACE(Peer) {
  visitor->Trace(main_web_socket_channel_);
  WebSocketChannelClient::Trace(visitor);
  WorkerThreadLifecycleObserver::Trace(visitor);
}

Bridge::Bridge(WebSocketChannelClient* client,
               WorkerGlobalScope& worker_global_scope)
    : client_(client),
      worker_global_scope_(worker_global_scope),
      loader_proxy_(worker_global_scope_->GetThread()->GetWorkerLoaderProxy()) {
}

Bridge::~Bridge() {
  DCHECK(!peer_);
}

void Bridge::ConnectOnMainThread(
    std::unique_ptr<SourceLocation> location,
    RefPtr<WorkerLoaderProxy> loader_proxy,
    WorkerThreadLifecycleContext* worker_thread_lifecycle_context,
    const KURL& url,
    const String& protocol,
    WebSocketChannelSyncHelper* sync_helper) {
  DCHECK(IsMainThread());
  DCHECK(!peer_);
  ThreadableLoadingContext* loading_context =
      loader_proxy->GetThreadableLoadingContext();
  if (!loading_context)
    return;
  Peer* peer = new Peer(this, loader_proxy_, worker_thread_lifecycle_context);
  if (peer->Initialize(std::move(location), loading_context)) {
    peer_ = peer;
    sync_helper->SetConnectRequestResult(peer_->Connect(url, protocol));
  }
  sync_helper->SignalWorkerThread();
}

bool Bridge::Connect(std::unique_ptr<SourceLocation> location,
                     const KURL& url,
                     const String& protocol) {
  // Wait for completion of the task on the main thread because the mixed
  // content check must synchronously be conducted.
  WebSocketChannelSyncHelper sync_helper;
  loader_proxy_->PostTaskToLoader(
      BLINK_FROM_HERE,
      CrossThreadBind(
          &Bridge::ConnectOnMainThread, WrapCrossThreadPersistent(this),
          WTF::Passed(location->Clone()), loader_proxy_,
          WrapCrossThreadPersistent(worker_global_scope_->GetThread()
                                        ->GetWorkerThreadLifecycleContext()),
          url, protocol, CrossThreadUnretained(&sync_helper)));
  sync_helper.Wait();
  return sync_helper.ConnectRequestResult();
}

void Bridge::Send(const CString& message) {
  DCHECK(peer_);
  std::unique_ptr<Vector<char>> data =
      WTF::WrapUnique(new Vector<char>(message.length()));
  if (message.length())
    memcpy(data->data(), static_cast<const char*>(message.data()),
           message.length());

  loader_proxy_->PostTaskToLoader(
      BLINK_FROM_HERE, CrossThreadBind(&Peer::SendTextAsCharVector, peer_,
                                       WTF::Passed(std::move(data))));
}

void Bridge::Send(const DOMArrayBuffer& binary_data,
                  unsigned byte_offset,
                  unsigned byte_length) {
  DCHECK(peer_);
  // ArrayBuffer isn't thread-safe, hence the content of ArrayBuffer is copied
  // into Vector<char>.
  std::unique_ptr<Vector<char>> data =
      WTF::MakeUnique<Vector<char>>(byte_length);
  if (binary_data.ByteLength())
    memcpy(data->data(),
           static_cast<const char*>(binary_data.Data()) + byte_offset,
           byte_length);

  loader_proxy_->PostTaskToLoader(
      BLINK_FROM_HERE, CrossThreadBind(&Peer::SendBinaryAsCharVector, peer_,
                                       WTF::Passed(std::move(data))));
}

void Bridge::Send(PassRefPtr<BlobDataHandle> data) {
  DCHECK(peer_);
  loader_proxy_->PostTaskToLoader(
      BLINK_FROM_HERE,
      CrossThreadBind(&Peer::SendBlob, peer_, std::move(data)));
}

void Bridge::Close(int code, const String& reason) {
  DCHECK(peer_);
  loader_proxy_->PostTaskToLoader(
      BLINK_FROM_HERE, CrossThreadBind(&Peer::Close, peer_, code, reason));
}

void Bridge::Fail(const String& reason,
                  MessageLevel level,
                  std::unique_ptr<SourceLocation> location) {
  DCHECK(peer_);
  loader_proxy_->PostTaskToLoader(
      BLINK_FROM_HERE, CrossThreadBind(&Peer::Fail, peer_, reason, level,
                                       WTF::Passed(location->Clone())));
}

void Bridge::Disconnect() {
  if (!peer_)
    return;

  loader_proxy_->PostTaskToLoader(BLINK_FROM_HERE,
                                  CrossThreadBind(&Peer::Disconnect, peer_));

  client_ = nullptr;
  peer_ = nullptr;
  worker_global_scope_.Clear();
}

DEFINE_TRACE(Bridge) {
  visitor->Trace(client_);
  visitor->Trace(worker_global_scope_);
}

}  // namespace blink

/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#ifndef WorkerWebSocketChannel_h
#define WorkerWebSocketChannel_h

#include <stdint.h>
#include <memory>
#include "bindings/core/v8/SourceLocation.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/WorkerThreadLifecycleObserver.h"
#include "modules/websockets/DocumentWebSocketChannel.h"
#include "modules/websockets/WebSocketChannel.h"
#include "modules/websockets/WebSocketChannelClient.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebTraceLocation.h"
#include "public/platform/modules/websockets/websocket.mojom-blink.h"

namespace blink {

class BlobDataHandle;
class KURL;
class ThreadableLoadingContext;
class WebSocketChannelSyncHelper;
class WorkerGlobalScope;
class WorkerThreadLifecycleContext;

class WorkerWebSocketChannel final : public WebSocketChannel {
  WTF_MAKE_NONCOPYABLE(WorkerWebSocketChannel);

 public:
  static WebSocketChannel* Create(WorkerGlobalScope& worker_global_scope,
                                  WebSocketChannelClient* client,
                                  std::unique_ptr<SourceLocation> location) {
    return new WorkerWebSocketChannel(worker_global_scope, client,
                                      std::move(location));
  }
  ~WorkerWebSocketChannel() override;

  // WebSocketChannel functions.
  bool Connect(const KURL&, const String& protocol) override;
  void Send(const CString&) override;
  void Send(const DOMArrayBuffer&,
            unsigned byte_offset,
            unsigned byte_length) override;
  void Send(RefPtr<BlobDataHandle>) override;
  void SendTextAsCharVector(std::unique_ptr<Vector<char>>) override {
    NOTREACHED();
  }
  void SendBinaryAsCharVector(std::unique_ptr<Vector<char>>) override {
    NOTREACHED();
  }
  void Close(int code, const String& reason) override;
  void Fail(const String& reason,
            MessageLevel,
            std::unique_ptr<SourceLocation>) override;
  void Disconnect() override;  // Will suppress didClose().

  virtual void Trace(blink::Visitor*);

  class Bridge;

  // A WebSocketChannelClient to pass to |main_channel_|. It forwards
  // method incovactions to the worker thread, and re-invokes them on the
  // WebSocketChannelClient given to the WorkerWebSocketChannel.
  //
  // Allocated and used in the main thread.
  class MainChannelClient final
      : public GarbageCollectedFinalized<MainChannelClient>,
        public WebSocketChannelClient,
        public WorkerThreadLifecycleObserver {
    USING_GARBAGE_COLLECTED_MIXIN(MainChannelClient);
    WTF_MAKE_NONCOPYABLE(MainChannelClient);

   public:
    MainChannelClient(Bridge*,
                      RefPtr<WebTaskRunner>,
                      WorkerThreadLifecycleContext*);
    ~MainChannelClient() override;

    // SourceLocation parameter may be shown when the connection fails.
    bool Initialize(std::unique_ptr<SourceLocation>, ThreadableLoadingContext*);

    bool Connect(const KURL&,
                 const String& protocol,
                 mojom::blink::WebSocketPtr);
    void SendTextAsCharVector(std::unique_ptr<Vector<char>>);
    void SendBinaryAsCharVector(std::unique_ptr<Vector<char>>);
    void SendBlob(RefPtr<BlobDataHandle>);
    void Close(int code, const String& reason);
    void Fail(const String& reason,
              MessageLevel,
              std::unique_ptr<SourceLocation>);
    void Disconnect();

    virtual void Trace(blink::Visitor*);
    // Promptly clear connection to bridge + loader proxy.
    EAGERLY_FINALIZE();

    // WebSocketChannelClient functions.
    void DidConnect(const String& subprotocol,
                    const String& extensions) override;
    void DidReceiveTextMessage(const String& payload) override;
    void DidReceiveBinaryMessage(std::unique_ptr<Vector<char>>) override;
    void DidConsumeBufferedAmount(uint64_t) override;
    void DidStartClosingHandshake() override;
    void DidClose(ClosingHandshakeCompletionStatus,
                  unsigned short code,
                  const String& reason) override;
    void DidError() override;

    // WorkerThreadLifecycleObserver function.
    void ContextDestroyed(WorkerThreadLifecycleContext*) override;

   private:
    void ReleaseMainChannel();

    CrossThreadWeakPersistent<Bridge> bridge_;
    RefPtr<WebTaskRunner> worker_networking_task_runner_;
    Member<DocumentWebSocketChannel> main_channel_;
  };

  // Bridge for MainChannelClient. Running on the worker thread.
  class Bridge final : public GarbageCollectedFinalized<Bridge> {
    WTF_MAKE_NONCOPYABLE(Bridge);

   public:
    Bridge(WebSocketChannelClient*, WorkerGlobalScope&);
    ~Bridge();

    // SourceLocation parameter may be shown when the connection fails.
    bool Connect(std::unique_ptr<SourceLocation>,
                 const KURL&,
                 const String& protocol);

    void Send(const CString& message);
    void Send(const DOMArrayBuffer&,
              unsigned byte_offset,
              unsigned byte_length);
    void Send(RefPtr<BlobDataHandle>);
    void Close(int code, const String& reason);
    void Fail(const String& reason,
              MessageLevel,
              std::unique_ptr<SourceLocation>);
    void Disconnect();

    void ConnectOnMainThread(std::unique_ptr<SourceLocation>,
                             ThreadableLoadingContext*,
                             RefPtr<WebTaskRunner>,
                             WorkerThreadLifecycleContext*,
                             const KURL&,
                             const String& protocol,
                             mojom::blink::WebSocketPtrInfo,
                             WebSocketChannelSyncHelper*);

    // Returns null when |disconnect| has already been called.
    WebSocketChannelClient* Client() { return client_; }

    void Trace(blink::Visitor*);
    // Promptly clear connection to peer + loader proxy.
    EAGERLY_FINALIZE();

   private:
    Member<WebSocketChannelClient> client_;
    Member<WorkerGlobalScope> worker_global_scope_;
    CrossThreadPersistent<ParentFrameTaskRunners> parent_frame_task_runners_;
    CrossThreadPersistent<MainChannelClient> main_channel_client_;
  };

 private:
  WorkerWebSocketChannel(WorkerGlobalScope&,
                         WebSocketChannelClient*,
                         std::unique_ptr<SourceLocation>);

  Member<Bridge> bridge_;
  std::unique_ptr<SourceLocation> location_at_connection_;
};

}  // namespace blink

#endif  // WorkerWebSocketChannel_h

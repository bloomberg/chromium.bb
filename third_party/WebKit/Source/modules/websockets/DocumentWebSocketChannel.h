/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef DocumentWebSocketChannel_h
#define DocumentWebSocketChannel_h

#include <stdint.h>
#include <memory>
#include "bindings/core/v8/SourceLocation.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/FileError.h"
#include "core/loader/ThreadableLoadingContext.h"
#include "modules/ModulesExport.h"
#include "modules/websockets/WebSocketChannel.h"
#include "modules/websockets/WebSocketHandle.h"
#include "modules/websockets/WebSocketHandleClient.h"
#include "platform/WebFrameScheduler.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Deque.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ThreadableLoadingContext;
class WebSocketHandshakeRequest;

// This class is a WebSocketChannel subclass that works with a Document in a
// DOMWindow (i.e. works in the main thread).
class MODULES_EXPORT DocumentWebSocketChannel final
    : public WebSocketChannel,
      public WebSocketHandleClient {
 public:
  // You can specify the source file and the line number information
  // explicitly by passing the last parameter.
  // In the usual case, they are set automatically and you don't have to
  // pass it.
  // Specify handle explicitly only in tests.
  static DocumentWebSocketChannel* Create(
      Document* document,
      WebSocketChannelClient* client,
      std::unique_ptr<SourceLocation> location,
      WebSocketHandle* handle = 0) {
    DCHECK(document);
    return Create(ThreadableLoadingContext::Create(*document), client,
                  std::move(location), handle);
  }
  static DocumentWebSocketChannel* Create(
      ThreadableLoadingContext* loading_context,
      WebSocketChannelClient* client,
      std::unique_ptr<SourceLocation> location,
      WebSocketHandle* handle = 0) {
    return new DocumentWebSocketChannel(loading_context, client,
                                        std::move(location), handle);
  }
  ~DocumentWebSocketChannel() override;

  // WebSocketChannel functions.
  bool Connect(const KURL&, const String& protocol) override;
  void Send(const CString& message) override;
  void Send(const DOMArrayBuffer&,
            unsigned byte_offset,
            unsigned byte_length) override;
  void Send(PassRefPtr<BlobDataHandle>) override;
  void SendTextAsCharVector(std::unique_ptr<Vector<char>> data) override;
  void SendBinaryAsCharVector(std::unique_ptr<Vector<char>> data) override;
  // Start closing handshake. Use the CloseEventCodeNotSpecified for the code
  // argument to omit payload.
  void Close(int code, const String& reason) override;
  void Fail(const String& reason,
            MessageLevel,
            std::unique_ptr<SourceLocation>) override;
  void Disconnect() override;

  DECLARE_VIRTUAL_TRACE();

 private:
  class BlobLoader;
  class Message;

  enum MessageType {
    kMessageTypeText,
    kMessageTypeBlob,
    kMessageTypeArrayBuffer,
    kMessageTypeTextAsCharVector,
    kMessageTypeBinaryAsCharVector,
    kMessageTypeClose,
  };

  struct ReceivedMessage {
    bool is_message_text;
    Vector<char> data;
  };

  DocumentWebSocketChannel(ThreadableLoadingContext*,
                           WebSocketChannelClient*,
                           std::unique_ptr<SourceLocation>,
                           WebSocketHandle*);
  void SendInternal(WebSocketHandle::MessageType,
                    const char* data,
                    size_t total_size,
                    uint64_t* consumed_buffered_amount);
  void ProcessSendQueue();
  void FlowControlIfNecessary();
  void FailAsError(const String& reason) {
    Fail(reason, kErrorMessageLevel, location_at_construction_->Clone());
  }
  void AbortAsyncOperations();
  void HandleDidClose(bool was_clean,
                      unsigned short code,
                      const String& reason);
  ThreadableLoadingContext* LoadingContext();

  // This may return nullptr.
  // TODO(kinuko): Remove dependency to document.
  Document* GetDocument();

  // WebSocketHandleClient functions.
  void DidConnect(WebSocketHandle*,
                  const String& selected_protocol,
                  const String& extensions) override;
  void DidStartOpeningHandshake(WebSocketHandle*,
                                PassRefPtr<WebSocketHandshakeRequest>) override;
  void DidFinishOpeningHandshake(WebSocketHandle*,
                                 const WebSocketHandshakeResponse*) override;
  void DidFail(WebSocketHandle*, const String& message) override;
  void DidReceiveData(WebSocketHandle*,
                      bool fin,
                      WebSocketHandle::MessageType,
                      const char* data,
                      size_t) override;
  void DidClose(WebSocketHandle*,
                bool was_clean,
                unsigned short code,
                const String& reason) override;
  void DidReceiveFlowControl(WebSocketHandle*, int64_t quota) override;
  void DidStartClosingHandshake(WebSocketHandle*) override;

  // Methods for BlobLoader.
  void DidFinishLoadingBlob(DOMArrayBuffer*);
  void DidFailLoadingBlob(FileError::ErrorCode);

  void TearDownFailedConnection();
  bool ShouldDisallowConnection(const KURL&);

  // m_handle is a handle of the connection.
  // m_handle == 0 means this channel is closed.
  std::unique_ptr<WebSocketHandle> handle_;

  // m_client can be deleted while this channel is alive, but this class
  // expects that disconnect() is called before the deletion.
  Member<WebSocketChannelClient> client_;
  KURL url_;
  // m_identifier > 0 means calling scriptContextExecution() returns a Document.
  unsigned long identifier_;
  Member<BlobLoader> blob_loader_;
  HeapDeque<Member<Message>> messages_;
  Vector<char> receiving_message_data_;
  Member<ThreadableLoadingContext> loading_context_;

  bool receiving_message_type_is_text_;
  uint64_t sending_quota_;
  uint64_t received_data_size_for_flow_control_;
  size_t sent_size_of_top_message_;
  std::unique_ptr<WebFrameScheduler::ActiveConnectionHandle>
      connection_handle_for_scheduler_;

  std::unique_ptr<SourceLocation> location_at_construction_;
  RefPtr<WebSocketHandshakeRequest> handshake_request_;

  static const uint64_t kReceivedDataSizeForFlowControlHighWaterMark = 1 << 15;
};

std::ostream& operator<<(std::ostream&, const DocumentWebSocketChannel*);

}  // namespace blink

#endif  // DocumentWebSocketChannel_h

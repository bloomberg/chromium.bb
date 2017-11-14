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

#include "modules/websockets/DocumentWebSocketChannel.h"

#include "core/dom/ExecutionContext.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/BaseFetchContext.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/MixedContentChecker.h"
#include "core/loader/SubresourceFilter.h"
#include "core/loader/ThreadableLoadingContext.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/probe/CoreProbes.h"
#include "core/typed_arrays/DOMArrayBuffer.h"
#include "modules/websockets/InspectorWebSocketEvents.h"
#include "modules/websockets/WebSocketChannelClient.h"
#include "modules/websockets/WebSocketFrame.h"
#include "modules/websockets/WebSocketHandleImpl.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "platform/WebFrameScheduler.h"
#include "platform/WebTaskRunner.h"
#include "platform/loader/fetch/UniqueIdentifier.h"
#include "platform/network/NetworkLog.h"
#include "platform/network/WebSocketHandshakeRequest.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/TaskType.h"
#include "public/platform/WebSocketHandshakeThrottle.h"
#include "public/platform/WebTraceLocation.h"
#include "public/platform/WebURL.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace blink {

class DocumentWebSocketChannel::BlobLoader final
    : public GarbageCollectedFinalized<DocumentWebSocketChannel::BlobLoader>,
      public FileReaderLoaderClient {
 public:
  BlobLoader(scoped_refptr<BlobDataHandle>, DocumentWebSocketChannel*);
  ~BlobLoader() override {}

  void Cancel();

  // FileReaderLoaderClient functions.
  void DidStartLoading() override {}
  void DidReceiveData() override {}
  void DidFinishLoading() override;
  void DidFail(FileError::ErrorCode) override;

  void Trace(blink::Visitor* visitor) { visitor->Trace(channel_); }

 private:
  Member<DocumentWebSocketChannel> channel_;
  std::unique_ptr<FileReaderLoader> loader_;
};

class DocumentWebSocketChannel::Message
    : public GarbageCollectedFinalized<DocumentWebSocketChannel::Message> {
 public:
  explicit Message(const CString&);
  explicit Message(scoped_refptr<BlobDataHandle>);
  explicit Message(DOMArrayBuffer*);
  // For WorkerWebSocketChannel
  explicit Message(std::unique_ptr<Vector<char>>, MessageType);
  // Close message
  Message(unsigned short code, const String& reason);

  void Trace(blink::Visitor* visitor) { visitor->Trace(array_buffer); }

  MessageType type;

  CString text;
  scoped_refptr<BlobDataHandle> blob_data_handle;
  Member<DOMArrayBuffer> array_buffer;
  std::unique_ptr<Vector<char>> vector_data;
  unsigned short code;
  String reason;
};

DocumentWebSocketChannel::BlobLoader::BlobLoader(
    scoped_refptr<BlobDataHandle> blob_data_handle,
    DocumentWebSocketChannel* channel)
    : channel_(channel),
      loader_(FileReaderLoader::Create(FileReaderLoader::kReadAsArrayBuffer,
                                       this)) {
  // TODO(kinuko): Remove dependency to document.
  loader_->Start(channel->GetDocument(), std::move(blob_data_handle));
}

void DocumentWebSocketChannel::BlobLoader::Cancel() {
  loader_->Cancel();
  // DidFail will be called immediately.
  // |this| is deleted here.
}

void DocumentWebSocketChannel::BlobLoader::DidFinishLoading() {
  channel_->DidFinishLoadingBlob(loader_->ArrayBufferResult());
  // |this| is deleted here.
}

void DocumentWebSocketChannel::BlobLoader::DidFail(
    FileError::ErrorCode error_code) {
  channel_->DidFailLoadingBlob(error_code);
  // |this| is deleted here.
}

struct DocumentWebSocketChannel::ConnectInfo {
  ConnectInfo(const String& selected_protocol, const String& extensions)
      : selected_protocol(selected_protocol), extensions(extensions) {}

  const String selected_protocol;
  const String extensions;
};

// static
DocumentWebSocketChannel* DocumentWebSocketChannel::CreateForTesting(
    Document* document,
    WebSocketChannelClient* client,
    std::unique_ptr<SourceLocation> location,
    WebSocketHandle* handle,
    std::unique_ptr<WebSocketHandshakeThrottle> handshake_throttle) {
  return new DocumentWebSocketChannel(
      ThreadableLoadingContext::Create(*document), client, std::move(location),
      WTF::WrapUnique(handle), std::move(handshake_throttle));
}

// static
DocumentWebSocketChannel* DocumentWebSocketChannel::Create(
    ThreadableLoadingContext* loading_context,
    WebSocketChannelClient* client,
    std::unique_ptr<SourceLocation> location) {
  return new DocumentWebSocketChannel(
      loading_context, client, std::move(location),
      std::make_unique<WebSocketHandleImpl>(),
      Platform::Current()->CreateWebSocketHandshakeThrottle());
}

DocumentWebSocketChannel::DocumentWebSocketChannel(
    ThreadableLoadingContext* loading_context,
    WebSocketChannelClient* client,
    std::unique_ptr<SourceLocation> location,
    std::unique_ptr<WebSocketHandle> handle,
    std::unique_ptr<WebSocketHandshakeThrottle> handshake_throttle)
    : handle_(std::move(handle)),
      client_(client),
      identifier_(CreateUniqueIdentifier()),
      loading_context_(loading_context),
      sending_quota_(0),
      received_data_size_for_flow_control_(
          kReceivedDataSizeForFlowControlHighWaterMark * 2),  // initial quota
      sent_size_of_top_message_(0),
      location_at_construction_(std::move(location)),
      handshake_throttle_(std::move(handshake_throttle)),
      throttle_passed_(false) {}

DocumentWebSocketChannel::~DocumentWebSocketChannel() {
  DCHECK(!blob_loader_);
}

bool DocumentWebSocketChannel::Connect(const KURL& url,
                                       const String& protocol,
                                       mojom::blink::WebSocketPtr socket_ptr) {
  NETWORK_DVLOG(1) << this << " Connect()";
  if (!handle_)
    return false;

  if (GetDocument()) {
    if (GetDocument()->GetFrame()) {
      if (MixedContentChecker::ShouldBlockWebSocket(GetDocument()->GetFrame(),
                                                    url))
        return false;
    }
    if (MixedContentChecker::IsMixedContent(GetDocument()->GetSecurityOrigin(),
                                            url)) {
      String message =
          "Connecting to a non-secure WebSocket server from a secure origin is "
          "deprecated.";
      GetDocument()->AddConsoleMessage(ConsoleMessage::Create(
          kJSMessageSource, kWarningMessageLevel, message));
    }

    if (GetDocument()->GetFrame()) {
      connection_handle_for_scheduler_ = GetDocument()
                                             ->GetFrame()
                                             ->FrameScheduler()
                                             ->OnActiveConnectionCreated();
    }
  }

  url_ = url;
  Vector<String> protocols;
  // Avoid placing an empty token in the Vector when the protocol string is
  // empty.
  if (!protocol.IsEmpty()) {
    // Since protocol is already verified and escaped, we can simply split
    // it.
    protocol.Split(", ", true, protocols);
  }

  // If the connection needs to be filtered, asynchronously fail. Synchronous
  // failure blocks the worker thread which should be avoided. Note that
  // returning "true" just indicates that this was not a mixed content error.
  if (ShouldDisallowConnection(url)) {
    GetDocument()
        ->GetTaskRunner(TaskType::kNetworking)
        ->PostTask(
            BLINK_FROM_HERE,
            WTF::Bind(&DocumentWebSocketChannel::TearDownFailedConnection,
                      WrapPersistent(this)));
    return true;
  }

  handle_->Initialize(std::move(socket_ptr));
  handle_->Connect(url, protocols,
                   loading_context_->GetFetchContext()->GetSecurityOrigin(),
                   loading_context_->GetFetchContext()->GetSiteForCookies(),
                   loading_context_->GetExecutionContext()->UserAgent(), this,
                   loading_context_->GetExecutionContext()
                       ->GetTaskRunner(TaskType::kNetworking)
                       .get());

  // TODO(ricea): Maybe lookup GetDocument()->GetFrame() less often?
  if (handshake_throttle_ && GetDocument() && GetDocument()->GetFrame() &&
      GetDocument()->GetFrame()->GetPage()) {
    // TODO(ricea): We may need to do something special here for SharedWorkers
    // and ServiceWorkers
    // TODO(ricea): Figure out who owns this WebFrame object and how long it can
    // be expected to live.
    LocalFrame* frame = GetDocument()->GetFrame();
    WebLocalFrame* web_frame = WebLocalFrameImpl::FromFrame(frame);
    handshake_throttle_->ThrottleHandshake(url, web_frame, this);
  } else {
    // Treat no throttle as success.
    throttle_passed_ = true;
  }

  FlowControlIfNecessary();
  TRACE_EVENT_INSTANT1("devtools.timeline", "WebSocketCreate",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorWebSocketCreateEvent::Data(
                           GetDocument(), identifier_, url, protocol));
  probe::didCreateWebSocket(GetDocument(), identifier_, url, protocol);
  return true;
}

bool DocumentWebSocketChannel::Connect(const KURL& url,
                                       const String& protocol) {
  mojom::blink::WebSocketPtr socket_ptr;
  auto socket_request = mojo::MakeRequest(&socket_ptr);
  if (GetDocument() && GetDocument()->GetFrame()) {
    GetDocument()->GetFrame()->GetInterfaceProvider().GetInterface(
        std::move(socket_request));
  }

  return Connect(url, protocol, std::move(socket_ptr));
}

void DocumentWebSocketChannel::Send(const CString& message) {
  NETWORK_DVLOG(1) << this << " Send(" << message << ") (CString argument)";
  // FIXME: Change the inspector API to show the entire message instead
  // of individual frames.
  probe::didSendWebSocketFrame(GetDocument(), identifier_,
                               WebSocketFrame::kOpCodeText, true,
                               message.data(), message.length());
  messages_.push_back(new Message(message));
  ProcessSendQueue();
}

void DocumentWebSocketChannel::Send(
    scoped_refptr<BlobDataHandle> blob_data_handle) {
  NETWORK_DVLOG(1) << this << " Send(" << blob_data_handle->Uuid() << ", "
                   << blob_data_handle->GetType() << ", "
                   << blob_data_handle->size() << ") "
                   << "(BlobDataHandle argument)";
  // FIXME: Change the inspector API to show the entire message instead
  // of individual frames.
  // FIXME: We can't access the data here.
  // Since Binary data are not displayed in Inspector, this does not
  // affect actual behavior.
  probe::didSendWebSocketFrame(GetDocument(), identifier_,
                               WebSocketFrame::kOpCodeBinary, true, "", 0);
  messages_.push_back(new Message(std::move(blob_data_handle)));
  ProcessSendQueue();
}

void DocumentWebSocketChannel::Send(const DOMArrayBuffer& buffer,
                                    unsigned byte_offset,
                                    unsigned byte_length) {
  NETWORK_DVLOG(1) << this << " Send(" << buffer.Data() << ", " << byte_offset
                   << ", " << byte_length << ") "
                   << "(DOMArrayBuffer argument)";
  // FIXME: Change the inspector API to show the entire message instead
  // of individual frames.
  probe::didSendWebSocketFrame(
      GetDocument(), identifier_, WebSocketFrame::kOpCodeBinary, true,
      static_cast<const char*>(buffer.Data()) + byte_offset, byte_length);
  // buffer.slice copies its contents.
  // FIXME: Reduce copy by sending the data immediately when we don't need to
  // queue the data.
  messages_.push_back(
      new Message(buffer.Slice(byte_offset, byte_offset + byte_length)));
  ProcessSendQueue();
}

void DocumentWebSocketChannel::SendTextAsCharVector(
    std::unique_ptr<Vector<char>> data) {
  NETWORK_DVLOG(1) << this << " SendTextAsCharVector("
                   << static_cast<void*>(data.get()) << ", " << data->size()
                   << ")";
  // FIXME: Change the inspector API to show the entire message instead
  // of individual frames.
  probe::didSendWebSocketFrame(GetDocument(), identifier_,
                               WebSocketFrame::kOpCodeText, true, data->data(),
                               data->size());
  messages_.push_back(
      new Message(std::move(data), kMessageTypeTextAsCharVector));
  ProcessSendQueue();
}

void DocumentWebSocketChannel::SendBinaryAsCharVector(
    std::unique_ptr<Vector<char>> data) {
  NETWORK_DVLOG(1) << this << " SendBinaryAsCharVector("
                   << static_cast<void*>(data.get()) << ", " << data->size()
                   << ")";
  // FIXME: Change the inspector API to show the entire message instead
  // of individual frames.
  probe::didSendWebSocketFrame(GetDocument(), identifier_,
                               WebSocketFrame::kOpCodeBinary, true,
                               data->data(), data->size());
  messages_.push_back(
      new Message(std::move(data), kMessageTypeBinaryAsCharVector));
  ProcessSendQueue();
}

void DocumentWebSocketChannel::Close(int code, const String& reason) {
  NETWORK_DVLOG(1) << this << " Close(" << code << ", " << reason << ")";
  DCHECK(handle_);
  unsigned short code_to_send = static_cast<unsigned short>(
      code == kCloseEventCodeNotSpecified ? kCloseEventCodeNoStatusRcvd : code);
  messages_.push_back(new Message(code_to_send, reason));
  ProcessSendQueue();
}

void DocumentWebSocketChannel::Fail(const String& reason,
                                    MessageLevel level,
                                    std::unique_ptr<SourceLocation> location) {
  NETWORK_DVLOG(1) << this << " Fail(" << reason << ")";
  if (GetDocument()) {
    probe::didReceiveWebSocketFrameError(GetDocument(), identifier_, reason);
    const String message = "WebSocket connection to '" + url_.ElidedString() +
                           "' failed: " + reason;
    GetDocument()->AddConsoleMessage(ConsoleMessage::Create(
        kJSMessageSource, level, message, std::move(location)));
  }
  // |reason| is only for logging and should not be provided for scripts,
  // hence close reason must be empty in tearDownFailedConnection.
  TearDownFailedConnection();
}

void DocumentWebSocketChannel::Disconnect() {
  NETWORK_DVLOG(1) << this << " disconnect()";
  if (identifier_) {
    TRACE_EVENT_INSTANT1(
        "devtools.timeline", "WebSocketDestroy", TRACE_EVENT_SCOPE_THREAD,
        "data", InspectorWebSocketEvent::Data(GetDocument(), identifier_));
    probe::didCloseWebSocket(GetDocument(), identifier_);
  }
  connection_handle_for_scheduler_.reset();
  AbortAsyncOperations();
  handshake_throttle_.reset();
  handle_.reset();
  client_ = nullptr;
  identifier_ = 0;
}

DocumentWebSocketChannel::Message::Message(const CString& text)
    : type(kMessageTypeText), text(text) {}

DocumentWebSocketChannel::Message::Message(
    scoped_refptr<BlobDataHandle> blob_data_handle)
    : type(kMessageTypeBlob), blob_data_handle(std::move(blob_data_handle)) {}

DocumentWebSocketChannel::Message::Message(DOMArrayBuffer* array_buffer)
    : type(kMessageTypeArrayBuffer), array_buffer(array_buffer) {}

DocumentWebSocketChannel::Message::Message(
    std::unique_ptr<Vector<char>> vector_data,
    MessageType type)
    : type(type), vector_data(std::move(vector_data)) {
  DCHECK(type == kMessageTypeTextAsCharVector ||
         type == kMessageTypeBinaryAsCharVector);
}

DocumentWebSocketChannel::Message::Message(unsigned short code,
                                           const String& reason)
    : type(kMessageTypeClose), code(code), reason(reason) {}

void DocumentWebSocketChannel::SendInternal(
    WebSocketHandle::MessageType message_type,
    const char* data,
    size_t total_size,
    uint64_t* consumed_buffered_amount) {
  WebSocketHandle::MessageType frame_type =
      sent_size_of_top_message_ ? WebSocketHandle::kMessageTypeContinuation
                                : message_type;
  DCHECK_GE(total_size, sent_size_of_top_message_);
  // The first cast is safe since the result of min() never exceeds
  // the range of size_t. The second cast is necessary to compile
  // min() on ILP32.
  size_t size = static_cast<size_t>(
      std::min(sending_quota_,
               static_cast<uint64_t>(total_size - sent_size_of_top_message_)));
  bool final = (sent_size_of_top_message_ + size == total_size);

  handle_->Send(final, frame_type, data + sent_size_of_top_message_, size);

  sent_size_of_top_message_ += size;
  sending_quota_ -= size;
  *consumed_buffered_amount += size;

  if (final) {
    messages_.pop_front();
    sent_size_of_top_message_ = 0;
  }
}

void DocumentWebSocketChannel::ProcessSendQueue() {
  DCHECK(handle_);
  uint64_t consumed_buffered_amount = 0;
  while (!messages_.IsEmpty() && !blob_loader_) {
    Message* message = messages_.front().Get();
    if (sending_quota_ == 0 && message->type != kMessageTypeClose)
      break;
    switch (message->type) {
      case kMessageTypeText:
        SendInternal(WebSocketHandle::kMessageTypeText, message->text.data(),
                     message->text.length(), &consumed_buffered_amount);
        break;
      case kMessageTypeBlob:
        DCHECK(!blob_loader_);
        blob_loader_ = new BlobLoader(message->blob_data_handle, this);
        break;
      case kMessageTypeArrayBuffer:
        SendInternal(WebSocketHandle::kMessageTypeBinary,
                     static_cast<const char*>(message->array_buffer->Data()),
                     message->array_buffer->ByteLength(),
                     &consumed_buffered_amount);
        break;
      case kMessageTypeTextAsCharVector:
        SendInternal(WebSocketHandle::kMessageTypeText,
                     message->vector_data->data(), message->vector_data->size(),
                     &consumed_buffered_amount);
        break;
      case kMessageTypeBinaryAsCharVector:
        SendInternal(WebSocketHandle::kMessageTypeBinary,
                     message->vector_data->data(), message->vector_data->size(),
                     &consumed_buffered_amount);
        break;
      case kMessageTypeClose: {
        // No message should be sent from now on.
        DCHECK_EQ(messages_.size(), 1u);
        DCHECK_EQ(sent_size_of_top_message_, 0u);
        handshake_throttle_.reset();
        handle_->Close(message->code, message->reason);
        messages_.pop_front();
        break;
      }
    }
  }
  if (client_ && consumed_buffered_amount > 0)
    client_->DidConsumeBufferedAmount(consumed_buffered_amount);
}

void DocumentWebSocketChannel::FlowControlIfNecessary() {
  if (!handle_ || received_data_size_for_flow_control_ <
                      kReceivedDataSizeForFlowControlHighWaterMark) {
    return;
  }
  handle_->FlowControl(received_data_size_for_flow_control_);
  received_data_size_for_flow_control_ = 0;
}

void DocumentWebSocketChannel::AbortAsyncOperations() {
  if (blob_loader_) {
    blob_loader_->Cancel();
    blob_loader_.Clear();
  }
}

void DocumentWebSocketChannel::HandleDidClose(bool was_clean,
                                              unsigned short code,
                                              const String& reason) {
  handshake_throttle_.reset();
  handle_.reset();
  AbortAsyncOperations();
  if (!client_) {
    return;
  }
  WebSocketChannelClient* client = client_;
  client_ = nullptr;
  WebSocketChannelClient::ClosingHandshakeCompletionStatus status =
      was_clean ? WebSocketChannelClient::kClosingHandshakeComplete
                : WebSocketChannelClient::kClosingHandshakeIncomplete;
  client->DidClose(status, code, reason);
  // client->DidClose may delete this object.
}

ThreadableLoadingContext* DocumentWebSocketChannel::LoadingContext() {
  return loading_context_;
}

Document* DocumentWebSocketChannel::GetDocument() {
  ExecutionContext* context = loading_context_->GetExecutionContext();
  if (context->IsDocument())
    return ToDocument(context);
  return nullptr;
}

void DocumentWebSocketChannel::DidConnect(WebSocketHandle* handle,
                                          const String& selected_protocol,
                                          const String& extensions) {
  NETWORK_DVLOG(1) << this << " DidConnect(" << handle << ", "
                   << String(selected_protocol) << ", " << String(extensions)
                   << ")";

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());
  DCHECK(client_);

  if (!throttle_passed_) {
    connect_info_ =
        std::make_unique<ConnectInfo>(selected_protocol, extensions);
    return;
  }

  handshake_throttle_.reset();

  client_->DidConnect(selected_protocol, extensions);
}

void DocumentWebSocketChannel::DidStartOpeningHandshake(
    WebSocketHandle* handle,
    scoped_refptr<WebSocketHandshakeRequest> request) {
  NETWORK_DVLOG(1) << this << " DidStartOpeningHandshake(" << handle << ")";

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());

  if (GetDocument()) {
    TRACE_EVENT_INSTANT1(
        "devtools.timeline", "WebSocketSendHandshakeRequest",
        TRACE_EVENT_SCOPE_THREAD, "data",
        InspectorWebSocketEvent::Data(GetDocument(), identifier_));
    probe::willSendWebSocketHandshakeRequest(GetDocument(), identifier_,
                                             request.get());
  }
  handshake_request_ = std::move(request);
}

void DocumentWebSocketChannel::DidFinishOpeningHandshake(
    WebSocketHandle* handle,
    const WebSocketHandshakeResponse* response) {
  NETWORK_DVLOG(1) << this << " DidFinishOpeningHandshake(" << handle << ")";

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());

  if (GetDocument()) {
    TRACE_EVENT_INSTANT1(
        "devtools.timeline", "WebSocketReceiveHandshakeResponse",
        TRACE_EVENT_SCOPE_THREAD, "data",
        InspectorWebSocketEvent::Data(GetDocument(), identifier_));
    probe::didReceiveWebSocketHandshakeResponse(
        GetDocument(), identifier_, handshake_request_.get(), response);
  }
  handshake_request_ = nullptr;
}

void DocumentWebSocketChannel::DidFail(WebSocketHandle* handle,
                                       const String& message) {
  NETWORK_DVLOG(1) << this << " DidFail(" << handle << ", " << String(message)
                   << ")";

  connection_handle_for_scheduler_.reset();

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());

  // This function is called when the browser is required to fail the
  // WebSocketConnection. Hence we fail this channel by calling
  // |this->failAsError| function.
  FailAsError(message);
  // |this| may be deleted.
}

void DocumentWebSocketChannel::DidReceiveData(WebSocketHandle* handle,
                                              bool fin,
                                              WebSocketHandle::MessageType type,
                                              const char* data,
                                              size_t size) {
  NETWORK_DVLOG(1) << this << " DidReceiveData(" << handle << ", " << fin
                   << ", " << type << ", (" << static_cast<const void*>(data)
                   << ", " << size << "))";

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());
  DCHECK(client_);
  // Non-final frames cannot be empty.
  DCHECK(fin || size);

  switch (type) {
    case WebSocketHandle::kMessageTypeText:
      DCHECK(receiving_message_data_.IsEmpty());
      receiving_message_type_is_text_ = true;
      break;
    case WebSocketHandle::kMessageTypeBinary:
      DCHECK(receiving_message_data_.IsEmpty());
      receiving_message_type_is_text_ = false;
      break;
    case WebSocketHandle::kMessageTypeContinuation:
      DCHECK(!receiving_message_data_.IsEmpty());
      break;
  }

  receiving_message_data_.Append(data, size);
  received_data_size_for_flow_control_ += size;
  FlowControlIfNecessary();
  if (!fin) {
    return;
  }
  // FIXME: Change the inspector API to show the entire message instead
  // of individual frames.
  WebSocketFrame::OpCode opcode = receiving_message_type_is_text_
                                      ? WebSocketFrame::kOpCodeText
                                      : WebSocketFrame::kOpCodeBinary;
  WebSocketFrame frame(opcode, receiving_message_data_.data(),
                       receiving_message_data_.size(), WebSocketFrame::kFinal);
  if (GetDocument()) {
    probe::didReceiveWebSocketFrame(GetDocument(), identifier_, frame.op_code,
                                    frame.masked, frame.payload,
                                    frame.payload_length);
  }
  if (receiving_message_type_is_text_) {
    String message = receiving_message_data_.IsEmpty()
                         ? g_empty_string
                         : String::FromUTF8(receiving_message_data_.data(),
                                            receiving_message_data_.size());
    receiving_message_data_.clear();
    if (message.IsNull()) {
      FailAsError("Could not decode a text frame as UTF-8.");
      // failAsError may delete this object.
    } else {
      client_->DidReceiveTextMessage(message);
    }
  } else {
    std::unique_ptr<Vector<char>> binary_data =
        WTF::WrapUnique(new Vector<char>);
    binary_data->swap(receiving_message_data_);
    client_->DidReceiveBinaryMessage(std::move(binary_data));
  }
}

void DocumentWebSocketChannel::DidClose(WebSocketHandle* handle,
                                        bool was_clean,
                                        unsigned short code,
                                        const String& reason) {
  NETWORK_DVLOG(1) << this << " DidClose(" << handle << ", " << was_clean
                   << ", " << code << ", " << String(reason) << ")";

  connection_handle_for_scheduler_.reset();

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());

  handle_.reset();

  if (identifier_ && GetDocument()) {
    TRACE_EVENT_INSTANT1(
        "devtools.timeline", "WebSocketDestroy", TRACE_EVENT_SCOPE_THREAD,
        "data", InspectorWebSocketEvent::Data(GetDocument(), identifier_));
    probe::didCloseWebSocket(GetDocument(), identifier_);
    identifier_ = 0;
  }

  HandleDidClose(was_clean, code, reason);
  // HandleDidClose may delete this object.
}

void DocumentWebSocketChannel::DidReceiveFlowControl(WebSocketHandle* handle,
                                                     int64_t quota) {
  NETWORK_DVLOG(1) << this << " DidReceiveFlowControl(" << handle << ", "
                   << quota << ")";

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());
  DCHECK_GE(quota, 0);

  sending_quota_ += quota;
  ProcessSendQueue();
}

void DocumentWebSocketChannel::DidStartClosingHandshake(
    WebSocketHandle* handle) {
  NETWORK_DVLOG(1) << this << " DidStartClosingHandshake(" << handle << ")";

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());

  if (client_)
    client_->DidStartClosingHandshake();
}

void DocumentWebSocketChannel::OnSuccess() {
  DCHECK(!throttle_passed_);
  DCHECK(handshake_throttle_);
  throttle_passed_ = true;
  handshake_throttle_ = nullptr;
  if (connect_info_) {
    client_->DidConnect(std::move(connect_info_->selected_protocol),
                        std::move(connect_info_->extensions));
    connect_info_.reset();
  }
}

void DocumentWebSocketChannel::OnError(const WebString& console_message) {
  DCHECK(!throttle_passed_);
  DCHECK(handshake_throttle_);
  handshake_throttle_ = nullptr;
  FailAsError(console_message);
}

void DocumentWebSocketChannel::DidFinishLoadingBlob(DOMArrayBuffer* buffer) {
  blob_loader_.Clear();
  DCHECK(handle_);
  // The loaded blob is always placed on m_messages[0].
  DCHECK_GT(messages_.size(), 0u);
  DCHECK_EQ(messages_.front()->type, kMessageTypeBlob);
  // We replace it with the loaded blob.
  messages_.front() = new Message(buffer);
  ProcessSendQueue();
}

void DocumentWebSocketChannel::DidFailLoadingBlob(
    FileError::ErrorCode error_code) {
  blob_loader_.Clear();
  if (error_code == FileError::kAbortErr) {
    // The error is caused by cancel().
    return;
  }
  // FIXME: Generate human-friendly reason message.
  FailAsError("Failed to load Blob: error code = " +
              String::Number(error_code));
  // |this| can be deleted here.
}

void DocumentWebSocketChannel::TearDownFailedConnection() {
  // m_handle and m_client can be null here.
  connection_handle_for_scheduler_.reset();
  handshake_throttle_.reset();

  if (client_)
    client_->DidError();

  HandleDidClose(false, kCloseEventCodeAbnormalClosure, String());
  // HandleDidClose may delete this object.
}

bool DocumentWebSocketChannel::ShouldDisallowConnection(const KURL& url) {
  DCHECK(handle_);
  DocumentLoader* loader = GetDocument()->Loader();
  if (!loader)
    return false;
  SubresourceFilter* subresource_filter = loader->GetSubresourceFilter();
  if (!subresource_filter)
    return false;
  return !subresource_filter->AllowWebSocketConnection(url);
}

void DocumentWebSocketChannel::Trace(blink::Visitor* visitor) {
  visitor->Trace(blob_loader_);
  visitor->Trace(messages_);
  visitor->Trace(client_);
  visitor->Trace(loading_context_);
  WebSocketChannel::Trace(visitor);
}

std::ostream& operator<<(std::ostream& ostream,
                         const DocumentWebSocketChannel* channel) {
  return ostream << "DocumentWebSocketChannel "
                 << static_cast<const void*>(channel);
}

}  // namespace blink

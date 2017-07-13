// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationConnection.h"

#include <memory>
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/Event.h"
#include "core/events/MessageEvent.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/frame/Deprecation.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "modules/EventTargetModules.h"
#include "modules/presentation/Presentation.h"
#include "modules/presentation/PresentationConnectionAvailableEvent.h"
#include "modules/presentation/PresentationConnectionCloseEvent.h"
#include "modules/presentation/PresentationController.h"
#include "modules/presentation/PresentationReceiver.h"
#include "modules/presentation/PresentationRequest.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

namespace {

const AtomicString& ConnectionStateToString(
    WebPresentationConnectionState state) {
  DEFINE_STATIC_LOCAL(const AtomicString, connecting_value, ("connecting"));
  DEFINE_STATIC_LOCAL(const AtomicString, connected_value, ("connected"));
  DEFINE_STATIC_LOCAL(const AtomicString, closed_value, ("closed"));
  DEFINE_STATIC_LOCAL(const AtomicString, terminated_value, ("terminated"));

  switch (state) {
    case WebPresentationConnectionState::kConnecting:
      return connecting_value;
    case WebPresentationConnectionState::kConnected:
      return connected_value;
    case WebPresentationConnectionState::kClosed:
      return closed_value;
    case WebPresentationConnectionState::kTerminated:
      return terminated_value;
  }

  NOTREACHED();
  return terminated_value;
}

const AtomicString& ConnectionCloseReasonToString(
    WebPresentationConnectionCloseReason reason) {
  DEFINE_STATIC_LOCAL(const AtomicString, error_value, ("error"));
  DEFINE_STATIC_LOCAL(const AtomicString, closed_value, ("closed"));
  DEFINE_STATIC_LOCAL(const AtomicString, went_away_value, ("wentaway"));

  switch (reason) {
    case WebPresentationConnectionCloseReason::kError:
      return error_value;
    case WebPresentationConnectionCloseReason::kClosed:
      return closed_value;
    case WebPresentationConnectionCloseReason::kWentAway:
      return went_away_value;
  }

  NOTREACHED();
  return error_value;
}

void ThrowPresentationDisconnectedError(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(kInvalidStateError,
                                    "Presentation connection is disconnected.");
}

}  // namespace

class PresentationConnection::Message final
    : public GarbageCollectedFinalized<PresentationConnection::Message> {
 public:
  Message(const String& text) : type(kMessageTypeText), text(text) {}

  Message(DOMArrayBuffer* array_buffer)
      : type(kMessageTypeArrayBuffer), array_buffer(array_buffer) {}

  Message(PassRefPtr<BlobDataHandle> blob_data_handle)
      : type(kMessageTypeBlob), blob_data_handle(std::move(blob_data_handle)) {}

  DEFINE_INLINE_TRACE() { visitor->Trace(array_buffer); }

  MessageType type;
  String text;
  Member<DOMArrayBuffer> array_buffer;
  RefPtr<BlobDataHandle> blob_data_handle;
};

class PresentationConnection::BlobLoader final
    : public GarbageCollectedFinalized<PresentationConnection::BlobLoader>,
      public FileReaderLoaderClient {
 public:
  BlobLoader(PassRefPtr<BlobDataHandle> blob_data_handle,
             PresentationConnection* presentation_connection)
      : presentation_connection_(presentation_connection),
        loader_(FileReaderLoader::Create(FileReaderLoader::kReadAsArrayBuffer,
                                         this)) {
    loader_->Start(presentation_connection_->GetExecutionContext(),
                   std::move(blob_data_handle));
  }
  ~BlobLoader() override {}

  // FileReaderLoaderClient functions.
  void DidStartLoading() override {}
  void DidReceiveData() override {}
  void DidFinishLoading() override {
    presentation_connection_->DidFinishLoadingBlob(
        loader_->ArrayBufferResult());
  }
  void DidFail(FileError::ErrorCode error_code) override {
    presentation_connection_->DidFailLoadingBlob(error_code);
  }

  void Cancel() { loader_->Cancel(); }

  DEFINE_INLINE_TRACE() { visitor->Trace(presentation_connection_); }

 private:
  Member<PresentationConnection> presentation_connection_;
  std::unique_ptr<FileReaderLoader> loader_;
};

PresentationConnection::PresentationConnection(LocalFrame* frame,
                                               const String& id,
                                               const KURL& url)
    : ContextClient(frame),
      id_(id),
      url_(url),
      state_(WebPresentationConnectionState::kConnecting),
      binary_type_(kBinaryTypeArrayBuffer),
      proxy_(nullptr) {}

PresentationConnection::~PresentationConnection() {
  DCHECK(!blob_loader_);
}

void PresentationConnection::BindProxy(
    std::unique_ptr<WebPresentationConnectionProxy> proxy) {
  DCHECK(proxy);
  proxy_ = std::move(proxy);
}

// static
PresentationConnection* PresentationConnection::Take(
    ScriptPromiseResolver* resolver,
    const WebPresentationInfo& presentation_info,
    PresentationRequest* request) {
  DCHECK(resolver);
  DCHECK(request);
  DCHECK(resolver->GetExecutionContext()->IsDocument());

  Document* document = ToDocument(resolver->GetExecutionContext());
  if (!document->GetFrame())
    return nullptr;

  PresentationController* controller =
      PresentationController::From(*document->GetFrame());
  if (!controller)
    return nullptr;

  return Take(controller, presentation_info, request);
}

// static
PresentationConnection* PresentationConnection::Take(
    PresentationController* controller,
    const WebPresentationInfo& presentation_info,
    PresentationRequest* request) {
  DCHECK(controller);
  DCHECK(request);

  PresentationConnection* connection = new PresentationConnection(
      controller->GetFrame(), presentation_info.id, presentation_info.url);
  controller->RegisterConnection(connection);

  // Fire onconnectionavailable event asynchronously.
  auto* event = PresentationConnectionAvailableEvent::Create(
      EventTypeNames::connectionavailable, connection);
  TaskRunnerHelper::Get(TaskType::kPresentation, request->GetExecutionContext())
      ->PostTask(BLINK_FROM_HERE,
                 WTF::Bind(&PresentationConnection::DispatchEventAsync,
                           WrapPersistent(request), WrapPersistent(event)));

  return connection;
}

// static
PresentationConnection* PresentationConnection::Take(
    PresentationReceiver* receiver,
    const WebPresentationInfo& presentation_info) {
  DCHECK(receiver);

  PresentationConnection* connection = new PresentationConnection(
      receiver->GetFrame(), presentation_info.id, presentation_info.url);
  receiver->RegisterConnection(connection);

  return connection;
}

const AtomicString& PresentationConnection::InterfaceName() const {
  return EventTargetNames::PresentationConnection;
}

ExecutionContext* PresentationConnection::GetExecutionContext() const {
  if (!GetFrame())
    return nullptr;
  return GetFrame()->GetDocument();
}

void PresentationConnection::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
  if (event_type == EventTypeNames::connect) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kPresentationConnectionConnectEventListener);
  } else if (event_type == EventTypeNames::close) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kPresentationConnectionCloseEventListener);
  } else if (event_type == EventTypeNames::terminate) {
    UseCounter::Count(
        GetExecutionContext(),
        WebFeature::kPresentationConnectionTerminateEventListener);
  } else if (event_type == EventTypeNames::message) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kPresentationConnectionMessageEventListener);
  }
}

DEFINE_TRACE(PresentationConnection) {
  visitor->Trace(blob_loader_);
  visitor->Trace(messages_);
  EventTargetWithInlineData::Trace(visitor);
  ContextClient::Trace(visitor);
}

const AtomicString& PresentationConnection::state() const {
  return ConnectionStateToString(state_);
}

void PresentationConnection::send(const String& message,
                                  ExceptionState& exception_state) {
  if (!CanSendMessage(exception_state))
    return;

  messages_.push_back(new Message(message));
  HandleMessageQueue();
}

void PresentationConnection::send(DOMArrayBuffer* array_buffer,
                                  ExceptionState& exception_state) {
  DCHECK(array_buffer);
  DCHECK(array_buffer->Buffer());
  if (!CanSendMessage(exception_state))
    return;

  messages_.push_back(new Message(array_buffer));
  HandleMessageQueue();
}

void PresentationConnection::send(
    NotShared<DOMArrayBufferView> array_buffer_view,
    ExceptionState& exception_state) {
  DCHECK(array_buffer_view);
  if (!CanSendMessage(exception_state))
    return;

  messages_.push_back(new Message(array_buffer_view.View()->buffer()));
  HandleMessageQueue();
}

void PresentationConnection::send(Blob* data, ExceptionState& exception_state) {
  DCHECK(data);
  if (!CanSendMessage(exception_state))
    return;

  messages_.push_back(new Message(data->GetBlobDataHandle()));
  HandleMessageQueue();
}

bool PresentationConnection::CanSendMessage(ExceptionState& exception_state) {
  if (state_ != WebPresentationConnectionState::kConnected) {
    ThrowPresentationDisconnectedError(exception_state);
    return false;
  }

  return !!proxy_;
}

void PresentationConnection::HandleMessageQueue() {
  if (!proxy_)
    return;

  while (!messages_.IsEmpty() && !blob_loader_) {
    Message* message = messages_.front().Get();
    switch (message->type) {
      case kMessageTypeText:
        proxy_->SendTextMessage(message->text);
        messages_.pop_front();
        break;
      case kMessageTypeArrayBuffer:
        proxy_->SendBinaryMessage(
            static_cast<const uint8_t*>(message->array_buffer->Data()),
            message->array_buffer->ByteLength());
        messages_.pop_front();
        break;
      case kMessageTypeBlob:
        DCHECK(!blob_loader_);
        blob_loader_ = new BlobLoader(message->blob_data_handle, this);
        break;
    }
  }
}

String PresentationConnection::binaryType() const {
  switch (binary_type_) {
    case kBinaryTypeBlob:
      return "blob";
    case kBinaryTypeArrayBuffer:
      return "arraybuffer";
  }
  NOTREACHED();
  return String();
}

void PresentationConnection::setBinaryType(const String& binary_type) {
  if (binary_type == "blob") {
    binary_type_ = kBinaryTypeBlob;
    return;
  }
  if (binary_type == "arraybuffer") {
    binary_type_ = kBinaryTypeArrayBuffer;
    return;
  }
  NOTREACHED();
}

void PresentationConnection::DidReceiveTextMessage(const WebString& message) {
  if (state_ != WebPresentationConnectionState::kConnected)
    return;

  DispatchEvent(MessageEvent::Create(message));
}

void PresentationConnection::DidReceiveBinaryMessage(const uint8_t* data,
                                                     size_t length) {
  if (state_ != WebPresentationConnectionState::kConnected)
    return;

  switch (binary_type_) {
    case kBinaryTypeBlob: {
      std::unique_ptr<BlobData> blob_data = BlobData::Create();
      blob_data->AppendBytes(data, length);
      Blob* blob =
          Blob::Create(BlobDataHandle::Create(std::move(blob_data), length));
      DispatchEvent(MessageEvent::Create(blob));
      return;
    }
    case kBinaryTypeArrayBuffer:
      DOMArrayBuffer* buffer = DOMArrayBuffer::Create(data, length);
      DispatchEvent(MessageEvent::Create(buffer));
      return;
  }
  NOTREACHED();
}

WebPresentationConnectionState PresentationConnection::GetState() {
  return state_;
}

void PresentationConnection::close() {
  if (state_ != WebPresentationConnectionState::kConnecting &&
      state_ != WebPresentationConnectionState::kConnected) {
    return;
  }
  WebPresentationClient* client =
      PresentationController::ClientFromContext(GetExecutionContext());
  if (client)
    client->CloseConnection(url_, id_, proxy_.get());

  TearDown();
}

void PresentationConnection::terminate() {
  if (state_ != WebPresentationConnectionState::kConnected)
    return;
  WebPresentationClient* client =
      PresentationController::ClientFromContext(GetExecutionContext());
  if (client)
    client->TerminatePresentation(url_, id_);

  TearDown();
}

bool PresentationConnection::Matches(
    const WebPresentationInfo& presentation_info) const {
  return url_ == KURL(presentation_info.url) &&
         id_ == String(presentation_info.id);
}

bool PresentationConnection::Matches(const String& id, const KURL& url) const {
  return url_ == url && id_ == id;
}

void PresentationConnection::DidChangeState(
    WebPresentationConnectionState state) {
  DidChangeState(state, true /* shouldDispatchEvent */);
}

void PresentationConnection::DidChangeState(
    WebPresentationConnectionState state,
    bool should_dispatch_event) {
  if (state_ == state)
    return;

  state_ = state;

  if (!should_dispatch_event)
    return;

  switch (state_) {
    case WebPresentationConnectionState::kConnecting:
      return;
    case WebPresentationConnectionState::kConnected:
      DispatchStateChangeEvent(Event::Create(EventTypeNames::connect));
      return;
    case WebPresentationConnectionState::kTerminated:
      DispatchStateChangeEvent(Event::Create(EventTypeNames::terminate));
      return;
    // Closed state is handled in |didClose()|.
    case WebPresentationConnectionState::kClosed:
      NOTREACHED();
      return;
  }
  NOTREACHED();
}

void PresentationConnection::NotifyTargetConnection(
    WebPresentationConnectionState state) {
  if (proxy_)
    proxy_->NotifyTargetConnection(state);
}

void PresentationConnection::DidClose(
    WebPresentationConnectionCloseReason reason,
    const String& message) {
  if (state_ == WebPresentationConnectionState::kClosed ||
      state_ == WebPresentationConnectionState::kTerminated) {
    return;
  }

  state_ = WebPresentationConnectionState::kClosed;
  DispatchStateChangeEvent(PresentationConnectionCloseEvent::Create(
      EventTypeNames::close, ConnectionCloseReasonToString(reason), message));
}

void PresentationConnection::DidClose() {
  DidClose(WebPresentationConnectionCloseReason::kClosed, "");
}

void PresentationConnection::DidFinishLoadingBlob(DOMArrayBuffer* buffer) {
  DCHECK(!messages_.IsEmpty());
  DCHECK_EQ(messages_.front()->type, kMessageTypeBlob);
  DCHECK(buffer);
  DCHECK(buffer->Buffer());
  // Send the loaded blob immediately here and continue processing the queue.
  if (proxy_) {
    proxy_->SendBinaryMessage(static_cast<const uint8_t*>(buffer->Data()),
                              buffer->ByteLength());
  }

  messages_.pop_front();
  blob_loader_.Clear();
  HandleMessageQueue();
}

void PresentationConnection::DidFailLoadingBlob(
    FileError::ErrorCode error_code) {
  DCHECK(!messages_.IsEmpty());
  DCHECK_EQ(messages_.front()->type, kMessageTypeBlob);
  // FIXME: generate error message?
  // Ignore the current failed blob item and continue with next items.
  messages_.pop_front();
  blob_loader_.Clear();
  HandleMessageQueue();
}

void PresentationConnection::DispatchStateChangeEvent(Event* event) {
  TaskRunnerHelper::Get(TaskType::kPresentation, GetExecutionContext())
      ->PostTask(BLINK_FROM_HERE,
                 WTF::Bind(&PresentationConnection::DispatchEventAsync,
                           WrapPersistent(this), WrapPersistent(event)));
}

// static
void PresentationConnection::DispatchEventAsync(EventTarget* target,
                                                Event* event) {
  DCHECK(target);
  DCHECK(event);
  target->DispatchEvent(event);
}

void PresentationConnection::TearDown() {
  // Cancel current Blob loading if any.
  if (blob_loader_) {
    blob_loader_->Cancel();
    blob_loader_.Clear();
  }
  messages_.clear();
}

}  // namespace blink

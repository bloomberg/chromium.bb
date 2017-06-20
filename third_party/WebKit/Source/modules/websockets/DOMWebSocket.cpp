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

#include "modules/websockets/DOMWebSocket.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/modules/v8/StringOrStringSequence.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/SecurityContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/events/MessageEvent.h"
#include "core/fileapi/Blob.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/websockets/CloseEvent.h"
#include "platform/Histogram.h"
#include "platform/blob/BlobData.h"
#include "platform/network/NetworkLog.h"
#include "platform/weborigin/KnownPorts.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/StringBuilder.h"
#include "public/platform/Platform.h"
#include "public/platform/WebInsecureRequestPolicy.h"

static const size_t kMaxByteSizeForHistogram = 100 * 1000 * 1000;
static const int32_t kBucketCountForMessageSizeHistogram = 50;

namespace blink {

DOMWebSocket::EventQueue::EventQueue(EventTarget* target)
    : state_(kActive),
      target_(target),
      resume_timer_(TaskRunnerHelper::Get(TaskType::kWebSocket,
                                          target->GetExecutionContext()),
                    this,
                    &EventQueue::ResumeTimerFired) {}

DOMWebSocket::EventQueue::~EventQueue() {
  ContextDestroyed();
}

void DOMWebSocket::EventQueue::Dispatch(Event* event) {
  switch (state_) {
    case kActive:
      DCHECK(events_.IsEmpty());
      DCHECK(target_->GetExecutionContext());
      target_->DispatchEvent(event);
      break;
    case kSuspended:
      events_.push_back(event);
      break;
    case kStopped:
      DCHECK(events_.IsEmpty());
      // Do nothing.
      break;
  }
}

bool DOMWebSocket::EventQueue::IsEmpty() const {
  return events_.IsEmpty();
}

void DOMWebSocket::EventQueue::Suspend() {
  resume_timer_.Stop();
  if (state_ != kActive)
    return;

  state_ = kSuspended;
}

void DOMWebSocket::EventQueue::Resume() {
  if (state_ != kSuspended || resume_timer_.IsActive())
    return;

  resume_timer_.StartOneShot(0, BLINK_FROM_HERE);
}

void DOMWebSocket::EventQueue::ContextDestroyed() {
  if (state_ == kStopped)
    return;

  state_ = kStopped;
  resume_timer_.Stop();
  events_.clear();
}

void DOMWebSocket::EventQueue::DispatchQueuedEvents() {
  if (state_ != kActive)
    return;

  HeapDeque<Member<Event>> events;
  events.Swap(events_);
  while (!events.IsEmpty()) {
    if (state_ == kStopped || state_ == kSuspended)
      break;
    DCHECK_EQ(state_, kActive);
    DCHECK(target_->GetExecutionContext());
    target_->DispatchEvent(events.TakeFirst());
    // |this| can be stopped here.
  }
  if (state_ == kSuspended) {
    while (!events_.IsEmpty())
      events.push_back(events_.TakeFirst());
    events.Swap(events_);
  }
}

void DOMWebSocket::EventQueue::ResumeTimerFired(TimerBase*) {
  DCHECK_EQ(state_, kSuspended);
  state_ = kActive;
  DispatchQueuedEvents();
}

DEFINE_TRACE(DOMWebSocket::EventQueue) {
  visitor->Trace(target_);
  visitor->Trace(events_);
}

const size_t kMaxReasonSizeInBytes = 123;

static inline bool IsValidSubprotocolCharacter(UChar character) {
  const UChar kMinimumProtocolCharacter = '!';  // U+0021.
  const UChar kMaximumProtocolCharacter = '~';  // U+007E.
  // Set to true if character does not matches "separators" ABNF defined in
  // RFC2616. SP and HT are excluded since the range check excludes them.
  bool is_not_separator =
      character != '"' && character != '(' && character != ')' &&
      character != ',' && character != '/' &&
      !(character >= ':' &&
        character <=
            '@')  // U+003A - U+0040 (':', ';', '<', '=', '>', '?', '@').
      && !(character >= '[' &&
           character <= ']')  // U+005B - U+005D ('[', '\\', ']').
      && character != '{' && character != '}';
  return character >= kMinimumProtocolCharacter &&
         character <= kMaximumProtocolCharacter && is_not_separator;
}

bool DOMWebSocket::IsValidSubprotocolString(const String& protocol) {
  if (protocol.IsEmpty())
    return false;
  for (size_t i = 0; i < protocol.length(); ++i) {
    if (!IsValidSubprotocolCharacter(protocol[i]))
      return false;
  }
  return true;
}

static String EncodeSubprotocolString(const String& protocol) {
  StringBuilder builder;
  for (size_t i = 0; i < protocol.length(); i++) {
    if (protocol[i] < 0x20 || protocol[i] > 0x7E)
      builder.Append(String::Format("\\u%04X", protocol[i]));
    else if (protocol[i] == 0x5c)
      builder.Append("\\\\");
    else
      builder.Append(protocol[i]);
  }
  return builder.ToString();
}

static String JoinStrings(const Vector<String>& strings,
                          const char* separator) {
  StringBuilder builder;
  for (size_t i = 0; i < strings.size(); ++i) {
    if (i)
      builder.Append(separator);
    builder.Append(strings[i]);
  }
  return builder.ToString();
}

static void SetInvalidStateErrorForSendMethod(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(kInvalidStateError,
                                    "Still in CONNECTING state.");
}

const char* DOMWebSocket::SubprotocolSeperator() {
  return ", ";
}

DOMWebSocket::DOMWebSocket(ExecutionContext* context)
    : SuspendableObject(context),
      state_(kConnecting),
      buffered_amount_(0),
      consumed_buffered_amount_(0),
      buffered_amount_after_close_(0),
      binary_type_(kBinaryTypeBlob),
      subprotocol_(""),
      extensions_(""),
      event_queue_(EventQueue::Create(this)),
      buffered_amount_consume_timer_(
          TaskRunnerHelper::Get(TaskType::kWebSocket, context),
          this,
          &DOMWebSocket::ReflectBufferedAmountConsumption) {}

DOMWebSocket::~DOMWebSocket() {
  DCHECK(!channel_);
}

void DOMWebSocket::LogError(const String& message) {
  if (GetExecutionContext())
    GetExecutionContext()->AddConsoleMessage(
        ConsoleMessage::Create(kJSMessageSource, kErrorMessageLevel, message));
}

DOMWebSocket* DOMWebSocket::Create(ExecutionContext* context,
                                   const String& url,
                                   ExceptionState& exception_state) {
  StringOrStringSequence protocols;
  return Create(context, url, protocols, exception_state);
}

DOMWebSocket* DOMWebSocket::Create(ExecutionContext* context,
                                   const String& url,
                                   const StringOrStringSequence& protocols,
                                   ExceptionState& exception_state) {
  if (url.IsNull()) {
    exception_state.ThrowDOMException(
        kSyntaxError,
        "Failed to create a WebSocket: the provided URL is invalid.");
    return nullptr;
  }

  DOMWebSocket* web_socket = new DOMWebSocket(context);
  web_socket->SuspendIfNeeded();

  if (protocols.isNull()) {
    Vector<String> protocols_vector;
    web_socket->Connect(url, protocols_vector, exception_state);
  } else if (protocols.isString()) {
    Vector<String> protocols_vector;
    protocols_vector.push_back(protocols.getAsString());
    web_socket->Connect(url, protocols_vector, exception_state);
  } else {
    DCHECK(protocols.isStringSequence());
    web_socket->Connect(url, protocols.getAsStringSequence(), exception_state);
  }

  if (exception_state.HadException())
    return nullptr;

  return web_socket;
}

void DOMWebSocket::Connect(const String& url,
                           const Vector<String>& protocols,
                           ExceptionState& exception_state) {
  UseCounter::Count(GetExecutionContext(), WebFeature::kWebSocket);

  NETWORK_DVLOG(1) << "WebSocket " << this << " connect() url=" << url;
  url_ = KURL(KURL(), url);

  if (GetExecutionContext()->GetSecurityContext().GetInsecureRequestPolicy() &
          kUpgradeInsecureRequests &&
      url_.Protocol() == "ws") {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kUpgradeInsecureRequestsUpgradedRequest);
    url_.SetProtocol("wss");
    if (url_.Port() == 80)
      url_.SetPort(443);
  }

  if (!url_.IsValid()) {
    state_ = kClosed;
    exception_state.ThrowDOMException(kSyntaxError,
                                      "The URL '" + url + "' is invalid.");
    return;
  }
  if (!url_.ProtocolIs("ws") && !url_.ProtocolIs("wss")) {
    state_ = kClosed;
    exception_state.ThrowDOMException(
        kSyntaxError, "The URL's scheme must be either 'ws' or 'wss'. '" +
                          url_.Protocol() + "' is not allowed.");
    return;
  }

  if (url_.HasFragmentIdentifier()) {
    state_ = kClosed;
    exception_state.ThrowDOMException(
        kSyntaxError,
        "The URL contains a fragment identifier ('" +
            url_.FragmentIdentifier() +
            "'). Fragment identifiers are not allowed in WebSocket URLs.");
    return;
  }

  if (!IsPortAllowedForScheme(url_)) {
    state_ = kClosed;
    exception_state.ThrowSecurityError(
        "The port " + String::Number(url_.Port()) + " is not allowed.");
    return;
  }

  if (!ContentSecurityPolicy::ShouldBypassMainWorld(GetExecutionContext()) &&
      !GetExecutionContext()->GetContentSecurityPolicy()->AllowConnectToSource(
          url_)) {
    state_ = kClosed;

    // Delay the event dispatch until after the current task by suspending and
    // resuming the queue. If we don't do this, the event is fired synchronously
    // with the constructor, meaning that it's impossible to listen for.
    event_queue_->Suspend();
    event_queue_->Dispatch(Event::Create(EventTypeNames::error));
    event_queue_->Resume();
    return;
  }

  // Fail if not all elements in |protocols| are valid.
  for (size_t i = 0; i < protocols.size(); ++i) {
    if (!IsValidSubprotocolString(protocols[i])) {
      state_ = kClosed;
      exception_state.ThrowDOMException(
          kSyntaxError, "The subprotocol '" +
                            EncodeSubprotocolString(protocols[i]) +
                            "' is invalid.");
      return;
    }
  }

  // Fail if there're duplicated elements in |protocols|.
  HashSet<String> visited;
  for (size_t i = 0; i < protocols.size(); ++i) {
    if (!visited.insert(protocols[i]).is_new_entry) {
      state_ = kClosed;
      exception_state.ThrowDOMException(
          kSyntaxError, "The subprotocol '" +
                            EncodeSubprotocolString(protocols[i]) +
                            "' is duplicated.");
      return;
    }
  }

  if (GetExecutionContext()->GetSecurityOrigin()->HasSuborigin()) {
    state_ = kClosed;
    exception_state.ThrowSecurityError(
        "Connecting to a WebSocket from a suborigin is not allowed.");
    return;
  }

  String protocol_string;
  if (!protocols.IsEmpty())
    protocol_string = JoinStrings(protocols, SubprotocolSeperator());

  origin_string_ = SecurityOrigin::Create(url_)->ToString();
  channel_ = CreateChannel(GetExecutionContext(), this);

  if (!channel_->Connect(url_, protocol_string)) {
    state_ = kClosed;
    exception_state.ThrowSecurityError(
        "An insecure WebSocket connection may not be initiated from a page "
        "loaded over HTTPS.");
    ReleaseChannel();
    return;
  }
}

void DOMWebSocket::UpdateBufferedAmountAfterClose(uint64_t payload_size) {
  buffered_amount_after_close_ += payload_size;

  LogError("WebSocket is already in CLOSING or CLOSED state.");
}

void DOMWebSocket::ReflectBufferedAmountConsumption(TimerBase*) {
  DCHECK_GE(buffered_amount_, consumed_buffered_amount_);
  // Cast to unsigned long long is required since clang doesn't accept
  // combination of %llu and uint64_t (known as unsigned long).
  NETWORK_DVLOG(1) << "WebSocket " << this
                   << " reflectBufferedAmountConsumption() " << buffered_amount_
                   << " => " << (buffered_amount_ - consumed_buffered_amount_);

  buffered_amount_ -= consumed_buffered_amount_;
  consumed_buffered_amount_ = 0;
}

void DOMWebSocket::ReleaseChannel() {
  DCHECK(channel_);
  channel_->Disconnect();
  channel_ = nullptr;
}

void DOMWebSocket::send(const String& message,
                        ExceptionState& exception_state) {
  CString encoded_message = message.Utf8();

  NETWORK_DVLOG(1) << "WebSocket " << this << " send() Sending String "
                   << message;
  if (state_ == kConnecting) {
    SetInvalidStateErrorForSendMethod(exception_state);
    return;
  }
  // No exception is raised if the connection was once established but has
  // subsequently been closed.
  if (state_ == kClosing || state_ == kClosed) {
    UpdateBufferedAmountAfterClose(encoded_message.length());
    return;
  }

  RecordSendTypeHistogram(kWebSocketSendTypeString);

  DCHECK(channel_);
  buffered_amount_ += encoded_message.length();
  channel_->Send(encoded_message);
}

void DOMWebSocket::send(DOMArrayBuffer* binary_data,
                        ExceptionState& exception_state) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " send() Sending ArrayBuffer "
                   << binary_data;
  DCHECK(binary_data);
  DCHECK(binary_data->Buffer());
  if (state_ == kConnecting) {
    SetInvalidStateErrorForSendMethod(exception_state);
    return;
  }
  if (state_ == kClosing || state_ == kClosed) {
    UpdateBufferedAmountAfterClose(binary_data->ByteLength());
    return;
  }
  RecordSendTypeHistogram(kWebSocketSendTypeArrayBuffer);
  RecordSendMessageSizeHistogram(kWebSocketSendTypeArrayBuffer,
                                 binary_data->ByteLength());
  DCHECK(channel_);
  buffered_amount_ += binary_data->ByteLength();
  channel_->Send(*binary_data, 0, binary_data->ByteLength());
}

void DOMWebSocket::send(NotShared<DOMArrayBufferView> array_buffer_view,
                        ExceptionState& exception_state) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " send() Sending ArrayBufferView "
                   << array_buffer_view.View();
  DCHECK(array_buffer_view);
  if (state_ == kConnecting) {
    SetInvalidStateErrorForSendMethod(exception_state);
    return;
  }
  if (state_ == kClosing || state_ == kClosed) {
    UpdateBufferedAmountAfterClose(array_buffer_view.View()->byteLength());
    return;
  }
  RecordSendTypeHistogram(kWebSocketSendTypeArrayBufferView);
  RecordSendMessageSizeHistogram(kWebSocketSendTypeArrayBufferView,
                                 array_buffer_view.View()->byteLength());
  DCHECK(channel_);
  buffered_amount_ += array_buffer_view.View()->byteLength();
  channel_->Send(*array_buffer_view.View()->buffer(),
                 array_buffer_view.View()->byteOffset(),
                 array_buffer_view.View()->byteLength());
}

void DOMWebSocket::send(Blob* binary_data, ExceptionState& exception_state) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " send() Sending Blob "
                   << binary_data->Uuid();
  DCHECK(binary_data);
  if (state_ == kConnecting) {
    SetInvalidStateErrorForSendMethod(exception_state);
    return;
  }
  if (state_ == kClosing || state_ == kClosed) {
    UpdateBufferedAmountAfterClose(binary_data->size());
    return;
  }
  unsigned long long size = binary_data->size();
  RecordSendTypeHistogram(kWebSocketSendTypeBlob);
  RecordSendMessageSizeHistogram(
      kWebSocketSendTypeBlob,
      clampTo<size_t>(size, 0, kMaxByteSizeForHistogram));
  buffered_amount_ += size;
  DCHECK(channel_);

  // When the runtime type of |binaryData| is File,
  // binaryData->blobDataHandle()->size() returns -1. However, in order to
  // maintain the value of |m_bufferedAmount| correctly, the WebSocket code
  // needs to fix the size of the File at this point. For this reason,
  // construct a new BlobDataHandle here with the size that this method
  // observed.
  channel_->Send(
      BlobDataHandle::Create(binary_data->Uuid(), binary_data->type(), size));
}

void DOMWebSocket::close(unsigned short code,
                         const String& reason,
                         ExceptionState& exception_state) {
  CloseInternal(code, reason, exception_state);
}

void DOMWebSocket::close(ExceptionState& exception_state) {
  CloseInternal(WebSocketChannel::kCloseEventCodeNotSpecified, String(),
                exception_state);
}

void DOMWebSocket::close(unsigned short code, ExceptionState& exception_state) {
  CloseInternal(code, String(), exception_state);
}

void DOMWebSocket::CloseInternal(int code,
                                 const String& reason,
                                 ExceptionState& exception_state) {
  String cleansed_reason = reason;
  if (code == WebSocketChannel::kCloseEventCodeNotSpecified) {
    NETWORK_DVLOG(1) << "WebSocket " << this
                     << " close() without code and reason";
  } else {
    NETWORK_DVLOG(1) << "WebSocket " << this << " close() code=" << code
                     << " reason=" << reason;
    if (!(code == WebSocketChannel::kCloseEventCodeNormalClosure ||
          (WebSocketChannel::kCloseEventCodeMinimumUserDefined <= code &&
           code <= WebSocketChannel::kCloseEventCodeMaximumUserDefined))) {
      exception_state.ThrowDOMException(
          kInvalidAccessError,
          "The code must be either 1000, or between 3000 and 4999. " +
              String::Number(code) + " is neither.");
      return;
    }
    // Bindings specify USVString, so unpaired surrogates are already replaced
    // with U+FFFD.
    CString utf8 = reason.Utf8();
    if (utf8.length() > kMaxReasonSizeInBytes) {
      exception_state.ThrowDOMException(
          kSyntaxError, "The message must not be greater than " +
                            String::Number(kMaxReasonSizeInBytes) + " bytes.");
      return;
    }
    if (!reason.IsEmpty() && !reason.Is8Bit()) {
      DCHECK_GT(utf8.length(), 0u);
      // reason might contain unpaired surrogates. Reconstruct it from
      // utf8.
      cleansed_reason = String::FromUTF8(utf8.data(), utf8.length());
    }
  }

  if (state_ == kClosing || state_ == kClosed)
    return;
  if (state_ == kConnecting) {
    state_ = kClosing;
    channel_->Fail("WebSocket is closed before the connection is established.",
                   kWarningMessageLevel,
                   SourceLocation::Create(String(), 0, 0, nullptr));
    return;
  }
  state_ = kClosing;
  if (channel_)
    channel_->Close(code, cleansed_reason);
}

const KURL& DOMWebSocket::url() const {
  return url_;
}

DOMWebSocket::State DOMWebSocket::readyState() const {
  return state_;
}

unsigned DOMWebSocket::bufferedAmount() const {
  uint64_t sum = buffered_amount_after_close_ + buffered_amount_;

  if (sum > std::numeric_limits<unsigned>::max())
    return std::numeric_limits<unsigned>::max();
  return sum;
}

String DOMWebSocket::protocol() const {
  return subprotocol_;
}

String DOMWebSocket::extensions() const {
  return extensions_;
}

String DOMWebSocket::binaryType() const {
  switch (binary_type_) {
    case kBinaryTypeBlob:
      return "blob";
    case kBinaryTypeArrayBuffer:
      return "arraybuffer";
  }
  NOTREACHED();
  return String();
}

void DOMWebSocket::setBinaryType(const String& binary_type) {
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

const AtomicString& DOMWebSocket::InterfaceName() const {
  return EventTargetNames::DOMWebSocket;
}

ExecutionContext* DOMWebSocket::GetExecutionContext() const {
  return SuspendableObject::GetExecutionContext();
}

void DOMWebSocket::ContextDestroyed(ExecutionContext*) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " contextDestroyed()";
  event_queue_->ContextDestroyed();
  if (channel_) {
    channel_->Close(WebSocketChannel::kCloseEventCodeGoingAway, String());
    ReleaseChannel();
  }
  if (state_ != kClosed)
    state_ = kClosed;
}

bool DOMWebSocket::HasPendingActivity() const {
  return channel_ || !event_queue_->IsEmpty();
}

void DOMWebSocket::Suspend() {
  event_queue_->Suspend();
}

void DOMWebSocket::Resume() {
  event_queue_->Resume();
}

void DOMWebSocket::DidConnect(const String& subprotocol,
                              const String& extensions) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " DidConnect()";
  if (state_ != kConnecting)
    return;
  state_ = kOpen;
  subprotocol_ = subprotocol;
  extensions_ = extensions;
  event_queue_->Dispatch(Event::Create(EventTypeNames::open));
}

void DOMWebSocket::DidReceiveTextMessage(const String& msg) {
  NETWORK_DVLOG(1) << "WebSocket " << this
                   << " DidReceiveTextMessage() Text message " << msg;

  if (state_ != kOpen)
    return;
  RecordReceiveTypeHistogram(kWebSocketReceiveTypeString);

  DCHECK(!origin_string_.IsNull());
  event_queue_->Dispatch(MessageEvent::Create(msg, origin_string_));
}

void DOMWebSocket::DidReceiveBinaryMessage(
    std::unique_ptr<Vector<char>> binary_data) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " DidReceiveBinaryMessage() "
                   << binary_data->size() << " byte binary message";

  DCHECK(!origin_string_.IsNull());

  switch (binary_type_) {
    case kBinaryTypeBlob: {
      size_t size = binary_data->size();
      RefPtr<RawData> raw_data = RawData::Create();
      binary_data->swap(*raw_data->MutableData());
      std::unique_ptr<BlobData> blob_data = BlobData::Create();
      blob_data->AppendData(std::move(raw_data), 0, BlobDataItem::kToEndOfFile);
      Blob* blob =
          Blob::Create(BlobDataHandle::Create(std::move(blob_data), size));
      RecordReceiveTypeHistogram(kWebSocketReceiveTypeBlob);
      RecordReceiveMessageSizeHistogram(kWebSocketReceiveTypeBlob, size);
      event_queue_->Dispatch(MessageEvent::Create(blob, origin_string_));
      break;
    }

    case kBinaryTypeArrayBuffer:
      DOMArrayBuffer* array_buffer =
          DOMArrayBuffer::Create(binary_data->data(), binary_data->size());
      RecordReceiveTypeHistogram(kWebSocketReceiveTypeArrayBuffer);
      RecordReceiveMessageSizeHistogram(kWebSocketReceiveTypeArrayBuffer,
                                        binary_data->size());
      event_queue_->Dispatch(
          MessageEvent::Create(array_buffer, origin_string_));
      break;
  }
}

void DOMWebSocket::DidError() {
  NETWORK_DVLOG(1) << "WebSocket " << this << " DidError()";
  state_ = kClosed;
  event_queue_->Dispatch(Event::Create(EventTypeNames::error));
}

void DOMWebSocket::DidConsumeBufferedAmount(uint64_t consumed) {
  DCHECK_GE(buffered_amount_, consumed + consumed_buffered_amount_);
  NETWORK_DVLOG(1) << "WebSocket " << this << " DidConsumeBufferedAmount("
                   << consumed << ")";
  if (state_ == kClosed)
    return;
  consumed_buffered_amount_ += consumed;
  if (!buffered_amount_consume_timer_.IsActive())
    buffered_amount_consume_timer_.StartOneShot(0, BLINK_FROM_HERE);
}

void DOMWebSocket::DidStartClosingHandshake() {
  NETWORK_DVLOG(1) << "WebSocket " << this << " DidStartClosingHandshake()";
  state_ = kClosing;
}

void DOMWebSocket::DidClose(
    ClosingHandshakeCompletionStatus closing_handshake_completion,
    unsigned short code,
    const String& reason) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " DidClose()";
  if (!channel_)
    return;
  bool all_data_has_been_consumed =
      buffered_amount_ == consumed_buffered_amount_;
  bool was_clean = state_ == kClosing && all_data_has_been_consumed &&
                   closing_handshake_completion == kClosingHandshakeComplete &&
                   code != WebSocketChannel::kCloseEventCodeAbnormalClosure;
  state_ = kClosed;

  event_queue_->Dispatch(CloseEvent::Create(was_clean, code, reason));
  ReleaseChannel();
}

void DOMWebSocket::RecordSendTypeHistogram(WebSocketSendType type) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, send_type_histogram,
      ("WebCore.WebSocket.SendType", kWebSocketSendTypeMax));
  send_type_histogram.Count(type);
}

void DOMWebSocket::RecordSendMessageSizeHistogram(WebSocketSendType type,
                                                  size_t size) {
  // Truncate |size| to avoid overflowing int32_t.
  int32_t size_to_count = clampTo<int32_t>(size, 0, kMaxByteSizeForHistogram);
  switch (type) {
    case kWebSocketSendTypeArrayBuffer: {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, array_buffer_message_size_histogram,
          ("WebCore.WebSocket.MessageSize.Send.ArrayBuffer", 1,
           kMaxByteSizeForHistogram, kBucketCountForMessageSizeHistogram));
      array_buffer_message_size_histogram.Count(size_to_count);
      return;
    }

    case kWebSocketSendTypeArrayBufferView: {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, array_buffer_view_message_size_histogram,
          ("WebCore.WebSocket.MessageSize.Send.ArrayBufferView", 1,
           kMaxByteSizeForHistogram, kBucketCountForMessageSizeHistogram));
      array_buffer_view_message_size_histogram.Count(size_to_count);
      return;
    }

    case kWebSocketSendTypeBlob: {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, blob_message_size_histogram,
          ("WebCore.WebSocket.MessageSize.Send.Blob", 1,
           kMaxByteSizeForHistogram, kBucketCountForMessageSizeHistogram));
      blob_message_size_histogram.Count(size_to_count);
      return;
    }

    default:
      NOTREACHED();
  }
}

void DOMWebSocket::RecordReceiveTypeHistogram(WebSocketReceiveType type) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, receive_type_histogram,
      ("WebCore.WebSocket.ReceiveType", kWebSocketReceiveTypeMax));
  receive_type_histogram.Count(type);
}

void DOMWebSocket::RecordReceiveMessageSizeHistogram(WebSocketReceiveType type,
                                                     size_t size) {
  // Truncate |size| to avoid overflowing int32_t.
  int32_t size_to_count = clampTo<int32_t>(size, 0, kMaxByteSizeForHistogram);
  switch (type) {
    case kWebSocketReceiveTypeArrayBuffer: {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, array_buffer_message_size_histogram,
          ("WebCore.WebSocket.MessageSize.Receive.ArrayBuffer", 1,
           kMaxByteSizeForHistogram, kBucketCountForMessageSizeHistogram));
      array_buffer_message_size_histogram.Count(size_to_count);
      return;
    }

    case kWebSocketReceiveTypeBlob: {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, blob_message_size_histogram,
          ("WebCore.WebSocket.MessageSize.Receive.Blob", 1,
           kMaxByteSizeForHistogram, kBucketCountForMessageSizeHistogram));
      blob_message_size_histogram.Count(size_to_count);
      return;
    }

    default:
      NOTREACHED();
  }
}

DEFINE_TRACE(DOMWebSocket) {
  visitor->Trace(channel_);
  visitor->Trace(event_queue_);
  WebSocketChannelClient::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
  SuspendableObject::Trace(visitor);
}

}  // namespace blink

/*
 * Copyright (C) 2009, 2012 Ericsson AB. All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#include "modules/eventsource/EventSource.h"

#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "bindings/core/v8/serialization/SerializedScriptValueFactory.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/events/Event.h"
#include "core/events/MessageEvent.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/ThreadableLoader.h"
#include "core/probe/CoreProbes.h"
#include "modules/eventsource/EventSourceInit.h"
#include "platform/HTTPNames.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/text/StringBuilder.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

const unsigned long long EventSource::kDefaultReconnectDelay = 3000;

inline EventSource::EventSource(ExecutionContext* context,
                                const KURL& url,
                                const EventSourceInit& event_source_init)
    : ContextLifecycleObserver(context),
      url_(url),
      current_url_(url),
      with_credentials_(event_source_init.withCredentials()),
      state_(kConnecting),
      connect_timer_(TaskRunnerHelper::Get(TaskType::kRemoteEvent, context),
                     this,
                     &EventSource::ConnectTimerFired),
      reconnect_delay_(kDefaultReconnectDelay) {}

EventSource* EventSource::Create(ExecutionContext* context,
                                 const String& url,
                                 const EventSourceInit& event_source_init,
                                 ExceptionState& exception_state) {
  if (context->IsDocument())
    UseCounter::Count(ToDocument(context), WebFeature::kEventSourceDocument);
  else
    UseCounter::Count(context, WebFeature::kEventSourceWorker);

  if (url.IsEmpty()) {
    exception_state.ThrowDOMException(
        kSyntaxError, "Cannot open an EventSource to an empty URL.");
    return nullptr;
  }

  KURL full_url = context->CompleteURL(url);
  if (!full_url.IsValid()) {
    exception_state.ThrowDOMException(
        kSyntaxError,
        "Cannot open an EventSource to '" + url + "'. The URL is invalid.");
    return nullptr;
  }

  EventSource* source = new EventSource(context, full_url, event_source_init);

  source->ScheduleInitialConnect();
  return source;
}

EventSource::~EventSource() {
  DCHECK_EQ(kClosed, state_);
  DCHECK(!loader_);
}

void EventSource::Dispose() {
  probe::detachClientRequest(GetExecutionContext(), this);
}

void EventSource::ScheduleInitialConnect() {
  DCHECK_EQ(kConnecting, state_);
  DCHECK(!loader_);

  connect_timer_.StartOneShot(0, BLINK_FROM_HERE);
}

void EventSource::Connect() {
  DCHECK_EQ(kConnecting, state_);
  DCHECK(!loader_);
  DCHECK(GetExecutionContext());

  ExecutionContext& execution_context = *this->GetExecutionContext();
  ResourceRequest request(current_url_);
  request.SetHTTPMethod(HTTPNames::GET);
  request.SetHTTPHeaderField(HTTPNames::Accept, "text/event-stream");
  request.SetHTTPHeaderField(HTTPNames::Cache_Control, "no-cache");
  request.SetRequestContext(WebURLRequest::kRequestContextEventSource);
  request.SetFetchRequestMode(WebURLRequest::kFetchRequestModeCORS);
  request.SetFetchCredentialsMode(
      with_credentials_ ? WebURLRequest::kFetchCredentialsModeInclude
                        : WebURLRequest::kFetchCredentialsModeSameOrigin);
  request.SetExternalRequestStateFromRequestorAddressSpace(
      execution_context.GetSecurityContext().AddressSpace());
  if (parser_ && !parser_->LastEventId().IsEmpty()) {
    // HTTP headers are Latin-1 byte strings, but the Last-Event-ID header is
    // encoded as UTF-8.
    // TODO(davidben): This should be captured in the type of
    // setHTTPHeaderField's arguments.
    CString last_event_id_utf8 = parser_->LastEventId().Utf8();
    request.SetHTTPHeaderField(
        HTTPNames::Last_Event_ID,
        AtomicString(reinterpret_cast<const LChar*>(last_event_id_utf8.data()),
                     last_event_id_utf8.length()));
  }

  SecurityOrigin* origin = execution_context.GetSecurityOrigin();

  ThreadableLoaderOptions options;
  options.preflight_policy = kPreventPreflight;

  ResourceLoaderOptions resource_loader_options;
  resource_loader_options.data_buffering_policy = kDoNotBufferData;
  resource_loader_options.security_origin = origin;

  probe::willSendEventSourceRequest(&execution_context, this);
  // probe::documentThreadableLoaderStartedLoadingForClient
  // will be called synchronously.
  loader_ = ThreadableLoader::Create(execution_context, this, options,
                                     resource_loader_options);
  loader_->Start(request);
}

void EventSource::NetworkRequestEnded() {
  probe::didFinishEventSourceRequest(GetExecutionContext(), this);

  loader_ = nullptr;

  if (state_ != kClosed)
    ScheduleReconnect();
}

void EventSource::ScheduleReconnect() {
  state_ = kConnecting;
  connect_timer_.StartOneShot(reconnect_delay_ / 1000.0, BLINK_FROM_HERE);
  DispatchEvent(Event::Create(EventTypeNames::error));
}

void EventSource::ConnectTimerFired(TimerBase*) {
  Connect();
}

String EventSource::url() const {
  return url_.GetString();
}

bool EventSource::withCredentials() const {
  return with_credentials_;
}

EventSource::State EventSource::readyState() const {
  return state_;
}

void EventSource::close() {
  if (state_ == kClosed) {
    DCHECK(!loader_);
    return;
  }
  if (parser_)
    parser_->Stop();

  // Stop trying to reconnect if EventSource was explicitly closed or if
  // contextDestroyed() was called.
  if (connect_timer_.IsActive()) {
    connect_timer_.Stop();
  }

  if (loader_) {
    loader_->Cancel();
    loader_ = nullptr;
  }

  state_ = kClosed;
}

const AtomicString& EventSource::InterfaceName() const {
  return EventTargetNames::EventSource;
}

ExecutionContext* EventSource::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

void EventSource::DidReceiveResponse(
    unsigned long,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(!handle);
  DCHECK_EQ(kConnecting, state_);
  DCHECK(loader_);

  current_url_ = response.Url();
  event_stream_origin_ = SecurityOrigin::Create(response.Url())->ToString();
  int status_code = response.HttpStatusCode();
  bool mime_type_is_valid = response.MimeType() == "text/event-stream";
  bool response_is_valid = status_code == 200 && mime_type_is_valid;
  if (response_is_valid) {
    const String& charset = response.TextEncodingName();
    // If we have a charset, the only allowed value is UTF-8 (case-insensitive).
    response_is_valid =
        charset.IsEmpty() || DeprecatedEqualIgnoringCase(charset, "UTF-8");
    if (!response_is_valid) {
      StringBuilder message;
      message.Append("EventSource's response has a charset (\"");
      message.Append(charset);
      message.Append("\") that is not UTF-8. Aborting the connection.");
      // FIXME: We are missing the source line.
      GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
          kJSMessageSource, kErrorMessageLevel, message.ToString()));
    }
  } else {
    // To keep the signal-to-noise ratio low, we only log 200-response with an
    // invalid MIME type.
    if (status_code == 200 && !mime_type_is_valid) {
      StringBuilder message;
      message.Append("EventSource's response has a MIME type (\"");
      message.Append(response.MimeType());
      message.Append(
          "\") that is not \"text/event-stream\". Aborting the connection.");
      // FIXME: We are missing the source line.
      GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
          kJSMessageSource, kErrorMessageLevel, message.ToString()));
    }
  }

  if (response_is_valid) {
    state_ = kOpen;
    AtomicString last_event_id;
    if (parser_) {
      // The new parser takes over the event ID.
      last_event_id = parser_->LastEventId();
    }
    parser_ = new EventSourceParser(last_event_id, this);
    DispatchEvent(Event::Create(EventTypeNames::open));
  } else {
    loader_->Cancel();
    DispatchEvent(Event::Create(EventTypeNames::error));
  }
}

void EventSource::DidReceiveData(const char* data, unsigned length) {
  DCHECK_EQ(kOpen, state_);
  DCHECK(loader_);
  DCHECK(parser_);

  parser_->AddBytes(data, length);
}

void EventSource::DidFinishLoading(unsigned long, double) {
  DCHECK_EQ(kOpen, state_);
  DCHECK(loader_);

  NetworkRequestEnded();
}

void EventSource::DidFail(const ResourceError& error) {
  DCHECK_NE(kClosed, state_);
  DCHECK(loader_);

  if (error.IsAccessCheck()) {
    AbortConnectionAttempt();
    return;
  }

  if (error.IsCancellation())
    state_ = kClosed;
  NetworkRequestEnded();
}

void EventSource::DidFailRedirectCheck() {
  DCHECK(loader_);

  AbortConnectionAttempt();
}

void EventSource::OnMessageEvent(const AtomicString& event_type,
                                 const String& data,
                                 const AtomicString& last_event_id) {
  MessageEvent* e = MessageEvent::Create();
  e->initMessageEvent(event_type, false, false, data, event_stream_origin_,
                      last_event_id, 0, nullptr);

  probe::willDispatchEventSourceEvent(GetExecutionContext(), this, event_type,
                                      last_event_id, data);
  DispatchEvent(e);
}

void EventSource::OnReconnectionTimeSet(unsigned long long reconnection_time) {
  reconnect_delay_ = reconnection_time;
}

void EventSource::AbortConnectionAttempt() {
  DCHECK_EQ(kConnecting, state_);

  loader_ = nullptr;
  state_ = kClosed;
  NetworkRequestEnded();

  DispatchEvent(Event::Create(EventTypeNames::error));
}

void EventSource::ContextDestroyed(ExecutionContext*) {
  probe::detachClientRequest(GetExecutionContext(), this);
  close();
}

bool EventSource::HasPendingActivity() const {
  return state_ != kClosed;
}

DEFINE_TRACE(EventSource) {
  visitor->Trace(parser_);
  visitor->Trace(loader_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  EventSourceParser::Client::Trace(visitor);
}

}  // namespace blink

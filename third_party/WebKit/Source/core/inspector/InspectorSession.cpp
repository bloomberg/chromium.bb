// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorSession.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/V8InspectorString.h"
#include "core/inspector/protocol/Protocol.h"
#include "core/probe/CoreProbes.h"

namespace blink {

namespace {
const char kV8StateKey[] = "v8";
}

InspectorSession::InspectorSession(Client* client,
                                   CoreProbeSink* instrumenting_agents,
                                   int session_id,
                                   v8_inspector::V8Inspector* inspector,
                                   int context_group_id,
                                   const String* saved_state)
    : client_(client),
      v8_session_(nullptr),
      session_id_(session_id),
      disposed_(false),
      instrumenting_agents_(instrumenting_agents),
      inspector_backend_dispatcher_(new protocol::UberDispatcher(this)) {
  if (saved_state) {
    std::unique_ptr<protocol::Value> state =
        protocol::StringUtil::parseJSON(*saved_state);
    if (state)
      state_ = protocol::DictionaryValue::cast(std::move(state));
    if (!state_)
      state_ = protocol::DictionaryValue::create();
  } else {
    state_ = protocol::DictionaryValue::create();
  }

  String v8_state;
  if (saved_state)
    state_->getString(kV8StateKey, &v8_state);
  v8_session_ = inspector->connect(context_group_id, this,
                                   ToV8InspectorStringView(v8_state));
}

InspectorSession::~InspectorSession() {
  DCHECK(disposed_);
}

void InspectorSession::Append(InspectorAgent* agent) {
  agents_.push_back(agent);
  agent->Init(instrumenting_agents_.Get(), inspector_backend_dispatcher_.get(),
              state_.get());
}

void InspectorSession::Restore() {
  DCHECK(!disposed_);
  for (size_t i = 0; i < agents_.size(); i++)
    agents_[i]->Restore();
}

void InspectorSession::Dispose() {
  DCHECK(!disposed_);
  disposed_ = true;
  inspector_backend_dispatcher_.reset();
  for (size_t i = agents_.size(); i > 0; i--)
    agents_[i - 1]->Dispose();
  agents_.clear();
  v8_session_.reset();
}

void InspectorSession::DispatchProtocolMessage(const String& method,
                                               const String& message) {
  DCHECK(!disposed_);
  if (v8_inspector::V8InspectorSession::canDispatchMethod(
          ToV8InspectorStringView(method))) {
    v8_session_->dispatchProtocolMessage(ToV8InspectorStringView(message));
  } else {
    inspector_backend_dispatcher_->dispatch(
        protocol::StringUtil::parseJSON(message));
  }
}

void InspectorSession::DidCommitLoadForLocalFrame(LocalFrame* frame) {
  for (size_t i = 0; i < agents_.size(); i++)
    agents_[i]->DidCommitLoadForLocalFrame(frame);
}

void InspectorSession::sendProtocolResponse(
    int call_id,
    std::unique_ptr<protocol::Serializable> message) {
  SendProtocolResponse(call_id, message->serialize());
}

void InspectorSession::sendResponse(
    int call_id,
    std::unique_ptr<v8_inspector::StringBuffer> message) {
  // We can potentially avoid copies if WebString would convert to utf8 right
  // from StringView, but it uses StringImpl itself, so we don't create any
  // extra copies here.
  SendProtocolResponse(call_id, ToCoreString(message->string()));
}

void InspectorSession::SendProtocolResponse(int call_id,
                                            const String& message) {
  if (disposed_)
    return;
  flushProtocolNotifications();
  state_->setString(kV8StateKey, ToCoreString(v8_session_->stateJSON()));
  String state_to_send = state_->serialize();
  if (state_to_send == last_sent_state_)
    state_to_send = String();
  else
    last_sent_state_ = state_to_send;
  client_->SendProtocolMessage(session_id_, call_id, message, state_to_send);
}

class InspectorSession::Notification {
 public:
  static std::unique_ptr<Notification> CreateForBlink(
      std::unique_ptr<protocol::Serializable> notification) {
    return std::unique_ptr<Notification>(
        new Notification(std::move(notification)));
  }

  static std::unique_ptr<Notification> CreateForV8(
      std::unique_ptr<v8_inspector::StringBuffer> notification) {
    return std::unique_ptr<Notification>(
        new Notification(std::move(notification)));
  }

  String Serialize() {
    if (blink_notification_) {
      serialized_ = blink_notification_->serialize();
      blink_notification_.reset();
    } else if (v8_notification_) {
      serialized_ = ToCoreString(v8_notification_->string());
      v8_notification_.reset();
    }
    return serialized_;
  }

 private:
  explicit Notification(std::unique_ptr<protocol::Serializable> notification)
      : blink_notification_(std::move(notification)) {}

  explicit Notification(
      std::unique_ptr<v8_inspector::StringBuffer> notification)
      : v8_notification_(std::move(notification)) {}

  std::unique_ptr<protocol::Serializable> blink_notification_;
  std::unique_ptr<v8_inspector::StringBuffer> v8_notification_;
  String serialized_;
};

void InspectorSession::sendProtocolNotification(
    std::unique_ptr<protocol::Serializable> notification) {
  if (disposed_)
    return;
  notification_queue_.push_back(
      Notification::CreateForBlink(std::move(notification)));
}

void InspectorSession::sendNotification(
    std::unique_ptr<v8_inspector::StringBuffer> notification) {
  if (disposed_)
    return;
  notification_queue_.push_back(
      Notification::CreateForV8(std::move(notification)));
}

void InspectorSession::flushProtocolNotifications() {
  if (disposed_)
    return;
  for (size_t i = 0; i < agents_.size(); i++)
    agents_[i]->FlushPendingProtocolNotifications();
  for (size_t i = 0; i < notification_queue_.size(); ++i) {
    client_->SendProtocolMessage(session_id_, 0,
                                 notification_queue_[i]->Serialize(), String());
  }
  notification_queue_.clear();
}

void InspectorSession::Trace(blink::Visitor* visitor) {
  visitor->Trace(instrumenting_agents_);
  visitor->Trace(agents_);
}

}  // namespace blink

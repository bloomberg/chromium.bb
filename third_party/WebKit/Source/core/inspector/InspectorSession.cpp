// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorSession.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/V8InspectorString.h"
#include "core/inspector/protocol/Protocol.h"

namespace blink {

namespace {
const char kV8StateKey[] = "v8";
}

InspectorSession::InspectorSession(Client* client,
                                   InstrumentingAgents* instrumentingAgents,
                                   int sessionId,
                                   v8_inspector::V8Inspector* inspector,
                                   int contextGroupId,
                                   const String* savedState)
    : m_client(client),
      m_v8Session(nullptr),
      m_sessionId(sessionId),
      m_disposed(false),
      m_instrumentingAgents(instrumentingAgents),
      m_inspectorBackendDispatcher(new protocol::UberDispatcher(this)) {
  if (savedState) {
    std::unique_ptr<protocol::Value> state =
        protocol::StringUtil::parseJSON(*savedState);
    if (state)
      m_state = protocol::DictionaryValue::cast(std::move(state));
    if (!m_state)
      m_state = protocol::DictionaryValue::create();
  } else {
    m_state = protocol::DictionaryValue::create();
  }

  String v8State;
  if (savedState)
    m_state->getString(kV8StateKey, &v8State);
  m_v8Session = inspector->connect(contextGroupId, this,
                                   toV8InspectorStringView(v8State));
}

InspectorSession::~InspectorSession() {
  DCHECK(m_disposed);
}

void InspectorSession::append(InspectorAgent* agent) {
  m_agents.append(agent);
  agent->init(m_instrumentingAgents.get(), m_inspectorBackendDispatcher.get(),
              m_state.get());
}

void InspectorSession::restore() {
  DCHECK(!m_disposed);
  for (size_t i = 0; i < m_agents.size(); i++)
    m_agents[i]->restore();
}

void InspectorSession::dispose() {
  DCHECK(!m_disposed);
  m_disposed = true;
  m_inspectorBackendDispatcher.reset();
  for (size_t i = m_agents.size(); i > 0; i--)
    m_agents[i - 1]->dispose();
  m_agents.clear();
  m_v8Session.reset();
}

void InspectorSession::dispatchProtocolMessage(const String& method,
                                               const String& message) {
  DCHECK(!m_disposed);
  if (v8_inspector::V8InspectorSession::canDispatchMethod(
          toV8InspectorStringView(method))) {
    m_v8Session->dispatchProtocolMessage(toV8InspectorStringView(message));
  } else {
    m_inspectorBackendDispatcher->dispatch(
        protocol::StringUtil::parseJSON(message));
  }
}

void InspectorSession::didCommitLoadForLocalFrame(LocalFrame* frame) {
  for (size_t i = 0; i < m_agents.size(); i++)
    m_agents[i]->didCommitLoadForLocalFrame(frame);
}

void InspectorSession::sendProtocolResponse(
    int callId,
    std::unique_ptr<protocol::Serializable> message) {
  sendProtocolResponse(callId, message->serialize());
}

void InspectorSession::sendResponse(
    int callId,
    std::unique_ptr<v8_inspector::StringBuffer> message) {
  // We can potentially avoid copies if WebString would convert to utf8 right
  // from StringView, but it uses StringImpl itself, so we don't create any
  // extra copies here.
  sendProtocolResponse(callId, toCoreString(message->string()));
}

void InspectorSession::sendProtocolResponse(int callId, const String& message) {
  if (m_disposed)
    return;
  flushProtocolNotifications();
  m_state->setString(kV8StateKey, toCoreString(m_v8Session->stateJSON()));
  String stateToSend = m_state->serialize();
  if (stateToSend == m_lastSentState)
    stateToSend = String();
  else
    m_lastSentState = stateToSend;
  m_client->sendProtocolMessage(m_sessionId, callId, message, stateToSend);
}

class InspectorSession::Notification {
 public:
  static std::unique_ptr<Notification> createForBlink(
      std::unique_ptr<protocol::Serializable> notification) {
    return std::unique_ptr<Notification>(
        new Notification(std::move(notification)));
  }

  static std::unique_ptr<Notification> createForV8(
      std::unique_ptr<v8_inspector::StringBuffer> notification) {
    return std::unique_ptr<Notification>(
        new Notification(std::move(notification)));
  }

  String serialize() {
    if (m_blinkNotification) {
      m_serialized = m_blinkNotification->serialize();
      m_blinkNotification.reset();
    } else if (m_v8Notification) {
      m_serialized = toCoreString(m_v8Notification->string());
      m_v8Notification.reset();
    }
    return m_serialized;
  }

 private:
  explicit Notification(std::unique_ptr<protocol::Serializable> notification)
      : m_blinkNotification(std::move(notification)) {}

  explicit Notification(
      std::unique_ptr<v8_inspector::StringBuffer> notification)
      : m_v8Notification(std::move(notification)) {}

  std::unique_ptr<protocol::Serializable> m_blinkNotification;
  std::unique_ptr<v8_inspector::StringBuffer> m_v8Notification;
  String m_serialized;
};

void InspectorSession::sendProtocolNotification(
    std::unique_ptr<protocol::Serializable> notification) {
  if (m_disposed)
    return;
  m_notificationQueue.append(
      Notification::createForBlink(std::move(notification)));
}

void InspectorSession::sendNotification(
    std::unique_ptr<v8_inspector::StringBuffer> notification) {
  if (m_disposed)
    return;
  m_notificationQueue.append(
      Notification::createForV8(std::move(notification)));
}

void InspectorSession::flushProtocolNotifications() {
  if (m_disposed)
    return;
  for (size_t i = 0; i < m_agents.size(); i++)
    m_agents[i]->flushPendingProtocolNotifications();
  for (size_t i = 0; i < m_notificationQueue.size(); ++i) {
    m_client->sendProtocolMessage(
        m_sessionId, 0, m_notificationQueue[i]->serialize(), String());
  }
  m_notificationQueue.clear();
}

DEFINE_TRACE(InspectorSession) {
  visitor->trace(m_instrumentingAgents);
  visitor->trace(m_agents);
}

}  // namespace blink

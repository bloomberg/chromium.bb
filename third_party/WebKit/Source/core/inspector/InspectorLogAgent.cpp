// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorLogAgent.h"

#include "bindings/core/v8/SourceLocation.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/IdentifiersFactory.h"

namespace blink {

namespace LogAgentState {
static const char logEnabled[] = "logEnabled";
}

namespace {

String messageSourceValue(MessageSource source) {
  DCHECK(source != ConsoleAPIMessageSource);
  switch (source) {
    case XMLMessageSource:
      return protocol::Log::LogEntry::SourceEnum::Xml;
    case JSMessageSource:
      return protocol::Log::LogEntry::SourceEnum::Javascript;
    case NetworkMessageSource:
      return protocol::Log::LogEntry::SourceEnum::Network;
    case StorageMessageSource:
      return protocol::Log::LogEntry::SourceEnum::Storage;
    case AppCacheMessageSource:
      return protocol::Log::LogEntry::SourceEnum::Appcache;
    case RenderingMessageSource:
      return protocol::Log::LogEntry::SourceEnum::Rendering;
    case SecurityMessageSource:
      return protocol::Log::LogEntry::SourceEnum::Security;
    case OtherMessageSource:
      return protocol::Log::LogEntry::SourceEnum::Other;
    case DeprecationMessageSource:
      return protocol::Log::LogEntry::SourceEnum::Deprecation;
    case WorkerMessageSource:
      return protocol::Log::LogEntry::SourceEnum::Worker;
    default:
      return protocol::Log::LogEntry::SourceEnum::Other;
  }
}

String messageLevelValue(MessageLevel level) {
  switch (level) {
    case DebugMessageLevel:
      return protocol::Log::LogEntry::LevelEnum::Debug;
    case LogMessageLevel:
      return protocol::Log::LogEntry::LevelEnum::Log;
    case WarningMessageLevel:
      return protocol::Log::LogEntry::LevelEnum::Warning;
    case ErrorMessageLevel:
      return protocol::Log::LogEntry::LevelEnum::Error;
    case InfoMessageLevel:
      return protocol::Log::LogEntry::LevelEnum::Info;
  }
  return protocol::Log::LogEntry::LevelEnum::Log;
}

}  // namespace

InspectorLogAgent::InspectorLogAgent(ConsoleMessageStorage* storage)
    : m_enabled(false), m_storage(storage) {}

InspectorLogAgent::~InspectorLogAgent() {}

DEFINE_TRACE(InspectorLogAgent) {
  visitor->trace(m_storage);
  InspectorBaseAgent::trace(visitor);
}

void InspectorLogAgent::restore() {
  if (!m_state->booleanProperty(LogAgentState::logEnabled, false))
    return;
  enable();
}

void InspectorLogAgent::consoleMessageAdded(ConsoleMessage* message) {
  DCHECK(m_enabled);

  std::unique_ptr<protocol::Log::LogEntry> entry =
      protocol::Log::LogEntry::create()
          .setSource(messageSourceValue(message->source()))
          .setLevel(messageLevelValue(message->level()))
          .setText(message->message())
          .setTimestamp(message->timestamp())
          .build();
  if (!message->location()->url().isEmpty())
    entry->setUrl(message->location()->url());
  std::unique_ptr<v8_inspector::protocol::Runtime::API::StackTrace> stackTrace =
      message->location()->buildInspectorObject();
  if (stackTrace)
    entry->setStackTrace(std::move(stackTrace));
  if (message->location()->lineNumber())
    entry->setLineNumber(message->location()->lineNumber() - 1);
  if (message->source() == WorkerMessageSource &&
      !message->workerId().isEmpty())
    entry->setWorkerId(message->workerId());
  if (message->source() == NetworkMessageSource && message->requestIdentifier())
    entry->setNetworkRequestId(
        IdentifiersFactory::requestId(message->requestIdentifier()));

  frontend()->entryAdded(std::move(entry));
  frontend()->flush();
}

Response InspectorLogAgent::enable() {
  if (m_enabled)
    return Response::OK();
  m_instrumentingAgents->addInspectorLogAgent(this);
  m_state->setBoolean(LogAgentState::logEnabled, true);
  m_enabled = true;

  if (m_storage->expiredCount()) {
    std::unique_ptr<protocol::Log::LogEntry> expired =
        protocol::Log::LogEntry::create()
            .setSource(protocol::Log::LogEntry::SourceEnum::Other)
            .setLevel(protocol::Log::LogEntry::LevelEnum::Warning)
            .setText(String::number(m_storage->expiredCount()) +
                     String(" log entires are not shown."))
            .setTimestamp(0)
            .build();
    frontend()->entryAdded(std::move(expired));
    frontend()->flush();
  }
  for (size_t i = 0; i < m_storage->size(); ++i)
    consoleMessageAdded(m_storage->at(i));
  return Response::OK();
}

Response InspectorLogAgent::disable() {
  if (!m_enabled)
    return Response::OK();
  m_state->setBoolean(LogAgentState::logEnabled, false);
  m_enabled = false;
  m_instrumentingAgents->removeInspectorLogAgent(this);
  return Response::OK();
}

Response InspectorLogAgent::clear() {
  m_storage->clear();
  return Response::OK();
}

}  // namespace blink

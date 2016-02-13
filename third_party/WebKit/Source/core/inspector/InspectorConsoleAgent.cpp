/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "core/inspector/InspectorConsoleAgent.h"

#include "bindings/core/v8/ScriptCallStack.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/ScriptArguments.h"
#include "platform/v8_inspector/public/V8Debugger.h"
#include "platform/v8_inspector/public/V8DebuggerAgent.h"
#include "platform/v8_inspector/public/V8RuntimeAgent.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace ConsoleAgentState {
static const char consoleMessagesEnabled[] = "consoleMessagesEnabled";
}

InspectorConsoleAgent::InspectorConsoleAgent(V8RuntimeAgent* runtimeAgent)
    : InspectorBaseAgent<InspectorConsoleAgent, protocol::Frontend::Console>("Console")
    , m_runtimeAgent(runtimeAgent)
    , m_debuggerAgent(nullptr)
    , m_enabled(false)
{
}

InspectorConsoleAgent::~InspectorConsoleAgent()
{
#if !ENABLE(OILPAN)
    m_instrumentingAgents->setInspectorConsoleAgent(0);
#endif
}

void InspectorConsoleAgent::enable(ErrorString*)
{
    if (m_enabled)
        return;
    m_instrumentingAgents->setInspectorConsoleAgent(this);
    m_enabled = true;
    enableStackCapturingIfNeeded();

    m_state->setBoolean(ConsoleAgentState::consoleMessagesEnabled, true);

    ConsoleMessageStorage* storage = messageStorage();
    if (storage->expiredCount()) {
        RefPtrWillBeRawPtr<ConsoleMessage> expiredMessage = ConsoleMessage::create(OtherMessageSource, WarningMessageLevel, String::format("%d console messages are not shown.", storage->expiredCount()));
        expiredMessage->setTimestamp(0);
        sendConsoleMessageToFrontend(expiredMessage.get(), false);
    }

    size_t messageCount = storage->size();
    for (size_t i = 0; i < messageCount; ++i)
        sendConsoleMessageToFrontend(storage->at(i), false);
}

void InspectorConsoleAgent::disable(ErrorString*)
{
    if (!m_enabled)
        return;
    m_instrumentingAgents->setInspectorConsoleAgent(nullptr);
    m_enabled = false;
    disableStackCapturingIfNeeded();

    m_state->setBoolean(ConsoleAgentState::consoleMessagesEnabled, false);
}

void InspectorConsoleAgent::restore()
{
    if (m_state->booleanProperty(ConsoleAgentState::consoleMessagesEnabled, false)) {
        frontend()->messagesCleared();
        ErrorString error;
        enable(&error);
    }
}

void InspectorConsoleAgent::addMessageToConsole(ConsoleMessage* consoleMessage)
{
    sendConsoleMessageToFrontend(consoleMessage, true);
    if (consoleMessage->type() != AssertMessageType)
        return;
    if (!m_debuggerAgent || !m_debuggerAgent->enabled())
        return;
    m_debuggerAgent->breakProgramOnException(protocol::Frontend::Debugger::Reason::Assert, nullptr);
}

void InspectorConsoleAgent::clearAllMessages()
{
    ErrorString error;
    clearMessages(&error);
}

void InspectorConsoleAgent::consoleMessagesCleared()
{
    m_runtimeAgent->disposeObjectGroup("console");
    frontend()->messagesCleared();
}

static protocol::TypeBuilder::Console::ConsoleMessage::Source::Enum messageSourceValue(MessageSource source)
{
    switch (source) {
    case XMLMessageSource: return protocol::TypeBuilder::Console::ConsoleMessage::Source::Xml;
    case JSMessageSource: return protocol::TypeBuilder::Console::ConsoleMessage::Source::Javascript;
    case NetworkMessageSource: return protocol::TypeBuilder::Console::ConsoleMessage::Source::Network;
    case ConsoleAPIMessageSource: return protocol::TypeBuilder::Console::ConsoleMessage::Source::Console_api;
    case StorageMessageSource: return protocol::TypeBuilder::Console::ConsoleMessage::Source::Storage;
    case AppCacheMessageSource: return protocol::TypeBuilder::Console::ConsoleMessage::Source::Appcache;
    case RenderingMessageSource: return protocol::TypeBuilder::Console::ConsoleMessage::Source::Rendering;
    case SecurityMessageSource: return protocol::TypeBuilder::Console::ConsoleMessage::Source::Security;
    case OtherMessageSource: return protocol::TypeBuilder::Console::ConsoleMessage::Source::Other;
    case DeprecationMessageSource: return protocol::TypeBuilder::Console::ConsoleMessage::Source::Deprecation;
    }
    return protocol::TypeBuilder::Console::ConsoleMessage::Source::Other;
}


static protocol::TypeBuilder::Console::ConsoleMessage::Type::Enum messageTypeValue(MessageType type)
{
    switch (type) {
    case LogMessageType: return protocol::TypeBuilder::Console::ConsoleMessage::Type::Log;
    case ClearMessageType: return protocol::TypeBuilder::Console::ConsoleMessage::Type::Clear;
    case DirMessageType: return protocol::TypeBuilder::Console::ConsoleMessage::Type::Dir;
    case DirXMLMessageType: return protocol::TypeBuilder::Console::ConsoleMessage::Type::Dirxml;
    case TableMessageType: return protocol::TypeBuilder::Console::ConsoleMessage::Type::Table;
    case TraceMessageType: return protocol::TypeBuilder::Console::ConsoleMessage::Type::Trace;
    case StartGroupMessageType: return protocol::TypeBuilder::Console::ConsoleMessage::Type::StartGroup;
    case StartGroupCollapsedMessageType: return protocol::TypeBuilder::Console::ConsoleMessage::Type::StartGroupCollapsed;
    case EndGroupMessageType: return protocol::TypeBuilder::Console::ConsoleMessage::Type::EndGroup;
    case AssertMessageType: return protocol::TypeBuilder::Console::ConsoleMessage::Type::Assert;
    case TimeEndMessageType: return protocol::TypeBuilder::Console::ConsoleMessage::Type::Log;
    case CountMessageType: return protocol::TypeBuilder::Console::ConsoleMessage::Type::Log;
    }
    return protocol::TypeBuilder::Console::ConsoleMessage::Type::Log;
}

static protocol::TypeBuilder::Console::ConsoleMessage::Level::Enum messageLevelValue(MessageLevel level)
{
    switch (level) {
    case DebugMessageLevel: return protocol::TypeBuilder::Console::ConsoleMessage::Level::Debug;
    case LogMessageLevel: return protocol::TypeBuilder::Console::ConsoleMessage::Level::Log;
    case WarningMessageLevel: return protocol::TypeBuilder::Console::ConsoleMessage::Level::Warning;
    case ErrorMessageLevel: return protocol::TypeBuilder::Console::ConsoleMessage::Level::Error;
    case InfoMessageLevel: return protocol::TypeBuilder::Console::ConsoleMessage::Level::Info;
    case RevokedErrorMessageLevel: return protocol::TypeBuilder::Console::ConsoleMessage::Level::RevokedError;
    }
    return protocol::TypeBuilder::Console::ConsoleMessage::Level::Log;
}

void InspectorConsoleAgent::sendConsoleMessageToFrontend(ConsoleMessage* consoleMessage, bool generatePreview)
{
    if (consoleMessage->workerGlobalScopeProxy())
        return;

    RefPtr<protocol::TypeBuilder::Console::ConsoleMessage> jsonObj = protocol::TypeBuilder::Console::ConsoleMessage::create()
        .setSource(messageSourceValue(consoleMessage->source()))
        .setLevel(messageLevelValue(consoleMessage->level()))
        .setText(consoleMessage->message())
        .setTimestamp(consoleMessage->timestamp());
    // FIXME: only send out type for ConsoleAPI source messages.
    jsonObj->setType(messageTypeValue(consoleMessage->type()));
    jsonObj->setLine(static_cast<int>(consoleMessage->lineNumber()));
    jsonObj->setColumn(static_cast<int>(consoleMessage->columnNumber()));
    if (consoleMessage->scriptId())
        jsonObj->setScriptId(String::number(consoleMessage->scriptId()));
    jsonObj->setUrl(consoleMessage->url());
    ScriptState* scriptState = consoleMessage->scriptState();
    if (scriptState)
        jsonObj->setExecutionContextId(scriptState->contextIdInDebugger());
    if (consoleMessage->source() == NetworkMessageSource && consoleMessage->requestIdentifier())
        jsonObj->setNetworkRequestId(IdentifiersFactory::requestId(consoleMessage->requestIdentifier()));
    RefPtrWillBeRawPtr<ScriptArguments> arguments = consoleMessage->scriptArguments();
    if (arguments && arguments->argumentCount()) {
        ScriptState::Scope scope(arguments->scriptState());
        v8::Local<v8::Context> context = arguments->scriptState()->context();
        RefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::Runtime::RemoteObject>> jsonArgs = protocol::TypeBuilder::Array<protocol::TypeBuilder::Runtime::RemoteObject>::create();
        if (consoleMessage->type() == TableMessageType && generatePreview) {
            v8::Local<v8::Value> table = arguments->argumentAt(0).v8Value();
            v8::Local<v8::Value> columns = arguments->argumentCount() > 1 ? arguments->argumentAt(1).v8Value() : v8::Local<v8::Value>();
            RefPtr<protocol::TypeBuilder::Runtime::RemoteObject> inspectorValue = m_runtimeAgent->wrapTable(context, table, columns);
            if (inspectorValue)
                jsonArgs->addItem(inspectorValue);
            else
                jsonArgs = nullptr;
        } else {
            for (unsigned i = 0; i < arguments->argumentCount(); ++i) {
                RefPtr<protocol::TypeBuilder::Runtime::RemoteObject> inspectorValue = m_runtimeAgent->wrapObject(context, arguments->argumentAt(i).v8Value(), "console", generatePreview);
                if (!inspectorValue) {
                    jsonArgs = nullptr;
                    break;
                }
                jsonArgs->addItem(inspectorValue);
            }
        }
        if (jsonArgs)
            jsonObj->setParameters(jsonArgs);
    }
    if (consoleMessage->callStack())
        jsonObj->setStack(consoleMessage->callStack()->buildInspectorObject());
    if (consoleMessage->messageId())
        jsonObj->setMessageId(consoleMessage->messageId());
    if (consoleMessage->relatedMessageId())
        jsonObj->setRelatedMessageId(consoleMessage->relatedMessageId());
    frontend()->messageAdded(jsonObj);
    frontend()->flush();
}

} // namespace blink

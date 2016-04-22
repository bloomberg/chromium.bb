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
#include "core/inspector/ScriptArguments.h"
#include "platform/v8_inspector/public/V8RuntimeAgent.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace ConsoleAgentState {
static const char consoleMessagesEnabled[] = "consoleMessagesEnabled";
}

InspectorConsoleAgent::InspectorConsoleAgent(V8RuntimeAgent* runtimeAgent)
    : InspectorBaseAgent<InspectorConsoleAgent, protocol::Frontend::Console>("Console")
    , m_runtimeAgent(runtimeAgent)
    , m_enabled(false)
{
}

InspectorConsoleAgent::~InspectorConsoleAgent()
{
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
        ConsoleMessage* expiredMessage = ConsoleMessage::create(OtherMessageSource, WarningMessageLevel, String::format("%d console messages are not shown.", storage->expiredCount()));
        expiredMessage->setTimestamp(0);
        sendConsoleMessageToFrontend(expiredMessage, false);
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

static String messageSourceValue(MessageSource source)
{
    switch (source) {
    case XMLMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::Xml;
    case JSMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::Javascript;
    case NetworkMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::Network;
    case ConsoleAPIMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::ConsoleApi;
    case StorageMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::Storage;
    case AppCacheMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::Appcache;
    case RenderingMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::Rendering;
    case SecurityMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::Security;
    case OtherMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::Other;
    case DeprecationMessageSource: return protocol::Console::ConsoleMessage::SourceEnum::Deprecation;
    }
    return protocol::Console::ConsoleMessage::SourceEnum::Other;
}


static String messageTypeValue(MessageType type)
{
    switch (type) {
    case LogMessageType: return protocol::Console::ConsoleMessage::TypeEnum::Log;
    case ClearMessageType: return protocol::Console::ConsoleMessage::TypeEnum::Clear;
    case DirMessageType: return protocol::Console::ConsoleMessage::TypeEnum::Dir;
    case DirXMLMessageType: return protocol::Console::ConsoleMessage::TypeEnum::Dirxml;
    case TableMessageType: return protocol::Console::ConsoleMessage::TypeEnum::Table;
    case TraceMessageType: return protocol::Console::ConsoleMessage::TypeEnum::Trace;
    case StartGroupMessageType: return protocol::Console::ConsoleMessage::TypeEnum::StartGroup;
    case StartGroupCollapsedMessageType: return protocol::Console::ConsoleMessage::TypeEnum::StartGroupCollapsed;
    case EndGroupMessageType: return protocol::Console::ConsoleMessage::TypeEnum::EndGroup;
    case AssertMessageType: return protocol::Console::ConsoleMessage::TypeEnum::Assert;
    case TimeEndMessageType: return protocol::Console::ConsoleMessage::TypeEnum::Log;
    case CountMessageType: return protocol::Console::ConsoleMessage::TypeEnum::Log;
    }
    return protocol::Console::ConsoleMessage::TypeEnum::Log;
}

static String messageLevelValue(MessageLevel level)
{
    switch (level) {
    case DebugMessageLevel: return protocol::Console::ConsoleMessage::LevelEnum::Debug;
    case LogMessageLevel: return protocol::Console::ConsoleMessage::LevelEnum::Log;
    case WarningMessageLevel: return protocol::Console::ConsoleMessage::LevelEnum::Warning;
    case ErrorMessageLevel: return protocol::Console::ConsoleMessage::LevelEnum::Error;
    case InfoMessageLevel: return protocol::Console::ConsoleMessage::LevelEnum::Info;
    case RevokedErrorMessageLevel: return protocol::Console::ConsoleMessage::LevelEnum::RevokedError;
    }
    return protocol::Console::ConsoleMessage::LevelEnum::Log;
}

void InspectorConsoleAgent::sendConsoleMessageToFrontend(ConsoleMessage* consoleMessage, bool generatePreview)
{
    if (consoleMessage->workerInspectorProxy())
        return;

    OwnPtr<protocol::Console::ConsoleMessage> jsonObj = protocol::Console::ConsoleMessage::create()
        .setSource(messageSourceValue(consoleMessage->source()))
        .setLevel(messageLevelValue(consoleMessage->level()))
        .setText(consoleMessage->message())
        .setTimestamp(consoleMessage->timestamp()).build();
    // FIXME: only send out type for ConsoleAPI source messages.
    jsonObj->setType(messageTypeValue(consoleMessage->type()));
    jsonObj->setLine(static_cast<int>(consoleMessage->lineNumber()));
    jsonObj->setColumn(static_cast<int>(consoleMessage->columnNumber()));
    if (consoleMessage->scriptId())
        jsonObj->setScriptId(String::number(consoleMessage->scriptId()));
    jsonObj->setUrl(consoleMessage->url());
    ScriptState* scriptState = consoleMessage->getScriptState();
    if (scriptState)
        jsonObj->setExecutionContextId(scriptState->contextIdInDebugger());
    if (consoleMessage->source() == NetworkMessageSource && consoleMessage->requestIdentifier())
        jsonObj->setNetworkRequestId(IdentifiersFactory::requestId(consoleMessage->requestIdentifier()));
    ScriptArguments* arguments = consoleMessage->scriptArguments();
    if (arguments && arguments->argumentCount()) {
        ScriptState::Scope scope(arguments->getScriptState());
        v8::Local<v8::Context> context = arguments->getScriptState()->context();
        OwnPtr<protocol::Array<protocol::Runtime::RemoteObject>> jsonArgs = protocol::Array<protocol::Runtime::RemoteObject>::create();
        if (consoleMessage->type() == TableMessageType && generatePreview) {
            v8::Local<v8::Value> table = arguments->argumentAt(0).v8Value();
            v8::Local<v8::Value> columns = arguments->argumentCount() > 1 ? arguments->argumentAt(1).v8Value() : v8::Local<v8::Value>();
            OwnPtr<protocol::Runtime::RemoteObject> inspectorValue = m_runtimeAgent->wrapTable(context, table, columns);
            if (inspectorValue)
                jsonArgs->addItem(inspectorValue.release());
            else
                jsonArgs = nullptr;
        } else {
            for (unsigned i = 0; i < arguments->argumentCount(); ++i) {
                OwnPtr<protocol::Runtime::RemoteObject> inspectorValue = m_runtimeAgent->wrapObject(context, arguments->argumentAt(i).v8Value(), "console", generatePreview);
                if (!inspectorValue) {
                    jsonArgs = nullptr;
                    break;
                }
                jsonArgs->addItem(inspectorValue.release());
            }
        }
        if (jsonArgs)
            jsonObj->setParameters(jsonArgs.release());
    }
    if (consoleMessage->callStack())
        jsonObj->setStack(consoleMessage->callStack()->buildInspectorObject());
    if (consoleMessage->messageId())
        jsonObj->setMessageId(consoleMessage->messageId());
    if (consoleMessage->relatedMessageId())
        jsonObj->setRelatedMessageId(consoleMessage->relatedMessageId());
    frontend()->messageAdded(jsonObj.release());
    frontend()->flush();
}

} // namespace blink

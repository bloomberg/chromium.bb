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


#include "config.h"
#include "core/inspector/InspectorConsoleAgent.h"

#include "bindings/core/v8/ScriptCallStackFactory.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptProfiler.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InjectedScript.h"
#include "core/inspector/InjectedScriptHost.h"
#include "core/inspector/InjectedScriptManager.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InspectorTimelineAgent.h"
#include "core/inspector/InspectorTracingAgent.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/ScriptArguments.h"
#include "core/inspector/ScriptAsyncCallStack.h"
#include "core/inspector/ScriptCallFrame.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/Page.h"
#include "core/xml/XMLHttpRequest.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceResponse.h"
#include "wtf/CurrentTime.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace ConsoleAgentState {
static const char monitoringXHR[] = "monitoringXHR";
static const char consoleMessagesEnabled[] = "consoleMessagesEnabled";
static const char tracingBasedTimeline[] = "tracingBasedTimeline";
}

int InspectorConsoleAgent::s_enabledAgentCount = 0;

InspectorConsoleAgent::InspectorConsoleAgent(InspectorTimelineAgent* timelineAgent, InspectorTracingAgent* tracingAgent, InjectedScriptManager* injectedScriptManager)
    : InspectorBaseAgent<InspectorConsoleAgent>("Console")
    , m_timelineAgent(timelineAgent)
    , m_tracingAgent(tracingAgent)
    , m_injectedScriptManager(injectedScriptManager)
    , m_frontend(0)
    , m_enabled(false)
{
}

InspectorConsoleAgent::~InspectorConsoleAgent()
{
#if !ENABLE(OILPAN)
    m_instrumentingAgents->setInspectorConsoleAgent(0);
#endif
}

void InspectorConsoleAgent::trace(Visitor* visitor)
{
    visitor->trace(m_timelineAgent);
    visitor->trace(m_tracingAgent);
    visitor->trace(m_injectedScriptManager);
    InspectorBaseAgent::trace(visitor);
}

void InspectorConsoleAgent::init()
{
    m_instrumentingAgents->setInspectorConsoleAgent(this);
}

void InspectorConsoleAgent::enable(ErrorString*)
{
    if (m_enabled)
        return;
    m_enabled = true;
    if (!s_enabledAgentCount)
        ScriptController::setCaptureCallStackForUncaughtExceptions(true);
    ++s_enabledAgentCount;

    m_state->setBoolean(ConsoleAgentState::consoleMessagesEnabled, true);

    ConsoleMessageStorage* storage = messageStorage();
    if (storage->expiredCount()) {
        RefPtrWillBeRawPtr<ConsoleMessage> expiredMessage = ConsoleMessage::create(OtherMessageSource, WarningMessageLevel, String::format("%d console messages are not shown.", storage->expiredCount()));
        expiredMessage->setTimestamp(0);
        sendConsoleMessageToFrontend(expiredMessage.get(), false);
    }

    size_t messageCount = storage->size();
    for (size_t i = 0; i < messageCount; ++i)
        sendConsoleMessageToFrontend(storage->at(i).get(), false);
}

void InspectorConsoleAgent::disable(ErrorString*)
{
    if (!m_enabled)
        return;
    m_enabled = false;
    if (!(--s_enabledAgentCount))
        ScriptController::setCaptureCallStackForUncaughtExceptions(false);
    m_state->setBoolean(ConsoleAgentState::consoleMessagesEnabled, false);
    m_state->setBoolean(ConsoleAgentState::tracingBasedTimeline, false);
}

void InspectorConsoleAgent::clearMessages(ErrorString*)
{
    messageStorage()->clear();
}

void InspectorConsoleAgent::reset()
{
    ErrorString error;
    clearMessages(&error);
    m_times.clear();
    m_counts.clear();
}

void InspectorConsoleAgent::restore()
{
    if (m_state->getBoolean(ConsoleAgentState::consoleMessagesEnabled)) {
        m_frontend->messagesCleared();
        ErrorString error;
        enable(&error);
    }
}

void InspectorConsoleAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->console();
}

void InspectorConsoleAgent::clearFrontend()
{
    m_frontend = 0;
    String errorString;
    disable(&errorString);
}

void InspectorConsoleAgent::addMessageToConsole(ConsoleMessage* consoleMessage)
{
    if (m_frontend && m_enabled)
        sendConsoleMessageToFrontend(consoleMessage, true);
}

void InspectorConsoleAgent::consoleMessagesCleared()
{
    m_injectedScriptManager->releaseObjectGroup("console");
    if (m_frontend && m_enabled)
        m_frontend->messagesCleared();
}

void InspectorConsoleAgent::consoleTime(ExecutionContext*, const String& title)
{
    // Follow Firebug's behavior of requiring a title that is not null or
    // undefined for timing functions
    if (title.isNull())
        return;

    m_times.add(title, monotonicallyIncreasingTime());
}

void InspectorConsoleAgent::consoleTimeEnd(ExecutionContext*, const String& title, ScriptState* scriptState)
{
    // Follow Firebug's behavior of requiring a title that is not null or
    // undefined for timing functions
    if (title.isNull())
        return;

    HashMap<String, double>::iterator it = m_times.find(title);
    if (it == m_times.end())
        return;

    double startTime = it->value;
    m_times.remove(it);

    double elapsed = monotonicallyIncreasingTime() - startTime;
    String message = title + String::format(": %.3fms", elapsed * 1000);

    RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(ConsoleAPIMessageSource, DebugMessageLevel, message);
    consoleMessage->setType(LogMessageType);
    consoleMessage->setScriptState(scriptState);
    messageStorage()->reportMessage(consoleMessage.release());
}

void InspectorConsoleAgent::setTracingBasedTimeline(ErrorString*, bool enabled)
{
    m_state->setBoolean(ConsoleAgentState::tracingBasedTimeline, enabled);
}

void InspectorConsoleAgent::consoleTimeline(ExecutionContext* context, const String& title, ScriptState* scriptState)
{
    UseCounter::count(context, UseCounter::DevToolsConsoleTimeline);
    if (m_tracingAgent && m_state->getBoolean(ConsoleAgentState::tracingBasedTimeline))
        m_tracingAgent->consoleTimeline(title);
    else
        m_timelineAgent->consoleTimeline(context, title, scriptState);
}

void InspectorConsoleAgent::consoleTimelineEnd(ExecutionContext* context, const String& title, ScriptState* scriptState)
{
    if (m_state->getBoolean(ConsoleAgentState::tracingBasedTimeline))
        m_tracingAgent->consoleTimelineEnd(title);
    else
        m_timelineAgent->consoleTimelineEnd(context, title, scriptState);
}

void InspectorConsoleAgent::consoleCount(ScriptState* scriptState, PassRefPtrWillBeRawPtr<ScriptArguments> arguments)
{
    RefPtrWillBeRawPtr<ScriptCallStack> callStack(createScriptCallStack(1));
    const ScriptCallFrame& lastCaller = callStack->at(0);
    // Follow Firebug's behavior of counting with null and undefined title in
    // the same bucket as no argument
    String title;
    arguments->getFirstArgumentAsString(title);
    String identifier = title.isEmpty() ? String(lastCaller.sourceURL() + ':' + String::number(lastCaller.lineNumber()))
                                        : String(title + '@');

    HashCountedSet<String>::AddResult result = m_counts.add(identifier);
    String message = title + ": " + String::number(result.storedValue->value);

    RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(ConsoleAPIMessageSource, DebugMessageLevel, message);
    consoleMessage->setType(LogMessageType);
    consoleMessage->setScriptState(scriptState);
    messageStorage()->reportMessage(consoleMessage.release());
}

void InspectorConsoleAgent::frameWindowDiscarded(LocalDOMWindow* window)
{
    m_injectedScriptManager->discardInjectedScriptsFor(window);
}

void InspectorConsoleAgent::didCommitLoad(LocalFrame* frame, DocumentLoader* loader)
{
    if (loader->frame() != frame->page()->mainFrame())
        return;
    reset();
}

void InspectorConsoleAgent::didFinishXHRLoading(XMLHttpRequest*, ThreadableLoaderClient*, unsigned long requestIdentifier, ScriptString, const AtomicString& method, const String& url, const String& sendURL, unsigned sendLineNumber)
{
    if (m_frontend && m_state->getBoolean(ConsoleAgentState::monitoringXHR)) {
        String message = "XHR finished loading: " + method + " \"" + url + "\".";
        RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(NetworkMessageSource, DebugMessageLevel, message, sendURL, sendLineNumber);
        consoleMessage->setRequestIdentifier(requestIdentifier);
        messageStorage()->reportMessage(consoleMessage.release());
    }
}

void InspectorConsoleAgent::didFailLoading(unsigned long requestIdentifier, const ResourceError& error)
{
    if (error.isCancellation()) // Report failures only.
        return;
    StringBuilder message;
    message.appendLiteral("Failed to load resource");
    if (!error.localizedDescription().isEmpty()) {
        message.appendLiteral(": ");
        message.append(error.localizedDescription());
    }
    RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(NetworkMessageSource, ErrorMessageLevel, message.toString(), error.failingURL());
    consoleMessage->setRequestIdentifier(requestIdentifier);
    messageStorage()->reportMessage(consoleMessage.release());
}

void InspectorConsoleAgent::setMonitoringXHREnabled(ErrorString*, bool enabled)
{
    m_state->setBoolean(ConsoleAgentState::monitoringXHR, enabled);
}

static TypeBuilder::Console::ConsoleMessage::Source::Enum messageSourceValue(MessageSource source)
{
    switch (source) {
    case XMLMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Xml;
    case JSMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Javascript;
    case NetworkMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Network;
    case ConsoleAPIMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Console_api;
    case StorageMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Storage;
    case AppCacheMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Appcache;
    case RenderingMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Rendering;
    case CSSMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Css;
    case SecurityMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Security;
    case OtherMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Other;
    case DeprecationMessageSource: return TypeBuilder::Console::ConsoleMessage::Source::Deprecation;
    }
    return TypeBuilder::Console::ConsoleMessage::Source::Other;
}


static TypeBuilder::Console::ConsoleMessage::Type::Enum messageTypeValue(MessageType type)
{
    switch (type) {
    case LogMessageType: return TypeBuilder::Console::ConsoleMessage::Type::Log;
    case ClearMessageType: return TypeBuilder::Console::ConsoleMessage::Type::Clear;
    case DirMessageType: return TypeBuilder::Console::ConsoleMessage::Type::Dir;
    case DirXMLMessageType: return TypeBuilder::Console::ConsoleMessage::Type::Dirxml;
    case TableMessageType: return TypeBuilder::Console::ConsoleMessage::Type::Table;
    case TraceMessageType: return TypeBuilder::Console::ConsoleMessage::Type::Trace;
    case StartGroupMessageType: return TypeBuilder::Console::ConsoleMessage::Type::StartGroup;
    case StartGroupCollapsedMessageType: return TypeBuilder::Console::ConsoleMessage::Type::StartGroupCollapsed;
    case EndGroupMessageType: return TypeBuilder::Console::ConsoleMessage::Type::EndGroup;
    case AssertMessageType: return TypeBuilder::Console::ConsoleMessage::Type::Assert;
    }
    return TypeBuilder::Console::ConsoleMessage::Type::Log;
}

static TypeBuilder::Console::ConsoleMessage::Level::Enum messageLevelValue(MessageLevel level)
{
    switch (level) {
    case DebugMessageLevel: return TypeBuilder::Console::ConsoleMessage::Level::Debug;
    case LogMessageLevel: return TypeBuilder::Console::ConsoleMessage::Level::Log;
    case WarningMessageLevel: return TypeBuilder::Console::ConsoleMessage::Level::Warning;
    case ErrorMessageLevel: return TypeBuilder::Console::ConsoleMessage::Level::Error;
    case InfoMessageLevel: return TypeBuilder::Console::ConsoleMessage::Level::Info;
    }
    return TypeBuilder::Console::ConsoleMessage::Level::Log;
}

void InspectorConsoleAgent::sendConsoleMessageToFrontend(ConsoleMessage* consoleMessage, bool generatePreview)
{
    if (consoleMessage->workerGlobalScopeProxy())
        return;

    RefPtr<TypeBuilder::Console::ConsoleMessage> jsonObj = TypeBuilder::Console::ConsoleMessage::create()
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
        jsonObj->setExecutionContextId(m_injectedScriptManager->injectedScriptIdFor(scriptState));
    if (consoleMessage->source() == NetworkMessageSource && consoleMessage->requestIdentifier())
        jsonObj->setNetworkRequestId(IdentifiersFactory::requestId(consoleMessage->requestIdentifier()));
    RefPtrWillBeRawPtr<ScriptArguments> arguments = consoleMessage->scriptArguments();
    if (arguments && arguments->argumentCount()) {
        InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(arguments->scriptState());
        if (!injectedScript.isEmpty()) {
            RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::RemoteObject> > jsonArgs = TypeBuilder::Array<TypeBuilder::Runtime::RemoteObject>::create();
            if (consoleMessage->type() == TableMessageType && generatePreview && arguments->argumentCount()) {
                ScriptValue table = arguments->argumentAt(0);
                ScriptValue columns = arguments->argumentCount() > 1 ? arguments->argumentAt(1) : ScriptValue();
                RefPtr<TypeBuilder::Runtime::RemoteObject> inspectorValue = injectedScript.wrapTable(table, columns);
                if (!inspectorValue) {
                    ASSERT_NOT_REACHED();
                    return;
                }
                jsonArgs->addItem(inspectorValue);
            } else {
                for (unsigned i = 0; i < arguments->argumentCount(); ++i) {
                    RefPtr<TypeBuilder::Runtime::RemoteObject> inspectorValue = injectedScript.wrapObject(arguments->argumentAt(i), "console", generatePreview);
                    if (!inspectorValue) {
                        ASSERT_NOT_REACHED();
                        return;
                    }
                    jsonArgs->addItem(inspectorValue);
                }
            }
            jsonObj->setParameters(jsonArgs);
        }
    }
    if (consoleMessage->callStack()) {
        jsonObj->setStackTrace(consoleMessage->callStack()->buildInspectorArray());
        RefPtrWillBeRawPtr<ScriptAsyncCallStack> asyncCallStack = consoleMessage->callStack()->asyncCallStack();
        if (asyncCallStack)
            jsonObj->setAsyncStackTrace(asyncCallStack->buildInspectorObject());
    }
    m_frontend->messageAdded(jsonObj);
    m_frontend->flush();
}

class InspectableHeapObject FINAL : public InjectedScriptHost::InspectableObject {
public:
    explicit InspectableHeapObject(int heapObjectId) : m_heapObjectId(heapObjectId) { }
    virtual ScriptValue get(ScriptState*) OVERRIDE
    {
        return ScriptProfiler::objectByHeapObjectId(m_heapObjectId);
    }
private:
    int m_heapObjectId;
};

void InspectorConsoleAgent::addInspectedHeapObject(ErrorString*, int inspectedHeapObjectId)
{
    m_injectedScriptManager->injectedScriptHost()->addInspectedObject(adoptPtr(new InspectableHeapObject(inspectedHeapObjectId)));
}

} // namespace blink

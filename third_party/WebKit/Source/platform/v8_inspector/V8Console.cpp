// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/V8Console.h"

#include "platform/inspector_protocol/String16.h"
#include "platform/v8_inspector/InjectedScript.h"
#include "platform/v8_inspector/InspectedContext.h"
#include "platform/v8_inspector/V8DebuggerAgentImpl.h"
#include "platform/v8_inspector/V8DebuggerImpl.h"
#include "platform/v8_inspector/V8InspectorSessionImpl.h"
#include "platform/v8_inspector/V8ProfilerAgentImpl.h"
#include "platform/v8_inspector/V8StackTraceImpl.h"
#include "platform/v8_inspector/V8StringUtil.h"
#include "platform/v8_inspector/public/ConsoleAPITypes.h"
#include "platform/v8_inspector/public/ConsoleTypes.h"
#include "platform/v8_inspector/public/V8DebuggerClient.h"

namespace blink {

namespace {

v8::Local<v8::Private> inspectedContextPrivateKey(v8::Isolate* isolate)
{
    return v8::Private::ForApi(isolate, toV8StringInternalized(isolate, "V8Console#InspectedContext"));
}

class ConsoleHelper {
    PROTOCOL_DISALLOW_COPY(ConsoleHelper);
public:
    ConsoleHelper(const v8::FunctionCallbackInfo<v8::Value>& info)
        : m_info(info)
        , m_isolate(info.GetIsolate())
        , m_context(info.GetIsolate()->GetCurrentContext())
        , m_inspectedContext(nullptr)
        , m_debuggerClient(nullptr)
    {
    }

    v8::Local<v8::Object> ensureConsole()
    {
        if (m_console.IsEmpty()) {
            ASSERT(!m_info.Data().IsEmpty());
            ASSERT(!m_info.Data()->IsUndefined());
            m_console = m_info.Data().As<v8::Object>();
        }
        return m_console;
    }

    InspectedContext* ensureInspectedContext()
    {
        if (m_inspectedContext)
            return m_inspectedContext;
        v8::Local<v8::Object> console = ensureConsole();

        v8::Local<v8::Private> key = inspectedContextPrivateKey(m_isolate);
        v8::Local<v8::Value> inspectedContextValue;
        if (!console->GetPrivate(m_context, key).ToLocal(&inspectedContextValue))
            return nullptr;
        ASSERT(inspectedContextValue->IsExternal());
        m_inspectedContext = static_cast<InspectedContext*>(inspectedContextValue.As<v8::External>()->Value());
        return m_inspectedContext;
    }

    V8DebuggerClient* ensureDebuggerClient()
    {
        if (m_debuggerClient)
            return m_debuggerClient;
        InspectedContext* inspectedContext = ensureInspectedContext();
        if (!inspectedContext)
            return nullptr;
        m_debuggerClient = inspectedContext->debugger()->client();
        return m_debuggerClient;
    }

    void addMessage(MessageType type, MessageLevel level, bool allowEmptyArguments, int skipArgumentCount)
    {
        if (!allowEmptyArguments && !m_info.Length())
            return;
        if (V8DebuggerClient* debuggerClient = ensureDebuggerClient())
            debuggerClient->reportMessageToConsole(m_context, type, level, String16(), &m_info, skipArgumentCount, -1);
    }

    void addMessage(MessageType type, MessageLevel level, const String16& message)
    {
        if (V8DebuggerClient* debuggerClient = ensureDebuggerClient())
            debuggerClient->reportMessageToConsole(m_context, type, level, message, nullptr, 0 /* skipArgumentsCount */, 1 /* maxStackSize */);
    }

    void addDeprecationMessage(const char* id, const String16& message)
    {
        if (checkAndSetPrivateFlagOnConsole(id, false))
            return;
        if (V8DebuggerClient* debuggerClient = ensureDebuggerClient())
            debuggerClient->reportMessageToConsole(m_context, LogMessageType, WarningMessageLevel, message, nullptr, 0 /* skipArgumentsCount */, 0 /* maxStackSize */);
    }

    bool firstArgToBoolean(bool defaultValue)
    {
        if (m_info.Length() < 1)
            return defaultValue;
        if (m_info[0]->IsBoolean())
            return m_info[0].As<v8::Boolean>()->Value();
        return m_info[0]->BooleanValue(m_context).FromMaybe(defaultValue);
    }

    String16 firstArgToString(const String16& defaultValue)
    {
        if (m_info.Length() < 1)
            return defaultValue;
        v8::Local<v8::String> titleValue;
        if (m_info[0]->IsObject()) {
            if (!m_info[0].As<v8::Object>()->ObjectProtoToString(m_context).ToLocal(&titleValue))
                return defaultValue;
        } else {
            if (!m_info[0]->ToString(m_context).ToLocal(&titleValue))
                return defaultValue;
        }
        return toProtocolString(titleValue);
    }

    v8::MaybeLocal<v8::Object> firstArgAsObject()
    {
        if (m_info.Length() < 1 || !m_info[0]->IsObject())
            return v8::MaybeLocal<v8::Object>();
        return m_info[0].As<v8::Object>();
    }

    v8::MaybeLocal<v8::Function> firstArgAsFunction()
    {
        if (m_info.Length() < 1 || !m_info[0]->IsFunction())
            return v8::MaybeLocal<v8::Function>();
        return m_info[0].As<v8::Function>();
    }

    v8::MaybeLocal<v8::Map> privateMap(const char* name)
    {
        v8::Local<v8::Object> console = ensureConsole();
        v8::Local<v8::Private> privateKey = v8::Private::ForApi(m_isolate, toV8StringInternalized(m_isolate, name));
        v8::Local<v8::Value> mapValue;
        if (!console->GetPrivate(m_context, privateKey).ToLocal(&mapValue))
            return v8::MaybeLocal<v8::Map>();
        if (mapValue->IsUndefined()) {
            v8::Local<v8::Map> map = v8::Map::New(m_isolate);
            if (!console->SetPrivate(m_context, privateKey, map).FromMaybe(false))
                return v8::MaybeLocal<v8::Map>();
            return map;
        }
        return mapValue->IsMap() ? mapValue.As<v8::Map>() : v8::MaybeLocal<v8::Map>();
    }

    int64_t getIntFromMap(v8::Local<v8::Map> map, const String16& key, int64_t defaultValue)
    {
        v8::Local<v8::String> v8Key = toV8String(m_isolate, key);
        if (!map->Has(m_context, v8Key).FromMaybe(false))
            return defaultValue;
        v8::Local<v8::Value> intValue;
        if (!map->Get(m_context, v8Key).ToLocal(&intValue))
            return defaultValue;
        return intValue.As<v8::Integer>()->Value();
    }

    void setIntOnMap(v8::Local<v8::Map> map, const String16& key, int64_t value)
    {
        v8::Local<v8::String> v8Key = toV8String(m_isolate, key);
        if (!map->Set(m_context, v8Key, v8::Integer::New(m_isolate, value)).ToLocal(&map))
            return;
    }

    double getDoubleFromMap(v8::Local<v8::Map> map, const String16& key, double defaultValue)
    {
        v8::Local<v8::String> v8Key = toV8String(m_isolate, key);
        if (!map->Has(m_context, v8Key).FromMaybe(false))
            return defaultValue;
        v8::Local<v8::Value> intValue;
        if (!map->Get(m_context, v8Key).ToLocal(&intValue))
            return defaultValue;
        return intValue.As<v8::Number>()->Value();
    }

    void setDoubleOnMap(v8::Local<v8::Map> map, const String16& key, double value)
    {
        v8::Local<v8::String> v8Key = toV8String(m_isolate, key);
        if (!map->Set(m_context, v8Key, v8::Number::New(m_isolate, value)).ToLocal(&map))
            return;
    }

    V8ProfilerAgentImpl* profilerAgent()
    {
        if (V8InspectorSessionImpl* session = currentSession()) {
            if (session && session->profilerAgentImpl()->enabled())
                return session->profilerAgentImpl();
        }
        return nullptr;
    }

    V8DebuggerAgentImpl* debuggerAgent()
    {
        if (V8InspectorSessionImpl* session = currentSession()) {
            if (session && session->debuggerAgentImpl()->enabled())
                return session->debuggerAgentImpl();
        }
        return nullptr;
    }
private:
    const v8::FunctionCallbackInfo<v8::Value>& m_info;
    v8::Isolate* m_isolate;
    v8::Local<v8::Context> m_context;
    v8::Local<v8::Object> m_console;
    InspectedContext* m_inspectedContext;
    V8DebuggerClient* m_debuggerClient;

    bool checkAndSetPrivateFlagOnConsole(const char* name, bool defaultValue)
    {
        v8::Local<v8::Object> console = ensureConsole();
        v8::Local<v8::Private> key = v8::Private::ForApi(m_isolate, toV8StringInternalized(m_isolate, name));
        v8::Local<v8::Value> flagValue;
        if (!console->GetPrivate(m_context, key).ToLocal(&flagValue))
            return defaultValue;
        ASSERT(flagValue->IsUndefined() || flagValue->IsBoolean());
        if (flagValue->IsBoolean()) {
            ASSERT(flagValue.As<v8::Boolean>()->Value());
            return true;
        }
        if (!console->SetPrivate(m_context, key, v8::True(m_isolate)).FromMaybe(false))
            return defaultValue;
        return false;
    }

    V8InspectorSessionImpl* currentSession()
    {
        InspectedContext* inspectedContext = ensureInspectedContext();
        if (!inspectedContext)
            return nullptr;
        return inspectedContext->debugger()->sessionForContextGroup(inspectedContext->contextGroupId());
    }
};

v8::MaybeLocal<v8::Object> createObjectWithClassName(V8DebuggerImpl* debugger, v8::Local<v8::Context> context, const char* className)
{
    v8::Isolate* isolate = debugger->isolate();
    v8::Local<v8::FunctionTemplate> functionTemplate;
    String16 functionTemplateName = String16("V8Console#") + className;
    if (!debugger->functionTemplate(functionTemplateName).ToLocal(&functionTemplate)) {
        functionTemplate = v8::FunctionTemplate::New(isolate);
        functionTemplate->SetClassName(toV8StringInternalized(isolate, className));
        debugger->setFunctionTemplate(functionTemplateName, functionTemplate);
    }
    v8::Local<v8::Function> constructor;
    if (!functionTemplate->GetFunction(context).ToLocal(&constructor))
        return v8::MaybeLocal<v8::Object>();
    return constructor->NewInstance(context, 0, nullptr);
}

void createBoundFunctionProperty(v8::Local<v8::Context> context, v8::Local<v8::Object> obj, v8::Local<v8::Object> prototype, const char* name, v8::FunctionCallback callback)
{
    v8::Local<v8::String> funcName = toV8StringInternalized(context->GetIsolate(), name);
    v8::Local<v8::Function> func;
    if (!v8::Function::New(context, callback, obj).ToLocal(&func))
        return;
    func->SetName(funcName);
    if (!prototype->Set(context, funcName, func).FromMaybe(false))
        return;
}

} // namespace

void V8Console::debugCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper(info).addMessage(LogMessageType, DebugMessageLevel, false, 0);
}

void V8Console::errorCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper(info).addMessage(LogMessageType, ErrorMessageLevel, false, 0);
}

void V8Console::infoCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper(info).addMessage(LogMessageType, InfoMessageLevel, false, 0);
}

void V8Console::logCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper(info).addMessage(LogMessageType, LogMessageLevel, false, 0);
}

void V8Console::warnCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper(info).addMessage(LogMessageType, WarningMessageLevel, false, 0);
}

void V8Console::dirCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper(info).addMessage(DirMessageType, LogMessageLevel, false, 0);
}

void V8Console::dirxmlCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper(info).addMessage(DirXMLMessageType, LogMessageLevel, false, 0);
}

void V8Console::tableCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper(info).addMessage(TableMessageType, LogMessageLevel, false, 0);
}

void V8Console::traceCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper(info).addMessage(TraceMessageType, LogMessageLevel, true, 0);
}

void V8Console::groupCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper(info).addMessage(StartGroupMessageType, LogMessageLevel, true, 0);
}

void V8Console::groupCollapsedCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper(info).addMessage(StartGroupCollapsedMessageType, LogMessageLevel, true, 0);
}

void V8Console::groupEndCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper(info).addMessage(EndGroupMessageType, LogMessageLevel, true, 0);
}

void V8Console::clearCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper(info).addMessage(ClearMessageType, LogMessageLevel, true, 0);
}

void V8Console::countCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper helper(info);

    String16 title = helper.firstArgToString(String16());
    String16 identifier;
    if (title.isEmpty()) {
        OwnPtr<V8StackTraceImpl> stackTrace = V8StackTraceImpl::capture(nullptr, 1);
        if (stackTrace)
            identifier = stackTrace->topSourceURL() + ":" + String16::number(stackTrace->topLineNumber());
    } else {
        identifier = title + "@";
    }

    v8::Local<v8::Map> countMap;
    if (!helper.privateMap("V8Console#countMap").ToLocal(&countMap))
        return;
    int64_t count = helper.getIntFromMap(countMap, identifier, 0) + 1;
    helper.setIntOnMap(countMap, identifier, count);
    helper.addMessage(CountMessageType, DebugMessageLevel, title + ": " + String16::number(count));
}

void V8Console::assertCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper helper(info);
    if (helper.firstArgToBoolean(false))
        return;
    helper.addMessage(AssertMessageType, ErrorMessageLevel, true, 1);
    if (V8DebuggerAgentImpl* debuggerAgent = helper.debuggerAgent())
        debuggerAgent->breakProgramOnException(protocol::Debugger::Paused::ReasonEnum::Assert, nullptr);
}

void V8Console::markTimelineCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper(info).addDeprecationMessage("V8Console#markTimelineDeprecated", "'console.markTimeline' is deprecated. Please use 'console.timeStamp' instead.");
    timeStampCallback(info);
}

void V8Console::profileCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper helper(info);
    if (V8ProfilerAgentImpl* profilerAgent = helper.profilerAgent())
        profilerAgent->consoleProfile(helper.firstArgToString(String16()));
}

void V8Console::profileEndCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper helper(info);
    if (V8ProfilerAgentImpl* profilerAgent = helper.profilerAgent())
        profilerAgent->consoleProfileEnd(helper.firstArgToString(String16()));
}

static void timeFunction(const v8::FunctionCallbackInfo<v8::Value>& info, bool timelinePrefix)
{
    ConsoleHelper helper(info);
    if (V8DebuggerClient* client = helper.ensureDebuggerClient()) {
        String16 protocolTitle = helper.firstArgToString(String16());
        if (timelinePrefix)
            protocolTitle = "Timeline '" + protocolTitle + "'";
        client->consoleTime(protocolTitle);

        if (info.Length() < 1)
            return;
        v8::Local<v8::Map> timeMap;
        if (!helper.privateMap("V8Console#timeMap").ToLocal(&timeMap))
            return;
        helper.setDoubleOnMap(timeMap, protocolTitle, client->currentTimeMS());
    }
}

static void timeEndFunction(const v8::FunctionCallbackInfo<v8::Value>& info, bool timelinePrefix)
{
    ConsoleHelper helper(info);
    if (V8DebuggerClient* client = helper.ensureDebuggerClient()) {
        String16 protocolTitle = helper.firstArgToString(String16());
        if (timelinePrefix)
            protocolTitle = "Timeline '" + protocolTitle + "'";
        client->consoleTimeEnd(protocolTitle);

        if (info.Length() < 1)
            return;
        v8::Local<v8::Map> timeMap;
        if (!helper.privateMap("V8Console#timeMap").ToLocal(&timeMap))
            return;
        double elapsed = client->currentTimeMS() - helper.getDoubleFromMap(timeMap, protocolTitle, 0.0);
        String16 message = protocolTitle + ": " + String16::fromDoubleFixedPrecision(elapsed, 3) + "ms";
        helper.addMessage(TimeEndMessageType, DebugMessageLevel, message);
    }
}

void V8Console::timelineCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper(info).addDeprecationMessage("V8Console#timeline", "'console.timeline' is deprecated. Please use 'console.time' instead.");
    timeFunction(info, true);
}

void V8Console::timelineEndCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper(info).addDeprecationMessage("V8Console#timelineEnd", "'console.timelineEnd' is deprecated. Please use 'console.timeEnd' instead.");
    timeEndFunction(info, true);
}

void V8Console::timeCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    timeFunction(info, false);
}

void V8Console::timeEndCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    timeEndFunction(info, false);
}

void V8Console::timeStampCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper helper(info);
    if (V8DebuggerClient* client = helper.ensureDebuggerClient())
        client->consoleTimeStamp(helper.firstArgToString(String16()));
}

void V8Console::memoryGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (V8DebuggerClient* client = ConsoleHelper(info).ensureDebuggerClient()) {
        v8::Local<v8::Value> memoryValue;
        if (!client->memoryInfo(info.GetIsolate(), info.GetIsolate()->GetCurrentContext(), info.Holder()).ToLocal(&memoryValue))
            return;
        info.GetReturnValue().Set(memoryValue);
    }
}

void V8Console::memorySetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    // We can't make the attribute readonly as it breaks existing code that relies on being able to assign to console.memory in strict mode. Instead, the setter just ignores the passed value.  http://crbug.com/468611
}

void V8Console::keysCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* isolate = info.GetIsolate();
    info.GetReturnValue().Set(v8::Array::New(isolate));

    ConsoleHelper helper(info);
    v8::Local<v8::Object> obj;
    if (!helper.firstArgAsObject().ToLocal(&obj))
        return;
    v8::Local<v8::Array> names;
    if (!obj->GetOwnPropertyNames(isolate->GetCurrentContext()).ToLocal(&names))
        return;
    info.GetReturnValue().Set(names);
}

void V8Console::valuesCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* isolate = info.GetIsolate();
    info.GetReturnValue().Set(v8::Array::New(isolate));

    ConsoleHelper helper(info);
    v8::Local<v8::Object> obj;
    if (!helper.firstArgAsObject().ToLocal(&obj))
        return;
    v8::Local<v8::Array> names;
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    if (!obj->GetOwnPropertyNames(context).ToLocal(&names))
        return;
    v8::Local<v8::Array> values = v8::Array::New(isolate, names->Length());
    for (size_t i = 0; i < names->Length(); ++i) {
        v8::Local<v8::Value> key;
        if (!names->Get(context, i).ToLocal(&key))
            continue;
        v8::Local<v8::Value> value;
        if (!obj->Get(context, key).ToLocal(&value))
            continue;
        if (!values->Set(context, i, value).FromMaybe(false))
            continue;
    }
    info.GetReturnValue().Set(values);
}

static void setFunctionBreakpoint(ConsoleHelper& helper, v8::Local<v8::Function> function, V8DebuggerAgentImpl::BreakpointSource source, const String16& condition, bool enable)
{
    V8DebuggerAgentImpl* debuggerAgent = helper.debuggerAgent();
    if (!debuggerAgent)
        return;
    String16 scriptId = String16::number(function->ScriptId());
    int lineNumber = function->GetScriptLineNumber();
    int columnNumber = function->GetScriptColumnNumber();
    if (lineNumber == v8::Function::kLineOffsetNotFound || columnNumber == v8::Function::kLineOffsetNotFound)
        return;
    if (enable)
        debuggerAgent->setBreakpointAt(scriptId, lineNumber, columnNumber, source, condition);
    else
        debuggerAgent->removeBreakpointAt(scriptId, lineNumber, columnNumber, source);
}

void V8Console::debugFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper helper(info);
    v8::Local<v8::Function> function;
    if (!helper.firstArgAsFunction().ToLocal(&function))
        return;
    setFunctionBreakpoint(helper, function, V8DebuggerAgentImpl::DebugCommandBreakpointSource, String16(), true);
}

void V8Console::undebugFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper helper(info);
    v8::Local<v8::Function> function;
    if (!helper.firstArgAsFunction().ToLocal(&function))
        return;
    setFunctionBreakpoint(helper, function, V8DebuggerAgentImpl::DebugCommandBreakpointSource, String16(), false);
}

void V8Console::monitorFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper helper(info);
    v8::Local<v8::Function> function;
    if (!helper.firstArgAsFunction().ToLocal(&function))
        return;
    v8::Local<v8::Value> name = function->GetName();
    if (!name->IsString() || !v8::Local<v8::String>::Cast(name)->Length())
        name = function->GetInferredName();
    String16 functionName = toProtocolStringWithTypeCheck(name);
    String16Builder builder;
    builder.append("console.log(\"function ");
    if (functionName.isEmpty())
        builder.append("(anonymous function)");
    else
        builder.append(functionName);
    builder.append(" called\" + (arguments.length > 0 ? \" with arguments: \" + Array.prototype.join.call(arguments, \", \") : \"\")) && false");
    setFunctionBreakpoint(helper, function, V8DebuggerAgentImpl::MonitorCommandBreakpointSource, builder.toString(), true);
}

void V8Console::unmonitorFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper helper(info);
    v8::Local<v8::Function> function;
    if (!helper.firstArgAsFunction().ToLocal(&function))
        return;
    setFunctionBreakpoint(helper, function, V8DebuggerAgentImpl::MonitorCommandBreakpointSource, String16(), false);
}

void V8Console::lastEvaluationResultCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ConsoleHelper helper(info);
    InspectedContext* context = helper.ensureInspectedContext();
    if (!context)
        return;
    if (InjectedScript* injectedScript = context->getInjectedScript())
        info.GetReturnValue().Set(injectedScript->lastEvaluationResult());
}

v8::MaybeLocal<v8::Object> V8Console::createConsole(InspectedContext* inspectedContext, bool hasMemoryAttribute)
{
    v8::Local<v8::Context> context = inspectedContext->context();
    v8::Isolate* isolate = context->GetIsolate();
    v8::MicrotasksScope microtasksScope(isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);

    v8::Local<v8::Object> console;
    if (!createObjectWithClassName(inspectedContext->debugger(), context, "Console").ToLocal(&console))
        return v8::MaybeLocal<v8::Object>();

    v8::Local<v8::Object> prototype;
    if (!createObjectWithClassName(inspectedContext->debugger(), context, "ConsolePrototype").ToLocal(&prototype))
        return v8::MaybeLocal<v8::Object>();

    createBoundFunctionProperty(context, console, prototype, "debug", V8Console::debugCallback);
    createBoundFunctionProperty(context, console, prototype, "error", V8Console::errorCallback);
    createBoundFunctionProperty(context, console, prototype, "info", V8Console::infoCallback);
    createBoundFunctionProperty(context, console, prototype, "log", V8Console::logCallback);
    createBoundFunctionProperty(context, console, prototype, "warn", V8Console::warnCallback);
    createBoundFunctionProperty(context, console, prototype, "dir", V8Console::dirCallback);
    createBoundFunctionProperty(context, console, prototype, "dirxml", V8Console::dirxmlCallback);
    createBoundFunctionProperty(context, console, prototype, "table", V8Console::tableCallback);
    createBoundFunctionProperty(context, console, prototype, "trace", V8Console::traceCallback);
    createBoundFunctionProperty(context, console, prototype, "group", V8Console::groupCallback);
    createBoundFunctionProperty(context, console, prototype, "groupCollapsed", V8Console::groupCollapsedCallback);
    createBoundFunctionProperty(context, console, prototype, "groupEnd", V8Console::groupEndCallback);
    createBoundFunctionProperty(context, console, prototype, "clear", V8Console::clearCallback);
    createBoundFunctionProperty(context, console, prototype, "count", V8Console::countCallback);
    createBoundFunctionProperty(context, console, prototype, "assert", V8Console::assertCallback);
    createBoundFunctionProperty(context, console, prototype, "markTimeline", V8Console::markTimelineCallback);
    createBoundFunctionProperty(context, console, prototype, "profile", V8Console::profileCallback);
    createBoundFunctionProperty(context, console, prototype, "profileEnd", V8Console::profileEndCallback);
    createBoundFunctionProperty(context, console, prototype, "timeline", V8Console::timelineCallback);
    createBoundFunctionProperty(context, console, prototype, "timelineEnd", V8Console::timelineEndCallback);
    createBoundFunctionProperty(context, console, prototype, "time", V8Console::timeCallback);
    createBoundFunctionProperty(context, console, prototype, "timeEnd", V8Console::timeEndCallback);
    createBoundFunctionProperty(context, console, prototype, "timeStamp", V8Console::timeStampCallback);

    if (!console->SetPrototype(context, prototype).FromMaybe(false))
        return v8::MaybeLocal<v8::Object>();

    if (hasMemoryAttribute)
        console->SetAccessorProperty(toV8StringInternalized(isolate, "memory"), v8::Function::New(isolate, V8Console::memoryGetterCallback, console), v8::Function::New(isolate, V8Console::memorySetterCallback), static_cast<v8::PropertyAttribute>(v8::None), v8::DEFAULT);

    console->SetPrivate(context, inspectedContextPrivateKey(isolate), v8::External::New(isolate, inspectedContext));
    return console;
}

v8::Local<v8::Object> V8Console::createCommandLineAPI(InspectedContext* inspectedContext)
{
    v8::Local<v8::Context> context = inspectedContext->context();
    v8::Isolate* isolate = context->GetIsolate();
    v8::MicrotasksScope microtasksScope(isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);

    v8::Local<v8::Object> commandLineAPI = v8::Object::New(isolate);

    createBoundFunctionProperty(context, commandLineAPI, commandLineAPI, "dir", V8Console::dirCallback);
    createBoundFunctionProperty(context, commandLineAPI, commandLineAPI, "dirxml", V8Console::dirxmlCallback);
    createBoundFunctionProperty(context, commandLineAPI, commandLineAPI, "profile", V8Console::profileCallback);
    createBoundFunctionProperty(context, commandLineAPI, commandLineAPI, "profileEnd", V8Console::profileEndCallback);
    createBoundFunctionProperty(context, commandLineAPI, commandLineAPI, "clear", V8Console::clearCallback);
    createBoundFunctionProperty(context, commandLineAPI, commandLineAPI, "table", V8Console::tableCallback);

    createBoundFunctionProperty(context, commandLineAPI, commandLineAPI, "keys", V8Console::keysCallback);
    createBoundFunctionProperty(context, commandLineAPI, commandLineAPI, "values", V8Console::valuesCallback);
    createBoundFunctionProperty(context, commandLineAPI, commandLineAPI, "debug", V8Console::debugFunctionCallback);
    createBoundFunctionProperty(context, commandLineAPI, commandLineAPI, "undebug", V8Console::undebugFunctionCallback);
    createBoundFunctionProperty(context, commandLineAPI, commandLineAPI, "monitor", V8Console::monitorFunctionCallback);
    createBoundFunctionProperty(context, commandLineAPI, commandLineAPI, "unmonitor", V8Console::unmonitorFunctionCallback);
    createBoundFunctionProperty(context, commandLineAPI, commandLineAPI, "$_", V8Console::lastEvaluationResultCallback);

    commandLineAPI->SetPrivate(context, inspectedContextPrivateKey(isolate), v8::External::New(isolate, inspectedContext));
    return commandLineAPI;
}

void V8Console::clearInspectedContextIfNeeded(v8::Local<v8::Context> context, v8::Local<v8::Object> console)
{
    v8::Isolate* isolate = context->GetIsolate();
    console->SetPrivate(context, inspectedContextPrivateKey(isolate), v8::External::New(isolate, nullptr));
}

bool V8Debugger::isCommandLineAPIMethod(const String16& name)
{
    DEFINE_STATIC_LOCAL(protocol::HashSet<String16>, methods, ());
    if (methods.size() == 0) {
        const char* members[] = { "$", "$$", "$x", "dir", "dirxml", "keys", "values", "profile", "profileEnd",
            "monitorEvents", "unmonitorEvents", "inspect", "copy", "clear", "getEventListeners",
            "debug", "undebug", "monitor", "unmonitor", "table" };
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(members); ++i)
            methods.add(members[i]);
    }
    return methods.find(name) != methods.end();
}

bool V8Debugger::isCommandLineAPIGetter(const String16& name)
{
    DEFINE_STATIC_LOCAL(protocol::HashSet<String16>, getters, ());
    if (getters.size() == 0) {
        const char* members[] = { "$0", "$1", "$2", "$3", "$4", "$_" };
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(members); ++i)
            getters.add(members[i]);
    }
    return getters.find(name) != getters.end();
}

} // namespace blink

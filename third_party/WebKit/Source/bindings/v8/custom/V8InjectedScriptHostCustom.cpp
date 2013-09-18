/*
 * Copyright (C) 2007-2011 Google Inc. All rights reserved.
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

#include "config.h"
#include "V8InjectedScriptHost.h"

#include "V8Database.h"
#include "V8HTMLAllCollection.h"
#include "V8HTMLCollection.h"
#include "V8Node.h"
#include "V8NodeList.h"
#include "V8Storage.h"
#include "bindings/v8/BindingSecurity.h"
#include "bindings/v8/ScriptDebugServer.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8AbstractEventListener.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8HiddenPropertyName.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "bindings/v8/custom/V8Float32ArrayCustom.h"
#include "bindings/v8/custom/V8Float64ArrayCustom.h"
#include "bindings/v8/custom/V8Int16ArrayCustom.h"
#include "bindings/v8/custom/V8Int32ArrayCustom.h"
#include "bindings/v8/custom/V8Int8ArrayCustom.h"
#include "bindings/v8/custom/V8Uint16ArrayCustom.h"
#include "bindings/v8/custom/V8Uint32ArrayCustom.h"
#include "bindings/v8/custom/V8Uint8ArrayCustom.h"
#include "bindings/v8/custom/V8Uint8ClampedArrayCustom.h"
#include "core/inspector/InjectedScript.h"
#include "core/inspector/InjectedScriptHost.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/platform/JSONValues.h"
#include "modules/webdatabase/Database.h"

namespace WebCore {

Node* InjectedScriptHost::scriptValueAsNode(ScriptValue value)
{
    v8::HandleScope scope(value.isolate());
    if (!value.isObject() || value.isNull())
        return 0;
    return V8Node::toNative(v8::Handle<v8::Object>::Cast(value.v8Value()));
}

ScriptValue InjectedScriptHost::nodeAsScriptValue(ScriptState* state, Node* node)
{
    v8::Isolate* isolate = state->isolate();
    v8::HandleScope scope(isolate);
    v8::Local<v8::Context> context = state->context();
    v8::Context::Scope contextScope(context);

    if (!BindingSecurity::shouldAllowAccessToNode(node))
        return ScriptValue(v8::Null(isolate), isolate);
    return ScriptValue(toV8(node, v8::Handle<v8::Object>(), isolate), isolate);
}

void V8InjectedScriptHost::inspectedObjectMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() < 1)
        return;

    if (!args[0]->IsInt32()) {
        throwTypeError("argument has to be an integer", args.GetIsolate());
        return;
    }

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(args.Holder());
    InjectedScriptHost::InspectableObject* object = host->inspectedObject(args[0]->ToInt32()->Value());
    v8SetReturnValue(args, object->get(ScriptState::current()).v8Value());
}

void V8InjectedScriptHost::internalConstructorNameMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() < 1)
        return;

    if (!args[0]->IsObject())
        return;

    v8SetReturnValue(args, args[0]->ToObject()->GetConstructorName());
}

void V8InjectedScriptHost::isHTMLAllCollectionMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() < 1)
        return;

    if (!args[0]->IsObject()) {
        v8SetReturnValue(args, false);
        return;
    }

    v8SetReturnValue(args, V8HTMLAllCollection::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate())));
}

void V8InjectedScriptHost::typeMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() < 1)
        return;

    v8::Handle<v8::Value> value = args[0];
    if (value->IsString()) {
        v8SetReturnValue(args, v8::String::NewSymbol("string"));
        return;
    }
    if (value->IsArray()) {
        v8SetReturnValue(args, v8::String::NewSymbol("array"));
        return;
    }
    if (value->IsBoolean()) {
        v8SetReturnValue(args, v8::String::NewSymbol("boolean"));
        return;
    }
    if (value->IsNumber()) {
        v8SetReturnValue(args, v8::String::NewSymbol("number"));
        return;
    }
    if (value->IsDate()) {
        v8SetReturnValue(args, v8::String::NewSymbol("date"));
        return;
    }
    if (value->IsRegExp()) {
        v8SetReturnValue(args, v8::String::NewSymbol("regexp"));
        return;
    }
    WrapperWorldType currentWorldType = worldType(args.GetIsolate());
    if (V8Node::HasInstance(value, args.GetIsolate(), currentWorldType)) {
        v8SetReturnValue(args, v8::String::NewSymbol("node"));
        return;
    }
    if (V8NodeList::HasInstance(value, args.GetIsolate(), currentWorldType)) {
        v8SetReturnValue(args, v8::String::NewSymbol("array"));
        return;
    }
    if (V8HTMLCollection::HasInstance(value, args.GetIsolate(), currentWorldType)) {
        v8SetReturnValue(args, v8::String::NewSymbol("array"));
        return;
    }
    if (V8Int8Array::HasInstance(value, args.GetIsolate(), currentWorldType) || V8Int16Array::HasInstance(value, args.GetIsolate(), currentWorldType) || V8Int32Array::HasInstance(value, args.GetIsolate(), currentWorldType)) {
        v8SetReturnValue(args, v8::String::NewSymbol("array"));
        return;
    }
    if (V8Uint8Array::HasInstance(value, args.GetIsolate(), currentWorldType) || V8Uint16Array::HasInstance(value, args.GetIsolate(), currentWorldType) || V8Uint32Array::HasInstance(value, args.GetIsolate(), currentWorldType)) {
        v8SetReturnValue(args, v8::String::NewSymbol("array"));
        return;
    }
    if (V8Float32Array::HasInstance(value, args.GetIsolate(), currentWorldType) || V8Float64Array::HasInstance(value, args.GetIsolate(), currentWorldType)) {
        v8SetReturnValue(args, v8::String::NewSymbol("array"));
        return;
    }
    if (V8Uint8ClampedArray::HasInstance(value, args.GetIsolate(), currentWorldType)) {
        v8SetReturnValue(args, v8::String::NewSymbol("array"));
        return;
    }
}

void V8InjectedScriptHost::functionDetailsMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() < 1)
        return;

    v8::Handle<v8::Value> value = args[0];
    if (!value->IsFunction())
        return;
    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(value);
    int lineNumber = function->GetScriptLineNumber();
    int columnNumber = function->GetScriptColumnNumber();

    v8::Local<v8::Object> location = v8::Object::New();
    location->Set(v8::String::NewSymbol("lineNumber"), v8::Integer::New(lineNumber, args.GetIsolate()));
    location->Set(v8::String::NewSymbol("columnNumber"), v8::Integer::New(columnNumber, args.GetIsolate()));
    location->Set(v8::String::NewSymbol("scriptId"), function->GetScriptId()->ToString());

    v8::Local<v8::Object> result = v8::Object::New();
    result->Set(v8::String::NewSymbol("location"), location);

    v8::Handle<v8::Value> name = function->GetName();
    if (name->IsString() && v8::Handle<v8::String>::Cast(name)->Length())
        result->Set(v8::String::NewSymbol("name"), name);

    v8::Handle<v8::Value> inferredName = function->GetInferredName();
    if (inferredName->IsString() && v8::Handle<v8::String>::Cast(inferredName)->Length())
        result->Set(v8::String::NewSymbol("inferredName"), inferredName);

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(args.Holder());
    ScriptDebugServer& debugServer = host->scriptDebugServer();
    v8::Handle<v8::Value> scopes = debugServer.functionScopes(function);
    if (!scopes.IsEmpty() && scopes->IsArray())
        result->Set(v8::String::NewSymbol("rawScopes"), scopes);

    v8SetReturnValue(args, result);
}

void V8InjectedScriptHost::getInternalPropertiesMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() < 1)
        return;

    v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(args[0]);

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(args.Holder());
    ScriptDebugServer& debugServer = host->scriptDebugServer();
    v8SetReturnValue(args, debugServer.getInternalProperties(object));
}

static v8::Handle<v8::Array> getJSListenerFunctions(Document* document, const EventListenerInfo& listenerInfo)
{
    v8::Local<v8::Array> result = v8::Array::New();
    size_t handlersCount = listenerInfo.eventListenerVector.size();
    for (size_t i = 0, outputIndex = 0; i < handlersCount; ++i) {
        RefPtr<EventListener> listener = listenerInfo.eventListenerVector[i].listener;
        if (listener->type() != EventListener::JSEventListenerType) {
            ASSERT_NOT_REACHED();
            continue;
        }
        V8AbstractEventListener* v8Listener = static_cast<V8AbstractEventListener*>(listener.get());
        v8::Local<v8::Context> context = toV8Context(document, v8Listener->world());
        // Hide listeners from other contexts.
        if (context != v8::Context::GetCurrent())
            continue;
        v8::Local<v8::Object> function;
        {
            // getListenerObject() may cause JS in the event attribute to get compiled, potentially unsuccessfully.
            v8::TryCatch block;
            function = v8Listener->getListenerObject(document);
            if (block.HasCaught())
                continue;
        }
        ASSERT(!function.IsEmpty());
        v8::Local<v8::Object> listenerEntry = v8::Object::New();
        listenerEntry->Set(v8::String::NewSymbol("listener"), function);
        listenerEntry->Set(v8::String::NewSymbol("useCapture"), v8::Boolean::New(listenerInfo.eventListenerVector[i].useCapture));
        result->Set(v8::Number::New(v8Listener->isolate(), outputIndex++), listenerEntry);
    }
    return result;
}

void V8InjectedScriptHost::getEventListenersMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() < 1)
        return;

    v8::Local<v8::Value> value = args[0];
    if (!V8Node::HasInstance(value, args.GetIsolate(), worldType(args.GetIsolate())))
        return;
    Node* node = V8Node::toNative(value->ToObject());
    if (!node)
        return;

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(args.Holder());
    Vector<EventListenerInfo> listenersArray;
    host->getEventListenersImpl(node, listenersArray);

    v8::Local<v8::Object> result = v8::Object::New();
    for (size_t i = 0; i < listenersArray.size(); ++i) {
        v8::Handle<v8::Array> listeners = getJSListenerFunctions(&node->document(), listenersArray[i]);
        if (!listeners->Length())
            continue;
        AtomicString eventType = listenersArray[i].eventType;
        result->Set(v8String(eventType, args.GetIsolate()), listeners);
    }

    v8SetReturnValue(args, result);
}

void V8InjectedScriptHost::inspectMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() < 2)
        return;

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(args.Holder());
    ScriptValue object(args[0], args.GetIsolate());
    ScriptValue hints(args[1], args.GetIsolate());
    host->inspectImpl(object.toJSONValue(ScriptState::current()), hints.toJSONValue(ScriptState::current()));
}

void V8InjectedScriptHost::databaseIdMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() > 0 && V8Database::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate()))) {
        Database* database = V8Database::toNative(v8::Handle<v8::Object>::Cast(args[0]));
        if (database) {
            InjectedScriptHost* host = V8InjectedScriptHost::toNative(args.Holder()); {
                v8SetReturnValueStringOrUndefined(args, host->databaseIdImpl(database), args.GetIsolate());
                return;
            }
        }
    }
}

void V8InjectedScriptHost::storageIdMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() > 0 && V8Storage::HasInstance(args[0], args.GetIsolate(), worldType(args.GetIsolate()))) {
        Storage* storage = V8Storage::toNative(v8::Handle<v8::Object>::Cast(args[0]));
        if (storage) {
            InjectedScriptHost* host = V8InjectedScriptHost::toNative(args.Holder());
            v8SetReturnValueStringOrUndefined(args, host->storageIdImpl(storage), args.GetIsolate());
            return;
        }
    }
}

void V8InjectedScriptHost::evaluateMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (args.Length() < 1) {
        v8::ThrowException(v8::Exception::Error(v8::String::New("One argument expected.")));
        return;
    }

    v8::Handle<v8::String> expression = args[0]->ToString();
    if (expression.IsEmpty()) {
        v8::ThrowException(v8::Exception::Error(v8::String::New("The argument must be a string.")));
        return;
    }

    ASSERT(!v8::Context::GetCurrent().IsEmpty());
    v8::TryCatch tryCatch;
    v8::Handle<v8::Value> result = V8ScriptRunner::compileAndRunInternalScript(expression, args.GetIsolate());
    if (tryCatch.HasCaught()) {
        v8SetReturnValue(args, tryCatch.ReThrow());
        return;
    }
    v8SetReturnValue(args, result);
}

void V8InjectedScriptHost::setFunctionVariableValueMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Handle<v8::Value> functionValue = args[0];
    int scopeIndex = args[1]->Int32Value();
    String variableName = toWebCoreStringWithUndefinedOrNullCheck(args[2]);
    v8::Handle<v8::Value> newValue = args[3];

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(args.Holder());
    ScriptDebugServer& debugServer = host->scriptDebugServer();
    v8SetReturnValue(args, debugServer.setFunctionVariableValue(functionValue, scopeIndex, variableName, newValue));
}

static bool getFunctionLocation(const v8::FunctionCallbackInfo<v8::Value>& args, String* scriptId, int* lineNumber, int* columnNumber)
{
    if (args.Length() < 1)
        return false;
    v8::Handle<v8::Value> fn = args[0];
    if (!fn->IsFunction())
        return false;
    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(fn);
    *lineNumber = function->GetScriptLineNumber();
    *columnNumber = function->GetScriptColumnNumber();
    if (*lineNumber == v8::Function::kLineOffsetNotFound || *columnNumber == v8::Function::kLineOffsetNotFound)
        return false;
    *scriptId = toWebCoreStringWithUndefinedOrNullCheck(function->GetScriptId());
    return true;
}

void V8InjectedScriptHost::debugFunctionMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    String scriptId;
    int lineNumber;
    int columnNumber;
    if (!getFunctionLocation(args, &scriptId, &lineNumber, &columnNumber))
        return;

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(args.Holder());
    host->debugFunction(scriptId, lineNumber, columnNumber);
}

void V8InjectedScriptHost::undebugFunctionMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    String scriptId;
    int lineNumber;
    int columnNumber;
    if (!getFunctionLocation(args, &scriptId, &lineNumber, &columnNumber))
        return;

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(args.Holder());
    host->undebugFunction(scriptId, lineNumber, columnNumber);
}

void V8InjectedScriptHost::monitorFunctionMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    String scriptId;
    int lineNumber;
    int columnNumber;
    if (!getFunctionLocation(args, &scriptId, &lineNumber, &columnNumber))
        return;

    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(args[0]);
    v8::Handle<v8::Value> name;
    if (args.Length() > 0 && args[0]->IsFunction()) {
        v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(args[0]);
        name = function->GetName();
        if (!name->IsString() || !v8::Handle<v8::String>::Cast(name)->Length())
            name = function->GetInferredName();
    }

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(args.Holder());
    host->monitorFunction(scriptId, lineNumber, columnNumber, toWebCoreStringWithUndefinedOrNullCheck(name));
}

void V8InjectedScriptHost::unmonitorFunctionMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    String scriptId;
    int lineNumber;
    int columnNumber;
    if (!getFunctionLocation(args, &scriptId, &lineNumber, &columnNumber))
        return;

    InjectedScriptHost* host = V8InjectedScriptHost::toNative(args.Holder());
    host->unmonitorFunction(scriptId, lineNumber, columnNumber);
}

} // namespace WebCore

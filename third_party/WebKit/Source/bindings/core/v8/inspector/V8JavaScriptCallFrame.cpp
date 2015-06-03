// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/inspector/V8JavaScriptCallFrame.h"

#include "wtf/RefPtr.h"

namespace blink {

namespace {

void callerAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::unwrap(info.Holder());
    JavaScriptCallFrame* caller = impl->caller();
    if (!caller)
        return;
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::FunctionTemplate> wrapperTemplate = impl->wrapperTemplate(isolate);
    if (wrapperTemplate.IsEmpty())
        return;
    info.GetReturnValue().Set(V8JavaScriptCallFrame::wrap(wrapperTemplate, isolate->GetCurrentContext(), caller));
}

void sourceIDAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::unwrap(info.Holder());
    info.GetReturnValue().Set(impl->sourceID());
}

void lineAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::unwrap(info.Holder());
    info.GetReturnValue().Set(impl->line());
}

void columnAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::unwrap(info.Holder());
    info.GetReturnValue().Set(impl->column());
}

void scopeChainAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::unwrap(info.Holder());
    info.GetReturnValue().Set(impl->scopeChain());
}

void thisObjectAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::unwrap(info.Holder());
    info.GetReturnValue().Set(impl->thisObject());
}

void stepInPositionsAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::unwrap(info.Holder());
    CString cstr = impl->stepInPositions().utf8();
    v8::Local<v8::Name> result = v8::String::NewFromUtf8(info.GetIsolate(), cstr.data(), v8::NewStringType::kNormal, cstr.length()).ToLocalChecked();
    info.GetReturnValue().Set(result);
}

void functionNameAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::unwrap(info.Holder());
    CString cstr = impl->functionName().utf8();
    v8::Local<v8::Name> result = v8::String::NewFromUtf8(info.GetIsolate(), cstr.data(), v8::NewStringType::kNormal, cstr.length()).ToLocalChecked();
    info.GetReturnValue().Set(result);
}

void functionLineAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::unwrap(info.Holder());
    info.GetReturnValue().Set(impl->functionLine());
}

void functionColumnAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::unwrap(info.Holder());
    info.GetReturnValue().Set(impl->functionColumn());
}

void isAtReturnAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::unwrap(info.Holder());
    info.GetReturnValue().Set(impl->isAtReturn());
}

void returnValueAttributeGetterCallback(v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::unwrap(info.Holder());
    info.GetReturnValue().Set(impl->returnValue());
}

void evaluateWithExceptionDetailsMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::unwrap(info.Holder());
    info.GetReturnValue().Set(impl->evaluateWithExceptionDetails(info[0], info[1]));
}

void restartMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::unwrap(info.Holder());
    v8::MaybeLocal<v8::Value> result = impl->restart();
    v8::Local<v8::Value> value;
    if (result.ToLocal(&value))
        info.GetReturnValue().Set(value);
}

void setVariableValueMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::unwrap(info.Holder());
    v8::Maybe<int32_t> maybeScopeIndex = info[0]->Int32Value(info.GetIsolate()->GetCurrentContext());
    if (maybeScopeIndex.IsNothing())
        return;
    int scopeIndex = maybeScopeIndex.FromJust();
    v8::MaybeLocal<v8::Value> result = impl->setVariableValue(scopeIndex, info[1], info[2]);
    v8::Local<v8::Value> value;
    if (result.ToLocal(&value))
        info.GetReturnValue().Set(value);

}

void scopeTypeMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    JavaScriptCallFrame* impl = V8JavaScriptCallFrame::unwrap(info.Holder());
    v8::Maybe<int32_t> maybeScopeIndex = info[0]->Int32Value(info.GetIsolate()->GetCurrentContext());
    if (maybeScopeIndex.IsNothing())
        return;
    int scopeIndex = maybeScopeIndex.FromJust();
    info.GetReturnValue().Set(impl->scopeType(scopeIndex));
}

struct V8AttributeConfiguration {
    const char* name;
    v8::AccessorNameGetterCallback callback;
};

const V8AttributeConfiguration V8JavaScriptCallFrameAttributes[] = {
    {"scopeChain", scopeChainAttributeGetterCallback},
    {"thisObject", thisObjectAttributeGetterCallback},
    {"returnValue", returnValueAttributeGetterCallback},

    {"caller", callerAttributeGetterCallback},
    {"sourceID", sourceIDAttributeGetterCallback},
    {"line", lineAttributeGetterCallback},
    {"column", columnAttributeGetterCallback},
    {"stepInPositions", stepInPositionsAttributeGetterCallback},
    {"functionName", functionNameAttributeGetterCallback},
    {"functionLine", functionLineAttributeGetterCallback},
    {"functionColumn", functionColumnAttributeGetterCallback},
    {"isAtReturn", isAtReturnAttributeGetterCallback},
};

struct V8MethodConfiguration {
    const char* name;
    v8::FunctionCallback callback;
};

const V8MethodConfiguration V8JavaScriptCallFrameMethods[] = {
    {"evaluateWithExceptionDetails", evaluateWithExceptionDetailsMethodCallback},
    {"restart", restartMethodCallback},
    {"setVariableValue", setVariableValueMethodCallback},
    {"scopeType", scopeTypeMethodCallback},
};

class WeakCallbackData final {
public:
    WeakCallbackData(v8::Isolate* isolate, PassRefPtr<JavaScriptCallFrame> frame, v8::Local<v8::Object> wrapper)
        : m_frame(frame)
        , m_persistent(isolate, wrapper)
    {
        m_persistent.SetWeak(this, &WeakCallbackData::weakCallback, v8::WeakCallbackType::kParameter);
    }

    RefPtr<JavaScriptCallFrame> m_frame;

private:
    static void weakCallback(const v8::WeakCallbackInfo<WeakCallbackData>& info)
    {
        delete info.GetParameter();
    }

    v8::Global<v8::Object> m_persistent;
};

} // namespace

v8::Local<v8::FunctionTemplate> V8JavaScriptCallFrame::createWrapperTemplate(v8::Isolate* isolate)
{
    v8::Local<v8::FunctionTemplate> functionTemplate = v8::FunctionTemplate::New(isolate);

    functionTemplate->SetClassName(v8::String::NewFromUtf8(isolate, "JavaScriptCallFrame", v8::NewStringType::kInternalized).ToLocalChecked());
    v8::Local<v8::ObjectTemplate> instanceTemplate = functionTemplate->InstanceTemplate();
    for (auto& config : V8JavaScriptCallFrameAttributes) {
        v8::Local<v8::Name> v8name = v8::String::NewFromUtf8(isolate, config.name, v8::NewStringType::kInternalized).ToLocalChecked();
        instanceTemplate->SetAccessor(v8name, config.callback);
    }

    for (auto& config : V8JavaScriptCallFrameMethods) {
        v8::Local<v8::Name> v8name = v8::String::NewFromUtf8(isolate, config.name, v8::NewStringType::kInternalized).ToLocalChecked();
        v8::Local<v8::FunctionTemplate> functionTemplate = v8::FunctionTemplate::New(isolate, config.callback);
        functionTemplate->RemovePrototype();
        instanceTemplate->Set(v8name, functionTemplate);
    }

    return functionTemplate;
}

v8::Local<v8::Object> V8JavaScriptCallFrame::wrap(v8::Local<v8::FunctionTemplate> constructorTemplate, v8::Local<v8::Context> context, PassRefPtr<JavaScriptCallFrame> frame)
{
    RefPtr<JavaScriptCallFrame> impl(frame);
    v8::Local<v8::Function> function;
    if (!constructorTemplate->GetFunction(context).ToLocal(&function))
        return v8::Local<v8::Object>();

    v8::MaybeLocal<v8::Object> maybeResult = function->NewInstance(context);
    v8::Local<v8::Object> result;
    if (!maybeResult.ToLocal(&result))
        return v8::Local<v8::Object>();

    v8::Isolate* isolate = context->GetIsolate();
    v8::Local<v8::External> objectReference = v8::External::New(isolate, new WeakCallbackData(isolate, impl, result));
    result->SetHiddenValue(hiddenPropertyName(isolate), objectReference);

    // Store template for .caller callback
    impl->setWrapperTemplate(constructorTemplate, isolate);

    return result;
}

JavaScriptCallFrame* V8JavaScriptCallFrame::unwrap(v8::Local<v8::Object> object)
{
    v8::Isolate* isolate = object->GetIsolate();
    v8::Local<v8::Value> value = object->GetHiddenValue(hiddenPropertyName(isolate));
    if (value.IsEmpty())
        return nullptr;
    if (!value->IsExternal())
        return nullptr;
    void* data = value.As<v8::External>()->Value();
    return reinterpret_cast<WeakCallbackData*>(data)->m_frame.get();
}

v8::Local<v8::String> V8JavaScriptCallFrame::hiddenPropertyName(v8::Isolate* isolate)
{
    return v8::String::NewFromUtf8(isolate, "v8inspector::JavaScriptCallFrame", v8::NewStringType::kInternalized).ToLocalChecked();
}

} // namespace blink

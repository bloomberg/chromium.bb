/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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
#include "bindings/v8/V8Binding.h"

#include "V8Element.h"
#include "V8NodeFilter.h"
#include "V8Window.h"
#include "V8WorkerGlobalScope.h"
#include "V8XPathNSResolver.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8AbstractEventListener.h"
#include "bindings/v8/V8BindingMacros.h"
#include "bindings/v8/V8NodeFilterCondition.h"
#include "bindings/v8/V8ObjectConstructor.h"
#include "bindings/v8/V8WindowShell.h"
#include "bindings/v8/WorkerScriptController.h"
#include "bindings/v8/custom/V8CustomXPathNSResolver.h"
#include "core/dom/Element.h"
#include "core/dom/NodeFilter.h"
#include "core/dom/QualifiedName.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/inspector/BindingVisitors.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/xml/XPathNSResolver.h"
#include "wtf/ArrayBufferContents.h"
#include "wtf/MainThread.h"
#include "wtf/MathExtras.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Threading.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringBuffer.h"
#include "wtf/text/StringHash.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

v8::Handle<v8::Value> throwError(V8ErrorType errorType, const String& message, v8::Isolate* isolate)
{
    return V8ThrowException::throwError(errorType, message, isolate);
}

v8::Handle<v8::Value> throwError(v8::Handle<v8::Value> exception, v8::Isolate* isolate)
{
    return V8ThrowException::throwError(exception, isolate);
}

v8::Handle<v8::Value> throwTypeError(const String& message, v8::Isolate* isolate)
{
    return V8ThrowException::throwTypeError(message, isolate);
}

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
    virtual void* Allocate(size_t size) OVERRIDE
    {
        void* data;
        WTF::ArrayBufferContents::allocateMemory(size, WTF::ArrayBufferContents::ZeroInitialize, data);
        return data;
    }

    virtual void* AllocateUninitialized(size_t size) OVERRIDE
    {
        void* data;
        WTF::ArrayBufferContents::allocateMemory(size, WTF::ArrayBufferContents::DontInitialize, data);
        return data;
    }

    virtual void Free(void* data, size_t size) OVERRIDE
    {
        WTF::ArrayBufferContents::freeMemory(data, size);
    }
};

v8::ArrayBuffer::Allocator* v8ArrayBufferAllocator()
{
    DEFINE_STATIC_LOCAL(ArrayBufferAllocator, arrayBufferAllocator, ());
    return &arrayBufferAllocator;
}

PassRefPtr<NodeFilter> toNodeFilter(v8::Handle<v8::Value> callback, v8::Isolate* isolate)
{
    RefPtr<NodeFilter> filter = NodeFilter::create();

    // FIXME: Should pass in appropriate creationContext
    v8::Handle<v8::Object> filterWrapper = toV8(filter, v8::Handle<v8::Object>(), isolate).As<v8::Object>();

    RefPtr<NodeFilterCondition> condition = V8NodeFilterCondition::create(callback, filterWrapper, isolate);
    filter->setCondition(condition.release());

    return filter.release();
}

const int32_t kMaxInt32 = 0x7fffffff;
const int32_t kMinInt32 = -kMaxInt32 - 1;
const uint32_t kMaxUInt32 = 0xffffffff;
const int64_t kJSMaxInteger = 0x20000000000000LL - 1; // 2^53 - 1, maximum uniquely representable integer in ECMAScript.

static double enforceRange(double x, double minimum, double maximum, const char* typeName, ExceptionState& exceptionState)
{
    if (std::isnan(x) || std::isinf(x)) {
        exceptionState.throwTypeError("Value is" + String(std::isinf(x) ? " infinite and" : "") + " not of type '" + String(typeName) + "'.");
        return 0;
    }
    x = trunc(x);
    if (x < minimum || x > maximum) {
        exceptionState.throwTypeError("Value is outside the '" + String(typeName) + "' value range.");
        return 0;
    }
    return x;
}

template <typename T>
struct IntTypeLimits {
};

template <>
struct IntTypeLimits<int8_t> {
    static const int8_t minValue = -128;
    static const int8_t maxValue = 127;
    static const unsigned numberOfValues = 256; // 2^8
};

template <>
struct IntTypeLimits<uint8_t> {
    static const uint8_t maxValue = 255;
    static const unsigned numberOfValues = 256; // 2^8
};

template <>
struct IntTypeLimits<int16_t> {
    static const short minValue = -32768;
    static const short maxValue = 32767;
    static const unsigned numberOfValues = 65536; // 2^16
};

template <>
struct IntTypeLimits<uint16_t> {
    static const unsigned short maxValue = 65535;
    static const unsigned numberOfValues = 65536; // 2^16
};

template <typename T>
static inline T toSmallerInt(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, const char* typeName, ExceptionState& exceptionState)
{
    typedef IntTypeLimits<T> LimitsTrait;

    // Fast case. The value is already a 32-bit integer in the right range.
    if (value->IsInt32()) {
        int32_t result = value->Int32Value();
        if (result >= LimitsTrait::minValue && result <= LimitsTrait::maxValue)
            return static_cast<T>(result);
        if (configuration == EnforceRange) {
            exceptionState.throwTypeError("Value is outside the '" + String(typeName) + "' value range.");
            return 0;
        }
        result %= LimitsTrait::numberOfValues;
        return static_cast<T>(result > LimitsTrait::maxValue ? result - LimitsTrait::numberOfValues : result);
    }

    // Can the value be converted to a number?
    V8TRYCATCH_EXCEPTION_RETURN(v8::Local<v8::Number>, numberObject, value->ToNumber(), exceptionState, 0);
    if (numberObject.IsEmpty()) {
        exceptionState.throwTypeError("Not convertible to a number value (of type '" + String(typeName) + "'.");
        return 0;
    }

    if (configuration == EnforceRange)
        return enforceRange(numberObject->Value(), LimitsTrait::minValue, LimitsTrait::maxValue, typeName, exceptionState);

    double numberValue = numberObject->Value();
    if (std::isnan(numberValue) || std::isinf(numberValue) || !numberValue)
        return 0;

    numberValue = numberValue < 0 ? -floor(fabs(numberValue)) : floor(fabs(numberValue));
    numberValue = fmod(numberValue, LimitsTrait::numberOfValues);

    return static_cast<T>(numberValue > LimitsTrait::maxValue ? numberValue - LimitsTrait::numberOfValues : numberValue);
}

template <typename T>
static inline T toSmallerUInt(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, const char* typeName, ExceptionState& exceptionState)
{
    typedef IntTypeLimits<T> LimitsTrait;

    // Fast case. The value is a 32-bit signed integer - possibly positive?
    if (value->IsInt32()) {
        int32_t result = value->Int32Value();
        if (result >= 0 && result <= LimitsTrait::maxValue)
            return static_cast<T>(result);
        if (configuration == EnforceRange) {
            exceptionState.throwTypeError("Value is outside the '" + String(typeName) + "' value range.");
            return 0;
        }
        return static_cast<T>(result);
    }

    // Can the value be converted to a number?
    V8TRYCATCH_EXCEPTION_RETURN(v8::Local<v8::Number>, numberObject, value->ToNumber(), exceptionState, 0);
    if (numberObject.IsEmpty()) {
        exceptionState.throwTypeError("Not convertible to a number value (of type '" + String(typeName) + "'.");
        return 0;
    }

    if (configuration == EnforceRange)
        return enforceRange(numberObject->Value(), 0, LimitsTrait::maxValue, typeName, exceptionState);

    // Does the value convert to nan or to an infinity?
    double numberValue = numberObject->Value();
    if (std::isnan(numberValue) || std::isinf(numberValue) || !numberValue)
        return 0;

    if (configuration == Clamp)
        return clampTo<T>(numberObject->Value());

    numberValue = numberValue < 0 ? -floor(fabs(numberValue)) : floor(fabs(numberValue));
    return static_cast<T>(fmod(numberValue, LimitsTrait::numberOfValues));
}

int8_t toInt8(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
{
    return toSmallerInt<int8_t>(value, configuration, "byte", exceptionState);
}

int8_t toInt8(v8::Handle<v8::Value> value)
{
    NonThrowableExceptionState exceptionState;
    return toInt8(value, NormalConversion, exceptionState);
}

uint8_t toUInt8(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
{
    return toSmallerUInt<uint8_t>(value, configuration, "octet", exceptionState);
}

uint8_t toUInt8(v8::Handle<v8::Value> value)
{
    NonThrowableExceptionState exceptionState;
    return toUInt8(value, NormalConversion, exceptionState);
}

int16_t toInt16(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
{
    return toSmallerInt<int16_t>(value, configuration, "short", exceptionState);
}

int16_t toInt16(v8::Handle<v8::Value> value)
{
    NonThrowableExceptionState exceptionState;
    return toInt16(value, NormalConversion, exceptionState);
}

uint16_t toUInt16(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
{
    return toSmallerUInt<uint16_t>(value, configuration, "unsigned short", exceptionState);
}

uint16_t toUInt16(v8::Handle<v8::Value> value)
{
    NonThrowableExceptionState exceptionState;
    return toUInt16(value, NormalConversion, exceptionState);
}

int32_t toInt32(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
{
    // Fast case. The value is already a 32-bit integer.
    if (value->IsInt32())
        return value->Int32Value();

    // Can the value be converted to a number?
    V8TRYCATCH_EXCEPTION_RETURN(v8::Local<v8::Number>, numberObject, value->ToNumber(), exceptionState, 0);
    if (numberObject.IsEmpty()) {
        exceptionState.throwTypeError("Not convertible to a number value (of type 'long'.)");
        return 0;
    }

    if (configuration == EnforceRange)
        return enforceRange(numberObject->Value(), kMinInt32, kMaxInt32, "long", exceptionState);

    // Does the value convert to nan or to an infinity?
    double numberValue = numberObject->Value();
    if (std::isnan(numberValue) || std::isinf(numberValue))
        return 0;

    if (configuration == Clamp)
        return clampTo<int32_t>(numberObject->Value());

    V8TRYCATCH_EXCEPTION_RETURN(int32_t, result, numberObject->Int32Value(), exceptionState, 0);
    return result;
}

int32_t toInt32(v8::Handle<v8::Value> value)
{
    NonThrowableExceptionState exceptionState;
    return toInt32(value, NormalConversion, exceptionState);
}

uint32_t toUInt32(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
{
    // Fast case. The value is already a 32-bit unsigned integer.
    if (value->IsUint32())
        return value->Uint32Value();

    // Fast case. The value is a 32-bit signed integer - possibly positive?
    if (value->IsInt32()) {
        int32_t result = value->Int32Value();
        if (result >= 0)
            return result;
        if (configuration == EnforceRange) {
            exceptionState.throwTypeError("Value is outside the 'unsigned long' value range.");
            return 0;
        }
        return result;
    }

    // Can the value be converted to a number?
    V8TRYCATCH_EXCEPTION_RETURN(v8::Local<v8::Number>, numberObject, value->ToNumber(), exceptionState, 0);
    if (numberObject.IsEmpty()) {
        exceptionState.throwTypeError("Not convertible to a number value (of type 'unsigned long'.)");
        return 0;
    }

    if (configuration == EnforceRange)
        return enforceRange(numberObject->Value(), 0, kMaxUInt32, "unsigned long", exceptionState);

    // Does the value convert to nan or to an infinity?
    double numberValue = numberObject->Value();
    if (std::isnan(numberValue) || std::isinf(numberValue))
        return 0;

    if (configuration == Clamp)
        return clampTo<uint32_t>(numberObject->Value());

    V8TRYCATCH_RETURN(uint32_t, result, numberObject->Uint32Value(), 0);
    return result;
}

uint32_t toUInt32(v8::Handle<v8::Value> value)
{
    NonThrowableExceptionState exceptionState;
    return toUInt32(value, NormalConversion, exceptionState);
}

int64_t toInt64(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
{
    // Fast case. The value is a 32-bit integer.
    if (value->IsInt32())
        return value->Int32Value();

    // Can the value be converted to a number?
    V8TRYCATCH_EXCEPTION_RETURN(v8::Local<v8::Number>, numberObject, value->ToNumber(), exceptionState, 0);
    if (numberObject.IsEmpty()) {
        exceptionState.throwTypeError("Not convertible to a number value (of type 'long long'.)");
        return 0;
    }

    double x = numberObject->Value();

    if (configuration == EnforceRange)
        return enforceRange(x, -kJSMaxInteger, kJSMaxInteger, "long long", exceptionState);

    // Does the value convert to nan or to an infinity?
    if (std::isnan(x) || std::isinf(x))
        return 0;

    // NaNs and +/-Infinity should be 0, otherwise modulo 2^64.
    unsigned long long integer;
    doubleToInteger(x, integer);
    return integer;
}

int64_t toInt64(v8::Handle<v8::Value> value)
{
    NonThrowableExceptionState exceptionState;
    return toInt64(value, NormalConversion, exceptionState);
}

uint64_t toUInt64(v8::Handle<v8::Value> value, IntegerConversionConfiguration configuration, ExceptionState& exceptionState)
{
    // Fast case. The value is a 32-bit unsigned integer.
    if (value->IsUint32())
        return value->Uint32Value();

    // Fast case. The value is a 32-bit integer.
    if (value->IsInt32()) {
        int32_t result = value->Int32Value();
        if (result >= 0)
            return result;
        if (configuration == EnforceRange) {
            exceptionState.throwTypeError("Value is outside the 'unsigned long long' value range.");
            return 0;
        }
        return result;
    }

    // Can the value be converted to a number?
    V8TRYCATCH_EXCEPTION_RETURN(v8::Local<v8::Number>, numberObject, value->ToNumber(), exceptionState, 0);
    if (numberObject.IsEmpty()) {
        exceptionState.throwTypeError("Not convertible to a number value (of type 'unsigned long long'.)");
        return 0;
    }

    double x = numberObject->Value();

    if (configuration == EnforceRange)
        return enforceRange(x, 0, kJSMaxInteger, "unsigned long long", exceptionState);

    // Does the value convert to nan or to an infinity?
    if (std::isnan(x) || std::isinf(x))
        return 0;

    // NaNs and +/-Infinity should be 0, otherwise modulo 2^64.
    unsigned long long integer;
    doubleToInteger(x, integer);
    return integer;
}

uint64_t toUInt64(v8::Handle<v8::Value> value)
{
    NonThrowableExceptionState exceptionState;
    return toUInt64(value, NormalConversion, exceptionState);
}

float toFloat(v8::Handle<v8::Value> value, ExceptionState& exceptionState)
{
    V8TRYCATCH_EXCEPTION_RETURN(v8::Local<v8::Number>, numberObject, value->ToNumber(), exceptionState, 0);
    return numberObject->NumberValue();
}

PassRefPtrWillBeRawPtr<XPathNSResolver> toXPathNSResolver(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    RefPtrWillBeRawPtr<XPathNSResolver> resolver = nullptr;
    if (V8XPathNSResolver::hasInstance(value, isolate))
        resolver = V8XPathNSResolver::toNative(v8::Handle<v8::Object>::Cast(value));
    else if (value->IsObject())
        resolver = V8CustomXPathNSResolver::create(value->ToObject(), isolate);
    return resolver;
}

DOMWindow* toDOMWindow(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    if (value.IsEmpty() || !value->IsObject())
        return 0;

    v8::Handle<v8::Object> windowWrapper = V8Window::findInstanceInPrototypeChain(v8::Handle<v8::Object>::Cast(value), isolate);
    if (!windowWrapper.IsEmpty())
        return V8Window::toNative(windowWrapper);
    return 0;
}

DOMWindow* toDOMWindow(v8::Handle<v8::Context> context)
{
    if (context.IsEmpty())
        return 0;
    return toDOMWindow(context->Global(), context->GetIsolate());
}

DOMWindow* enteredDOMWindow(v8::Isolate* isolate)
{
    return toDOMWindow(isolate->GetEnteredContext());
}

DOMWindow* currentDOMWindow(v8::Isolate* isolate)
{
    return toDOMWindow(isolate->GetCurrentContext());
}

DOMWindow* callingDOMWindow(v8::Isolate* isolate)
{
    v8::Handle<v8::Context> context = isolate->GetCallingContext();
    if (context.IsEmpty()) {
        // Unfortunately, when processing script from a plug-in, we might not
        // have a calling context. In those cases, we fall back to the
        // entered context.
        context = isolate->GetEnteredContext();
    }
    return toDOMWindow(context);
}

ExecutionContext* toExecutionContext(v8::Handle<v8::Context> context)
{
    v8::Handle<v8::Object> global = context->Global();
    v8::Handle<v8::Object> windowWrapper = V8Window::findInstanceInPrototypeChain(global, context->GetIsolate());
    if (!windowWrapper.IsEmpty())
        return V8Window::toNative(windowWrapper)->executionContext();
    v8::Handle<v8::Object> workerWrapper = V8WorkerGlobalScope::findInstanceInPrototypeChain(global, context->GetIsolate());
    if (!workerWrapper.IsEmpty())
        return V8WorkerGlobalScope::toNative(workerWrapper)->executionContext();
    // FIXME: Is this line of code reachable?
    return 0;
}

ExecutionContext* currentExecutionContext(v8::Isolate* isolate)
{
    return toExecutionContext(isolate->GetCurrentContext());
}

ExecutionContext* callingExecutionContext(v8::Isolate* isolate)
{
    v8::Handle<v8::Context> context = isolate->GetCallingContext();
    if (context.IsEmpty()) {
        // Unfortunately, when processing script from a plug-in, we might not
        // have a calling context. In those cases, we fall back to the
        // entered context.
        context = isolate->GetEnteredContext();
    }
    return toExecutionContext(context);
}

LocalFrame* toFrameIfNotDetached(v8::Handle<v8::Context> context)
{
    DOMWindow* window = toDOMWindow(context);
    if (window && window->isCurrentlyDisplayedInFrame())
        return window->frame();
    // We return 0 here because |context| is detached from the LocalFrame. If we
    // did return |frame| we could get in trouble because the frame could be
    // navigated to another security origin.
    return 0;
}

v8::Local<v8::Context> toV8Context(ExecutionContext* context, DOMWrapperWorld* world)
{
    ASSERT(context);
    if (context->isDocument()) {
        if (LocalFrame* frame = toDocument(context)->frame())
            return frame->script().windowShell(world)->context();
    } else if (context->isWorkerGlobalScope()) {
        if (WorkerScriptController* script = toWorkerGlobalScope(context)->script())
            return script->context();
    }
    return v8::Local<v8::Context>();
}

v8::Local<v8::Context> toV8Context(v8::Isolate* isolate, LocalFrame* frame, DOMWrapperWorld* world)
{
    if (!frame)
        return v8::Local<v8::Context>();
    v8::Local<v8::Context> context = frame->script().windowShell(world)->context();
    if (context.IsEmpty())
        return v8::Local<v8::Context>();
    LocalFrame* attachedFrame= toFrameIfNotDetached(context);
    return frame == attachedFrame ? context : v8::Local<v8::Context>();
}

v8::Local<v8::Value> handleMaxRecursionDepthExceeded(v8::Isolate* isolate)
{
    throwError(v8RangeError, "Maximum call stack size exceeded.", isolate);
    return v8::Local<v8::Value>();
}

void crashIfV8IsDead()
{
    if (v8::V8::IsDead()) {
        // FIXME: We temporarily deal with V8 internal error situations
        // such as out-of-memory by crashing the renderer.
        CRASH();
    }
}

v8::Handle<v8::Function> getBoundFunction(v8::Handle<v8::Function> function)
{
    v8::Handle<v8::Value> boundFunction = function->GetBoundFunction();
    return boundFunction->IsFunction() ? v8::Handle<v8::Function>::Cast(boundFunction) : function;
}

void addHiddenValueToArray(v8::Handle<v8::Object> object, v8::Local<v8::Value> value, int arrayIndex, v8::Isolate* isolate)
{
    v8::Local<v8::Value> arrayValue = object->GetInternalField(arrayIndex);
    if (arrayValue->IsNull() || arrayValue->IsUndefined()) {
        arrayValue = v8::Array::New(isolate);
        object->SetInternalField(arrayIndex, arrayValue);
    }

    v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(arrayValue);
    array->Set(v8::Integer::New(isolate, array->Length()), value);
}

void removeHiddenValueFromArray(v8::Handle<v8::Object> object, v8::Local<v8::Value> value, int arrayIndex, v8::Isolate* isolate)
{
    v8::Local<v8::Value> arrayValue = object->GetInternalField(arrayIndex);
    if (!arrayValue->IsArray())
        return;
    v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(arrayValue);
    for (int i = array->Length() - 1; i >= 0; --i) {
        v8::Local<v8::Value> item = array->Get(v8::Integer::New(isolate, i));
        if (item->StrictEquals(value)) {
            array->Delete(i);
            return;
        }
    }
}

void moveEventListenerToNewWrapper(v8::Handle<v8::Object> object, EventListener* oldValue, v8::Local<v8::Value> newValue, int arrayIndex, v8::Isolate* isolate)
{
    if (oldValue) {
        V8AbstractEventListener* oldListener = V8AbstractEventListener::cast(oldValue);
        if (oldListener) {
            v8::Local<v8::Object> oldListenerObject = oldListener->getExistingListenerObject();
            if (!oldListenerObject.IsEmpty())
                removeHiddenValueFromArray(object, oldListenerObject, arrayIndex, isolate);
        }
    }
    // Non-callable input is treated as null and ignored
    if (newValue->IsFunction())
        addHiddenValueToArray(object, newValue, arrayIndex, isolate);
}

v8::Isolate* toIsolate(ExecutionContext* context)
{
    if (context && context->isDocument())
        return V8PerIsolateData::mainThreadIsolate();
    return v8::Isolate::GetCurrent();
}

v8::Isolate* toIsolate(LocalFrame* frame)
{
    ASSERT(frame);
    return frame->script().isolate();
}

PassOwnPtr<V8ExecutionScope> V8ExecutionScope::create(v8::Isolate* isolate)
{
    return adoptPtr(new V8ExecutionScope(isolate));
}

V8ExecutionScope::V8ExecutionScope(v8::Isolate* isolate)
    : m_handleScope(isolate)
    , m_context(v8::Context::New(isolate))
    , m_contextScope(m_context)
    , m_world(DOMWrapperWorld::create())
    , m_perContextData(V8PerContextData::create(m_context, m_world))
{
}

} // namespace WebCore

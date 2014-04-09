/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "bindings/v8/IDBBindingUtilities.h"

#include "RuntimeEnabledFeatures.h"
#include "V8DOMStringList.h"
#include "V8IDBCursor.h"
#include "V8IDBCursorWithValue.h"
#include "V8IDBDatabase.h"
#include "V8IDBIndex.h"
#include "V8IDBKeyRange.h"
#include "V8IDBObjectStore.h"
#include "V8IDBRequest.h"
#include "V8IDBTransaction.h"
#include "bindings/v8/DOMRequestState.h"
#include "bindings/v8/SerializedScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8HiddenValue.h"
#include "bindings/v8/custom/V8ArrayBufferViewCustom.h"
#include "bindings/v8/custom/V8Uint8ArrayCustom.h"
#include "modules/indexeddb/IDBKey.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBKeyRange.h"
#include "modules/indexeddb/IDBTracing.h"
#include "platform/SharedBuffer.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/MathExtras.h"
#include "wtf/Uint8Array.h"
#include "wtf/Vector.h"

namespace WebCore {

static v8::Handle<v8::Value> deserializeIDBValueBuffer(SharedBuffer*, v8::Isolate*);

static v8::Handle<v8::Value> toV8(const IDBKeyPath& value, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    switch (value.type()) {
    case IDBKeyPath::NullType:
        return v8::Null(isolate);
    case IDBKeyPath::StringType:
        return v8String(isolate, value.string());
    case IDBKeyPath::ArrayType:
        RefPtr<DOMStringList> keyPaths = DOMStringList::create();
        for (Vector<String>::const_iterator it = value.array().begin(); it != value.array().end(); ++it)
            keyPaths->append(*it);
        return toV8(keyPaths.release(), creationContext, isolate);
    }
    ASSERT_NOT_REACHED();
    return v8::Undefined(isolate);
}

static v8::Handle<v8::Value> toV8(const IDBKey* key, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (!key) {
        // This should be undefined, not null.
        // Spec: http://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html#idl-def-IDBKeyRange
        return v8Undefined();
    }

    switch (key->type()) {
    case IDBKey::InvalidType:
    case IDBKey::MinType:
        ASSERT_NOT_REACHED();
        return v8Undefined();
    case IDBKey::NumberType:
        return v8::Number::New(isolate, key->number());
    case IDBKey::StringType:
        return v8String(isolate, key->string());
    case IDBKey::BinaryType:
        return toV8(Uint8Array::create(reinterpret_cast<const unsigned char*>(key->binary()->data()), key->binary()->size()), creationContext, isolate);
    case IDBKey::DateType:
        return v8::Date::New(isolate, key->date());
    case IDBKey::ArrayType:
        {
            v8::Local<v8::Array> array = v8::Array::New(isolate, key->array().size());
            for (size_t i = 0; i < key->array().size(); ++i)
                array->Set(i, toV8(key->array()[i].get(), creationContext, isolate));
            return array;
        }
    }

    ASSERT_NOT_REACHED();
    return v8Undefined();
}

static v8::Handle<v8::Value> toV8(const IDBAny* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (!impl)
        return v8::Null(isolate);

    switch (impl->type()) {
    case IDBAny::UndefinedType:
        return v8::Undefined(isolate);
    case IDBAny::NullType:
        return v8::Null(isolate);
    case IDBAny::DOMStringListType:
        return toV8(impl->domStringList(), creationContext, isolate);
    case IDBAny::IDBCursorType: {
        // Ensure request wrapper is kept alive at least as long as the cursor wrapper,
        // so that event listeners are retained.
        v8::Handle<v8::Value> cursor = toV8(impl->idbCursor(), creationContext, isolate);
        v8::Handle<v8::Value> request = toV8(impl->idbCursor()->request(), creationContext, isolate);
        V8HiddenValue::setHiddenValue(isolate, cursor->ToObject(), V8HiddenValue::idbCursorRequest(isolate), request);
        return cursor;
    }
    case IDBAny::IDBCursorWithValueType: {
        // Ensure request wrapper is kept alive at least as long as the cursor wrapper,
        // so that event listeners are retained.
        v8::Handle<v8::Value> cursor = toV8(impl->idbCursorWithValue(), creationContext, isolate);
        v8::Handle<v8::Value> request = toV8(impl->idbCursorWithValue()->request(), creationContext, isolate);
        V8HiddenValue::setHiddenValue(isolate, cursor->ToObject(), V8HiddenValue::idbCursorRequest(isolate), request);
        return cursor;
    }
    case IDBAny::IDBDatabaseType:
        return toV8(impl->idbDatabase(), creationContext, isolate);
    case IDBAny::IDBIndexType:
        return toV8(impl->idbIndex(), creationContext, isolate);
    case IDBAny::IDBObjectStoreType:
        return toV8(impl->idbObjectStore(), creationContext, isolate);
    case IDBAny::IDBTransactionType:
        return toV8(impl->idbTransaction(), creationContext, isolate);
    case IDBAny::BufferType:
        return deserializeIDBValueBuffer(impl->buffer(), isolate);
    case IDBAny::StringType:
        return v8String(isolate, impl->string());
    case IDBAny::IntegerType:
        return v8::Number::New(isolate, impl->integer());
    case IDBAny::KeyType:
        return toV8(impl->key(), creationContext, isolate);
    case IDBAny::KeyPathType:
        return toV8(impl->keyPath(), creationContext, isolate);
    case IDBAny::BufferKeyAndKeyPathType: {
        v8::Handle<v8::Value> value = deserializeIDBValueBuffer(impl->buffer(), isolate);
        v8::Handle<v8::Value> key = toV8(impl->key(), creationContext, isolate);
        bool injected = injectV8KeyIntoV8Value(key, value, impl->keyPath(), isolate);
        ASSERT_UNUSED(injected, injected);
        return value;
    }
    }

    ASSERT_NOT_REACHED();
    return v8::Undefined(isolate);
}

static const size_t maximumDepth = 2000;

static PassRefPtr<IDBKey> createIDBKeyFromValue(v8::Handle<v8::Value> value, Vector<v8::Handle<v8::Array> >& stack, v8::Isolate* isolate, bool allowExperimentalTypes = false)
{
    if (value->IsNumber() && !std::isnan(value->NumberValue()))
        return IDBKey::createNumber(value->NumberValue());
    if (value->IsString())
        return IDBKey::createString(toCoreString(value.As<v8::String>()));
    if (value->IsDate() && !std::isnan(value->NumberValue()))
        return IDBKey::createDate(value->NumberValue());
    if (value->IsUint8Array() && (allowExperimentalTypes || RuntimeEnabledFeatures::indexedDBExperimentalEnabled())) {
        // Per discussion in https://www.w3.org/Bugs/Public/show_bug.cgi?id=23332 the
        // input type is constrained to Uint8Array to match the output type.
        ArrayBufferView* view = WebCore::V8ArrayBufferView::toNative(value->ToObject());
        const char* start = static_cast<const char*>(view->baseAddress());
        size_t length = view->byteLength();
        return IDBKey::createBinary(SharedBuffer::create(start, length));
    }
    if (value->IsArray()) {
        v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(value);

        if (stack.contains(array))
            return nullptr;
        if (stack.size() >= maximumDepth)
            return nullptr;
        stack.append(array);

        IDBKey::KeyArray subkeys;
        uint32_t length = array->Length();
        for (uint32_t i = 0; i < length; ++i) {
            v8::Local<v8::Value> item = array->Get(v8::Int32::New(isolate, i));
            RefPtr<IDBKey> subkey = createIDBKeyFromValue(item, stack, isolate, allowExperimentalTypes);
            if (!subkey)
                subkeys.append(IDBKey::createInvalid());
            else
                subkeys.append(subkey);
        }

        stack.removeLast();
        return IDBKey::createArray(subkeys);
    }
    return nullptr;
}

static PassRefPtr<IDBKey> createIDBKeyFromValue(v8::Handle<v8::Value> value, v8::Isolate* isolate, bool allowExperimentalTypes = false)
{
    Vector<v8::Handle<v8::Array> > stack;
    RefPtr<IDBKey> key = createIDBKeyFromValue(value, stack, isolate, allowExperimentalTypes);
    if (key)
        return key;
    return IDBKey::createInvalid();
}

template<typename T>
static bool getValueFrom(T indexOrName, v8::Handle<v8::Value>& v8Value)
{
    v8::Local<v8::Object> object = v8Value->ToObject();
    if (!object->Has(indexOrName))
        return false;
    v8Value = object->Get(indexOrName);
    return true;
}

template<typename T>
static bool setValue(v8::Handle<v8::Value>& v8Object, T indexOrName, const v8::Handle<v8::Value>& v8Value)
{
    v8::Local<v8::Object> object = v8Object->ToObject();
    return object->Set(indexOrName, v8Value);
}

static bool get(v8::Handle<v8::Value>& object, const String& keyPathElement, v8::Handle<v8::Value>& result, v8::Isolate* isolate)
{
    if (object->IsString() && keyPathElement == "length") {
        int32_t length = v8::Handle<v8::String>::Cast(object)->Length();
        result = v8::Number::New(isolate, length);
        return true;
    }
    return object->IsObject() && getValueFrom(v8String(isolate, keyPathElement), result);
}

static bool canSet(v8::Handle<v8::Value>& object, const String& keyPathElement)
{
    return object->IsObject();
}

static bool set(v8::Handle<v8::Value>& object, const String& keyPathElement, const v8::Handle<v8::Value>& v8Value, v8::Isolate* isolate)
{
    return canSet(object, keyPathElement) && setValue(object, v8String(isolate, keyPathElement), v8Value);
}

static v8::Handle<v8::Value> getNthValueOnKeyPath(v8::Handle<v8::Value>& rootValue, const Vector<String>& keyPathElements, size_t index, v8::Isolate* isolate)
{
    v8::Handle<v8::Value> currentValue(rootValue);
    ASSERT(index <= keyPathElements.size());
    for (size_t i = 0; i < index; ++i) {
        v8::Handle<v8::Value> parentValue(currentValue);
        if (!get(parentValue, keyPathElements[i], currentValue, isolate))
            return v8Undefined();
    }

    return currentValue;
}

static bool canInjectNthValueOnKeyPath(v8::Handle<v8::Value>& rootValue, const Vector<String>& keyPathElements, size_t index, v8::Isolate* isolate)
{
    if (!rootValue->IsObject())
        return false;

    v8::Handle<v8::Value> currentValue(rootValue);

    ASSERT(index <= keyPathElements.size());
    for (size_t i = 0; i < index; ++i) {
        v8::Handle<v8::Value> parentValue(currentValue);
        const String& keyPathElement = keyPathElements[i];
        if (!get(parentValue, keyPathElement, currentValue, isolate))
            return canSet(parentValue, keyPathElement);
    }
    return true;
}


static v8::Handle<v8::Value> ensureNthValueOnKeyPath(v8::Handle<v8::Value>& rootValue, const Vector<String>& keyPathElements, size_t index, v8::Isolate* isolate)
{
    v8::Handle<v8::Value> currentValue(rootValue);

    ASSERT(index <= keyPathElements.size());
    for (size_t i = 0; i < index; ++i) {
        v8::Handle<v8::Value> parentValue(currentValue);
        const String& keyPathElement = keyPathElements[i];
        if (!get(parentValue, keyPathElement, currentValue, isolate)) {
            v8::Handle<v8::Object> object = v8::Object::New(isolate);
            if (!set(parentValue, keyPathElement, object, isolate))
                return v8Undefined();
            currentValue = object;
        }
    }

    return currentValue;
}

static PassRefPtr<IDBKey> createIDBKeyFromScriptValueAndKeyPath(const ScriptValue& value, const String& keyPath, v8::Isolate* isolate, bool allowExperimentalTypes)
{
    Vector<String> keyPathElements;
    IDBKeyPathParseError error;
    IDBParseKeyPath(keyPath, keyPathElements, error);
    ASSERT(error == IDBKeyPathParseErrorNone);
    ASSERT(isolate->InContext());

    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Value> v8Value(value.v8Value());
    v8::Handle<v8::Value> v8Key(getNthValueOnKeyPath(v8Value, keyPathElements, keyPathElements.size(), isolate));
    if (v8Key.IsEmpty())
        return nullptr;
    return createIDBKeyFromValue(v8Key, isolate, allowExperimentalTypes);
}

static PassRefPtr<IDBKey> createIDBKeyFromScriptValueAndKeyPath(const ScriptValue& value, const IDBKeyPath& keyPath, v8::Isolate* isolate, bool allowExperimentalTypes = false)
{
    ASSERT(!keyPath.isNull());
    v8::HandleScope handleScope(isolate);
    if (keyPath.type() == IDBKeyPath::ArrayType) {
        IDBKey::KeyArray result;
        const Vector<String>& array = keyPath.array();
        for (size_t i = 0; i < array.size(); ++i) {
            RefPtr<IDBKey> key = createIDBKeyFromScriptValueAndKeyPath(value, array[i], isolate, allowExperimentalTypes);
            if (!key)
                return nullptr;
            result.append(key);
        }
        return IDBKey::createArray(result);
    }

    ASSERT(keyPath.type() == IDBKeyPath::StringType);
    return createIDBKeyFromScriptValueAndKeyPath(value, keyPath.string(), isolate, allowExperimentalTypes);
}

PassRefPtr<IDBKey> createIDBKeyFromScriptValueAndKeyPath(DOMRequestState* state, const ScriptValue& value, const IDBKeyPath& keyPath)
{
    IDB_TRACE("createIDBKeyFromScriptValueAndKeyPath");
    v8::Isolate* isolate = state ? state->isolate() : v8::Isolate::GetCurrent();
    return createIDBKeyFromScriptValueAndKeyPath(value, keyPath, isolate);
}

static v8::Handle<v8::Value> deserializeIDBValueBuffer(SharedBuffer* buffer, v8::Isolate* isolate)
{
    ASSERT(isolate->InContext());
    if (!buffer)
        return v8::Null(isolate);

    // FIXME: The extra copy here can be eliminated by allowing SerializedScriptValue to take a raw const char* or const uint8_t*.
    Vector<uint8_t> value;
    value.append(buffer->data(), buffer->size());
    RefPtr<SerializedScriptValue> serializedValue = SerializedScriptValue::createFromWireBytes(value);
    return serializedValue->deserialize(isolate, 0, 0);
}

bool injectV8KeyIntoV8Value(v8::Handle<v8::Value> key, v8::Handle<v8::Value> value, const IDBKeyPath& keyPath, v8::Isolate* isolate)
{
    IDB_TRACE("injectIDBV8KeyIntoV8Value");
    ASSERT(isolate->InContext());

    ASSERT(keyPath.type() == IDBKeyPath::StringType);
    Vector<String> keyPathElements;
    IDBKeyPathParseError error;
    IDBParseKeyPath(keyPath.string(), keyPathElements, error);
    ASSERT(error == IDBKeyPathParseErrorNone);

    if (!keyPathElements.size())
        return false;

    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Value> parent(ensureNthValueOnKeyPath(value, keyPathElements, keyPathElements.size() - 1, isolate));
    if (parent.IsEmpty())
        return false;

    if (!set(parent, keyPathElements.last(), key, isolate))
        return false;

    return true;
}

bool canInjectIDBKeyIntoScriptValue(DOMRequestState* state, const ScriptValue& scriptValue, const IDBKeyPath& keyPath)
{
    IDB_TRACE("canInjectIDBKeyIntoScriptValue");
    ASSERT(keyPath.type() == IDBKeyPath::StringType);
    Vector<String> keyPathElements;
    IDBKeyPathParseError error;
    IDBParseKeyPath(keyPath.string(), keyPathElements, error);
    ASSERT(error == IDBKeyPathParseErrorNone);

    if (!keyPathElements.size())
        return false;

    v8::Handle<v8::Value> v8Value(scriptValue.v8Value());
    return canInjectNthValueOnKeyPath(v8Value, keyPathElements, keyPathElements.size() - 1, state->context()->GetIsolate());
}

ScriptValue idbAnyToScriptValue(DOMRequestState* state, PassRefPtr<IDBAny> any)
{
    v8::Isolate* isolate = state ? state->isolate() : v8::Isolate::GetCurrent();
    ASSERT(isolate->InContext());
    v8::Local<v8::Context> context = state ? state->context() : isolate->GetCurrentContext();
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Value> v8Value(toV8(any.get(), context->Global(), isolate));
    return ScriptValue(v8Value, isolate);
}

ScriptValue idbKeyToScriptValue(DOMRequestState* state, PassRefPtr<IDBKey> key)
{
    v8::Isolate* isolate = state ? state->isolate() : v8::Isolate::GetCurrent();
    ASSERT(isolate->InContext());
    v8::Local<v8::Context> context = state ? state->context() : isolate->GetCurrentContext();
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Value> v8Value(toV8(key.get(), context->Global(), isolate));
    return ScriptValue(v8Value, isolate);
}

PassRefPtr<IDBKey> scriptValueToIDBKey(DOMRequestState* state, const ScriptValue& scriptValue)
{
    v8::Isolate* isolate = state ? state->isolate() : v8::Isolate::GetCurrent();
    ASSERT(isolate->InContext());
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Value> v8Value(scriptValue.v8Value());
    return createIDBKeyFromValue(v8Value, isolate);
}

PassRefPtr<IDBKeyRange> scriptValueToIDBKeyRange(DOMRequestState* state, const ScriptValue& scriptValue)
{
    v8::Isolate* isolate = state ? state->isolate() : v8::Isolate::GetCurrent();
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Value> value(scriptValue.v8Value());
    return V8IDBKeyRange::toNativeWithTypeCheck(isolate, value);
}

#ifndef NDEBUG
void assertPrimaryKeyValidOrInjectable(DOMRequestState* state, PassRefPtr<SharedBuffer> buffer, PassRefPtr<IDBKey> prpKey, const IDBKeyPath& keyPath)
{
    RefPtr<IDBKey> key(prpKey);

    DOMRequestState::Scope scope(*state);
    v8::Isolate* isolate = state ? state->isolate() : v8::Isolate::GetCurrent();

    ScriptValue keyValue = idbKeyToScriptValue(state, key);
    ScriptValue scriptValue(deserializeIDBValueBuffer(buffer.get(), isolate), isolate);

    // This assertion is about already persisted data, so allow experimental types.
    const bool allowExperimentalTypes = true;
    RefPtr<IDBKey> expectedKey = createIDBKeyFromScriptValueAndKeyPath(scriptValue, keyPath, isolate, allowExperimentalTypes);
    ASSERT(!expectedKey || expectedKey->isEqual(key.get()));

    bool injected = injectV8KeyIntoV8Value(keyValue.v8Value(), scriptValue.v8Value(), keyPath, isolate);
    ASSERT_UNUSED(injected, injected);
}
#endif

} // namespace WebCore

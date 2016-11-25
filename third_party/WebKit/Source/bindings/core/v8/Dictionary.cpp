/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "bindings/core/v8/Dictionary.h"

#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/V8StringResource.h"
#include "core/dom/ExecutionContext.h"

namespace blink {

Dictionary::Dictionary(v8::Isolate* isolate,
                       v8::Local<v8::Value> dictionaryObject,
                       ExceptionState& exceptionState)
    : m_isolate(isolate) {
  DCHECK(isolate);

  // https://heycam.github.io/webidl/#es-dictionary
  // Type of an ECMAScript value must be Undefined, Null or Object.
  if (dictionaryObject.IsEmpty() || dictionaryObject->IsUndefined()) {
    m_valueType = ValueType::Undefined;
    return;
  }
  if (dictionaryObject->IsNull()) {
    m_valueType = ValueType::Null;
    return;
  }
  if (dictionaryObject->IsObject()) {
    m_valueType = ValueType::Object;
    m_dictionaryObject = dictionaryObject.As<v8::Object>();
    return;
  }

  exceptionState.throwTypeError(
      "The dictionary provided is neither undefined, null nor an Object.");
}

bool Dictionary::hasProperty(const StringView& key,
                             ExceptionState& exceptionState) const {
  if (m_dictionaryObject.IsEmpty())
    return false;

  v8::TryCatch tryCatch(m_isolate);
  bool hasKey = false;
  if (!m_dictionaryObject->Has(v8Context(), v8String(m_isolate, key))
           .To(&hasKey)) {
    exceptionState.rethrowV8Exception(tryCatch.Exception());
    return false;
  }

  return hasKey;
}

DictionaryIterator Dictionary::getIterator(
    ExecutionContext* executionContext) const {
  v8::Local<v8::Value> iteratorGetter;
  if (!getInternal(v8::Symbol::GetIterator(m_isolate), iteratorGetter) ||
      !iteratorGetter->IsFunction())
    return nullptr;
  v8::Local<v8::Value> iterator;
  if (!v8Call(V8ScriptRunner::callFunction(
                  v8::Local<v8::Function>::Cast(iteratorGetter),
                  executionContext, m_dictionaryObject, 0, nullptr, m_isolate),
              iterator))
    return nullptr;
  if (!iterator->IsObject())
    return nullptr;
  return DictionaryIterator(v8::Local<v8::Object>::Cast(iterator), m_isolate);
}

bool Dictionary::get(const StringView& key, Dictionary& value) const {
  v8::Local<v8::Value> v8Value;
  if (!get(key, v8Value))
    return false;

  if (v8Value->IsObject()) {
    ASSERT(m_isolate);
    ASSERT(m_isolate == v8::Isolate::GetCurrent());
    // TODO(bashi,yukishiino): Should rethrow the exception.
    // http://crbug.com/666661
    TrackExceptionState exceptionState;
    value = Dictionary(m_isolate, v8Value, exceptionState);
  }

  return true;
}

bool Dictionary::getInternal(const v8::Local<v8::Value>& key,
                             v8::Local<v8::Value>& result) const {
  if (m_dictionaryObject.IsEmpty())
    return false;

  if (!v8CallBoolean(m_dictionaryObject->Has(v8Context(), key)))
    return false;

  // Swallow a possible exception in v8::Object::Get().
  // TODO(bashi,yukishiino): Should rethrow the exception.
  // http://crbug.com/666661
  v8::TryCatch tryCatch(isolate());
  return m_dictionaryObject->Get(v8Context(), key).ToLocal(&result);
}

WARN_UNUSED_RESULT static v8::MaybeLocal<v8::String> getStringValueInArray(
    v8::Local<v8::Context> context,
    v8::Local<v8::Array> array,
    uint32_t index) {
  v8::Local<v8::Value> value;
  if (!array->Get(context, index).ToLocal(&value))
    return v8::MaybeLocal<v8::String>();
  return value->ToString(context);
}

HashMap<String, String> Dictionary::getOwnPropertiesAsStringHashMap(
    ExceptionState& exceptionState) const {
  if (m_dictionaryObject.IsEmpty())
    return HashMap<String, String>();

  v8::TryCatch tryCatch(isolate());
  v8::Local<v8::Array> propertyNames;
  if (!m_dictionaryObject->GetOwnPropertyNames(v8Context())
           .ToLocal(&propertyNames)) {
    exceptionState.rethrowV8Exception(tryCatch.Exception());
    return HashMap<String, String>();
  }

  HashMap<String, String> ownProperties;
  for (uint32_t i = 0; i < propertyNames->Length(); ++i) {
    v8::Local<v8::String> key;
    if (!getStringValueInArray(v8Context(), propertyNames, i).ToLocal(&key)) {
      exceptionState.rethrowV8Exception(tryCatch.Exception());
      return HashMap<String, String>();
    }
    V8StringResource<> stringKey(key);
    if (!stringKey.prepare(isolate(), exceptionState))
      return HashMap<String, String>();

    v8::Local<v8::Value> value;
    if (!m_dictionaryObject->Get(v8Context(), key).ToLocal(&value)) {
      exceptionState.rethrowV8Exception(tryCatch.Exception());
      return HashMap<String, String>();
    }
    V8StringResource<> stringValue(value);
    if (!stringValue.prepare(isolate(), exceptionState))
      return HashMap<String, String>();

    if (!static_cast<const String&>(stringKey).isEmpty())
      ownProperties.set(stringKey, stringValue);
  }

  return ownProperties;
}

Vector<String> Dictionary::getPropertyNames(
    ExceptionState& exceptionState) const {
  if (m_dictionaryObject.IsEmpty())
    return Vector<String>();

  v8::TryCatch tryCatch(isolate());
  v8::Local<v8::Array> propertyNames;
  if (!m_dictionaryObject->GetPropertyNames(v8Context())
           .ToLocal(&propertyNames)) {
    exceptionState.rethrowV8Exception(tryCatch.Exception());
    return Vector<String>();
  }

  Vector<String> names;
  for (uint32_t i = 0; i < propertyNames->Length(); ++i) {
    v8::Local<v8::String> key;
    if (!getStringValueInArray(v8Context(), propertyNames, i).ToLocal(&key)) {
      exceptionState.rethrowV8Exception(tryCatch.Exception());
      return Vector<String>();
    }
    V8StringResource<> stringKey(key);
    if (!stringKey.prepare(isolate(), exceptionState))
      return Vector<String>();

    names.append(stringKey);
  }

  return names;
}

}  // namespace blink

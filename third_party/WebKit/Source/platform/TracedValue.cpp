// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "platform/TracedValue.h"

#include "platform/JSONValues.h"

namespace blink {

namespace {

class ConvertibleToTraceFormatJSONValue : public TraceEvent::ConvertableToTraceFormat {
    WTF_MAKE_NONCOPYABLE(ConvertibleToTraceFormatJSONValue);
public:
    explicit ConvertibleToTraceFormatJSONValue(PassRefPtr<JSONValue> value) : m_value(value) { }
    virtual String asTraceFormat() const OVERRIDE
    {
        return m_value->toJSONString();
    }

private:
    virtual ~ConvertibleToTraceFormatJSONValue() { }

    RefPtr<JSONValue> m_value;
};

String threadSafeCopy(const String& string)
{
    RefPtr<StringImpl> copy(string.impl());
    if (string.isSafeToSendToAnotherThread())
        return string;
    return string.isolatedCopy();
}

}

TracedValueBase::TracedValueBase()
{
}

TracedValueBase::~TracedValueBase()
{
}

void TracedValueBase::setInteger(const char* name, int value)
{
    currentDictionary()->setNumber(name, value);
}

void TracedValueBase::setDouble(const char* name, double value)
{
    currentDictionary()->setNumber(name, value);
}

void TracedValueBase::setBoolean(const char* name, bool value)
{
    currentDictionary()->setBoolean(name, value);
}

void TracedValueBase::setString(const char* name, const String& value)
{
    currentDictionary()->setString(name, threadSafeCopy(value));
}

void TracedValueBase::beginDictionaryNamed(const char* name)
{
    RefPtr<JSONObject> dictionary = JSONObject::create();
    currentDictionary()->setObject(name, dictionary);
    m_stack.append(dictionary);
}

void TracedValueBase::beginArrayNamed(const char* name)
{
    RefPtr<JSONArray> array = JSONArray::create();
    currentDictionary()->setArray(name, array);
    m_stack.append(array);
}

void TracedValueBase::endCurrentDictionary()
{
    ASSERT(m_stack.size() > 1);
    ASSERT(currentDictionary());
    m_stack.removeLast();
}

void TracedValueBase::pushInteger(int value)
{
    currentArray()->pushInt(value);
}

void TracedValueBase::pushDouble(double value)
{
    currentArray()->pushNumber(value);
}

void TracedValueBase::pushBoolean(bool value)
{
    currentArray()->pushBoolean(value);
}

void TracedValueBase::pushString(const String& value)
{
    currentArray()->pushString(threadSafeCopy(value));
}

void TracedValueBase::pushArray()
{
    RefPtr<JSONArray> array = JSONArray::create();
    currentArray()->pushArray(array);
    m_stack.append(array);
}

void TracedValueBase::pushDictionary()
{
    RefPtr<JSONObject> dictionary = JSONObject::create();
    currentArray()->pushObject(dictionary);
    m_stack.append(dictionary);
}

void TracedValueBase::endCurrentArray()
{
    ASSERT(m_stack.size() > 1);
    ASSERT(currentArray());
    m_stack.removeLast();
}

JSONObject* TracedValueBase::currentDictionary() const
{
    ASSERT(!m_stack.isEmpty());
    ASSERT(m_stack.last()->type() == JSONValue::TypeObject);
    return static_cast<JSONObject*>(m_stack.last().get());
}

JSONArray* TracedValueBase::currentArray() const
{
    ASSERT(!m_stack.isEmpty());
    ASSERT(m_stack.last()->type() == JSONValue::TypeArray);
    return static_cast<JSONArray*>(m_stack.last().get());
}

TracedValue::TracedValue()
{
    m_stack.append(JSONObject::create());
}

TracedValue::~TracedValue()
{
    ASSERT(m_stack.isEmpty());
}

TracedDictionary<TracedValue>& TracedValue::beginDictionary(const char* name)
{
    beginDictionaryNamed(name);
    return *reinterpret_cast<TracedDictionary<TracedValue>* >(this);
}

TracedArray<TracedValue>& TracedValue::beginArray(const char* name)
{
    beginArrayNamed(name);
    return *reinterpret_cast<TracedArray<TracedValue>* >(this);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> TracedValue::finish()
{
    ASSERT(m_stack.size() == 1);
    RefPtr<JSONValue> value(currentDictionary());
    m_stack.clear();
    return adoptRef(new ConvertibleToTraceFormatJSONValue(value));
}

}

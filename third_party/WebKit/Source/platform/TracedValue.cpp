// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "platform/TracedValue.h"

#include "platform/Decimal.h"
#include "platform/JSONValues.h"
#include "wtf/HashMap.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringHash.h"

namespace blink {

namespace {

String threadSafeCopy(const String& string)
{
    RefPtr<StringImpl> copy(string.impl());
    if (string.isSafeToSendToAnotherThread())
        return string;
    return string.isolatedCopy();
}

} // namespace

// InternalValue and its subclasses are JSON like objects which TracedValue
// internally uses. We don't use JSONValues because we want to count how much
// memory TracedValue uses.
// TODO(bashi): InternalValue should use an allocator which counts allocation
// size as tracing overhead.
class InternalValue : public RefCounted<InternalValue> {
public:
    typedef enum {
        TypeNone,
        TypeNumber,
        TypeBoolean,
        TypeString,
        TypeArray,
        TypeDictionary,
    } Type;
    virtual ~InternalValue() { }
    virtual Type type()
    {
        ASSERT_NOT_REACHED();
        return TypeNone;
    }
    virtual void toJSON(StringBuilder* builder)
    {
        ASSERT_NOT_REACHED();
    }
};

class TracedNumberValue : public InternalValue {
public:
    static PassRefPtr<TracedNumberValue> create(double value)
    {
        return adoptRef(new TracedNumberValue(value));
    }
    virtual ~TracedNumberValue() { }
    virtual Type type() { return TypeNumber; }
    virtual void toJSON(StringBuilder* builder)
    {
        builder->append(Decimal::fromDouble(m_value).toString());
    }

private:
    explicit TracedNumberValue(double value) : m_value(value) { }
    double m_value;
};

class TracedBooleanValue : public InternalValue {
public:
    static PassRefPtr<TracedBooleanValue> create(bool value)
    {
        return adoptRef(new TracedBooleanValue(value));
    }
    virtual ~TracedBooleanValue() { }
    virtual Type type() { return TypeBoolean; }
    virtual void toJSON(StringBuilder* builder)
    {
        builder->append(m_value ? "true" : "false");
    }

private:
    explicit TracedBooleanValue(bool value) : m_value(value) { }
    bool m_value;
};

class TracedStringValue : public InternalValue {
public:
    static PassRefPtr<TracedStringValue> create(String value)
    {
        return adoptRef(new TracedStringValue(value));
    }
    virtual ~TracedStringValue() { }
    virtual Type type() { return TypeString; }
    virtual void toJSON(StringBuilder* builder)
    {
        doubleQuoteStringForJSON(m_value, builder);
    }

private:
    explicit TracedStringValue(String value) : m_value(threadSafeCopy(value)) { }
    String m_value;
};

class TracedArrayValue : public InternalValue {
public:
    static PassRefPtr<TracedArrayValue> create()
    {
        return adoptRef(new TracedArrayValue());
    }
    virtual ~TracedArrayValue() { }
    virtual Type type() { return TypeArray; }
    virtual void toJSON(StringBuilder* builder)
    {
        builder->append('[');
        for (TracedValueVector::const_iterator it = m_value.begin(); it != m_value.end(); ++it) {
            if (it != m_value.begin())
                builder->append(',');
            (*it)->toJSON(builder);
        }
        builder->append(']');
    }
    void push(PassRefPtr<InternalValue> value) { m_value.append(value); }

private:
    TracedArrayValue() { }
    TracedValueVector m_value;
};

class TracedDictionaryValue : public InternalValue {
public:
    static PassRefPtr<TracedDictionaryValue> create()
    {
        return adoptRef(new TracedDictionaryValue());
    }
    virtual ~TracedDictionaryValue() { }
    virtual Type type() { return TypeDictionary; }
    virtual void toJSON(StringBuilder* builder)
    {
        builder->append('{');
        for (size_t i = 0; i < m_order.size(); ++i) {
            TracedValueHashMap::const_iterator it = m_value.find(m_order[i]);
            if (i)
                builder->append(',');
            doubleQuoteStringForJSON(it->key, builder);
            builder->append(':');
            it->value->toJSON(builder);
        }
        builder->append('}');
    }
    void set(const char* name, PassRefPtr<InternalValue> value)
    {
        String nameString = String(name);
        if (m_value.set(nameString, value).isNewEntry)
            m_order.append(nameString);
    }

private:
    TracedDictionaryValue() { }
    TracedValueHashMap m_value;
    Vector<String> m_order;
};

PassRefPtr<TracedValue> TracedValue::create()
{
    return adoptRef(new TracedValue());
}

TracedValue::TracedValue()
{
    m_stack.append(TracedDictionaryValue::create());
}

TracedValue::~TracedValue()
{
    ASSERT(m_stack.size() == 1);
}

void TracedValue::setInteger(const char* name, int value)
{
    currentDictionary()->set(name, TracedNumberValue::create(value));
}

void TracedValue::setDouble(const char* name, double value)
{
    currentDictionary()->set(name, TracedNumberValue::create(value));
}

void TracedValue::setBoolean(const char* name, bool value)
{
    currentDictionary()->set(name, TracedBooleanValue::create(value));
}

void TracedValue::setString(const char* name, const String& value)
{
    currentDictionary()->set(name, TracedStringValue::create(value));
}

void TracedValue::beginDictionary(const char* name)
{
    RefPtr<TracedDictionaryValue> dictionary = TracedDictionaryValue::create();
    currentDictionary()->set(name, dictionary);
    m_stack.append(dictionary);
}

void TracedValue::beginArray(const char* name)
{
    RefPtr<TracedArrayValue> array = TracedArrayValue::create();
    currentDictionary()->set(name, array);
    m_stack.append(array);
}

void TracedValue::endDictionary()
{
    ASSERT(m_stack.size() > 1);
    ASSERT(currentDictionary());
    m_stack.removeLast();
}

void TracedValue::pushInteger(int value)
{
    currentArray()->push(TracedNumberValue::create(value));
}

void TracedValue::pushDouble(double value)
{
    currentArray()->push(TracedNumberValue::create(value));
}

void TracedValue::pushBoolean(bool value)
{
    currentArray()->push(TracedBooleanValue::create(value));
}

void TracedValue::pushString(const String& value)
{
    currentArray()->push(TracedStringValue::create(value));
}

void TracedValue::beginArray()
{
    RefPtr<TracedArrayValue> array = TracedArrayValue::create();
    currentArray()->push(array);
    m_stack.append(array);
}

void TracedValue::beginDictionary()
{
    RefPtr<TracedDictionaryValue> dictionary = TracedDictionaryValue::create();
    currentArray()->push(dictionary);
    m_stack.append(dictionary);
}

void TracedValue::endArray()
{
    ASSERT(m_stack.size() > 1);
    ASSERT(currentArray());
    m_stack.removeLast();
}

String TracedValue::asTraceFormat() const
{
    ASSERT(m_stack.size() == 1);
    StringBuilder builder;
    m_stack.first()->toJSON(&builder);
    return builder.toString();
}

TracedDictionaryValue* TracedValue::currentDictionary() const
{
    ASSERT(!m_stack.isEmpty());
    ASSERT(m_stack.last()->type() == InternalValue::TypeDictionary);
    return static_cast<TracedDictionaryValue*>(m_stack.last().get());
}

TracedArrayValue* TracedValue::currentArray() const
{
    ASSERT(!m_stack.isEmpty());
    ASSERT(m_stack.last()->type() == InternalValue::TypeArray);
    return static_cast<TracedArrayValue*>(m_stack.last().get());
}

}

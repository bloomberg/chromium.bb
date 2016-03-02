// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/inspector_protocol/Values.h"

#include "platform/Decimal.h"
#include "platform/inspector_protocol/Parser.h"
#include "wtf/MathExtras.h"
#include "wtf/text/StringBuilder.h"

namespace blink {
namespace protocol {

namespace {

const char* const nullString = "null";
const char* const trueString = "true";
const char* const falseString = "false";

inline bool escapeChar(UChar c, StringBuilder* dst)
{
    switch (c) {
    case '\b': dst->appendLiteral("\\b"); break;
    case '\f': dst->appendLiteral("\\f"); break;
    case '\n': dst->appendLiteral("\\n"); break;
    case '\r': dst->appendLiteral("\\r"); break;
    case '\t': dst->appendLiteral("\\t"); break;
    case '\\': dst->appendLiteral("\\\\"); break;
    case '"': dst->appendLiteral("\\\""); break;
    default:
        return false;
    }
    return true;
}

void escapeStringForJSON(const String& str, StringBuilder* dst)
{
    for (unsigned i = 0; i < str.length(); ++i) {
        UChar c = str[i];
        if (!escapeChar(c, dst)) {
            if (c < 32 || c > 126 || c == '<' || c == '>') {
                // 1. Escaping <, > to prevent script execution.
                // 2. Technically, we could also pass through c > 126 as UTF8, but this
                //    is also optional. It would also be a pain to implement here.
                unsigned symbol = static_cast<unsigned>(c);
                String symbolCode = String::format("\\u%04X", symbol);
                dst->append(symbolCode);
            } else {
                dst->append(c);
            }
        }
    }
}

void doubleQuoteStringForJSON(const String& str, StringBuilder* dst)
{
    dst->append('"');
    escapeStringForJSON(str, dst);
    dst->append('"');
}

} // anonymous namespace

bool Value::asBoolean(bool*) const
{
    return false;
}

bool Value::asNumber(double*) const
{
    return false;
}

bool Value::asNumber(int*) const
{
    return false;
}

bool Value::asString(String*) const
{
    return false;
}

String Value::toJSONString() const
{
    StringBuilder result;
    result.reserveCapacity(512);
    writeJSON(&result);
    return result.toString();
}

void Value::writeJSON(StringBuilder* output) const
{
    ASSERT(m_type == TypeNull);
    output->append(nullString, 4);
}

PassOwnPtr<Value> Value::clone() const
{
    return Value::null();
}

bool FundamentalValue::asBoolean(bool* output) const
{
    if (type() != TypeBoolean)
        return false;
    *output = m_boolValue;
    return true;
}

bool FundamentalValue::asNumber(double* output) const
{
    if (type() != TypeNumber)
        return false;
    *output = m_doubleValue;
    return true;
}

bool FundamentalValue::asNumber(int* output) const
{
    if (type() != TypeNumber)
        return false;
    *output = static_cast<int>(m_doubleValue);
    return true;
}

void FundamentalValue::writeJSON(StringBuilder* output) const
{
    ASSERT(type() == TypeBoolean || type() == TypeNumber);
    if (type() == TypeBoolean) {
        if (m_boolValue)
            output->append(trueString, 4);
        else
            output->append(falseString, 5);
    } else if (type() == TypeNumber) {
        if (!std::isfinite(m_doubleValue)) {
            output->append(nullString, 4);
            return;
        }
        output->append(Decimal::fromDouble(m_doubleValue).toString());
    }
}

PassOwnPtr<Value> FundamentalValue::clone() const
{
    return type() == TypeNumber ? FundamentalValue::create(m_doubleValue) : FundamentalValue::create(m_boolValue);
}

bool StringValue::asString(String* output) const
{
    *output = m_stringValue;
    return true;
}

void StringValue::writeJSON(StringBuilder* output) const
{
    ASSERT(type() == TypeString);
    doubleQuoteStringForJSON(m_stringValue, output);
}

PassOwnPtr<Value> StringValue::clone() const
{
    return StringValue::create(m_stringValue);
}

DictionaryValue::~DictionaryValue()
{
}

void DictionaryValue::setBoolean(const String& name, bool value)
{
    setValue(name, FundamentalValue::create(value));
}

void DictionaryValue::setNumber(const String& name, double value)
{
    setValue(name, FundamentalValue::create(value));
}

void DictionaryValue::setString(const String& name, const String& value)
{
    setValue(name, StringValue::create(value));
}

void DictionaryValue::setValue(const String& name, PassOwnPtr<Value> value)
{
    ASSERT(value);
    if (m_data.set(name, value).isNewEntry)
        m_order.append(name);
}

void DictionaryValue::setObject(const String& name, PassOwnPtr<DictionaryValue> value)
{
    ASSERT(value);
    if (m_data.set(name, value).isNewEntry)
        m_order.append(name);
}

void DictionaryValue::setArray(const String& name, PassOwnPtr<ListValue> value)
{
    ASSERT(value);
    if (m_data.set(name, value).isNewEntry)
        m_order.append(name);
}

bool DictionaryValue::getBoolean(const String& name, bool* output) const
{
    protocol::Value* value = get(name);
    if (!value)
        return false;
    return value->asBoolean(output);
}

bool DictionaryValue::getString(const String& name, String* output) const
{
    protocol::Value* value = get(name);
    if (!value)
        return false;
    return value->asString(output);
}

DictionaryValue* DictionaryValue::getObject(const String& name) const
{
    return DictionaryValue::cast(get(name));
}

protocol::ListValue* DictionaryValue::getArray(const String& name) const
{
    return ListValue::cast(get(name));
}

protocol::Value* DictionaryValue::get(const String& name) const
{
    Dictionary::const_iterator it = m_data.find(name);
    if (it == m_data.end())
        return nullptr;
    return it->value.get();
}

DictionaryValue::Entry DictionaryValue::at(size_t index) const
{
    String key = m_order[index];
    return std::make_pair(key, m_data.get(key));
}

bool DictionaryValue::booleanProperty(const String& name, bool defaultValue) const
{
    bool result = defaultValue;
    getBoolean(name, &result);
    return result;
}

void DictionaryValue::remove(const String& name)
{
    m_data.remove(name);
    for (size_t i = 0; i < m_order.size(); ++i) {
        if (m_order[i] == name) {
            m_order.remove(i);
            break;
        }
    }
}

void DictionaryValue::writeJSON(StringBuilder* output) const
{
    output->append('{');
    for (size_t i = 0; i < m_order.size(); ++i) {
        Dictionary::const_iterator it = m_data.find(m_order[i]);
        ASSERT_WITH_SECURITY_IMPLICATION(it != m_data.end());
        if (i)
            output->append(',');
        doubleQuoteStringForJSON(it->key, output);
        output->append(':');
        it->value->writeJSON(output);
    }
    output->append('}');
}

PassOwnPtr<Value> DictionaryValue::clone() const
{
    OwnPtr<DictionaryValue> result = DictionaryValue::create();
    for (size_t i = 0; i < m_order.size(); ++i) {
        Dictionary::const_iterator it = m_data.find(m_order[i]);
        ASSERT(it != m_data.end());
        result->setValue(it->key, it->value->clone());
    }
    return result.release();
}

DictionaryValue::DictionaryValue()
    : Value(TypeObject)
    , m_data()
    , m_order()
{
}

ListValue::~ListValue()
{
}

void ListValue::writeJSON(StringBuilder* output) const
{
    output->append('[');
    for (Vector<OwnPtr<protocol::Value>>::const_iterator it = m_data.begin(); it != m_data.end(); ++it) {
        if (it != m_data.begin())
            output->append(',');
        (*it)->writeJSON(output);
    }
    output->append(']');
}

PassOwnPtr<Value> ListValue::clone() const
{
    OwnPtr<ListValue> result = ListValue::create();
    for (Vector<OwnPtr<protocol::Value>>::const_iterator it = m_data.begin(); it != m_data.end(); ++it)
        result->pushValue((*it)->clone());
    return result.release();
}

ListValue::ListValue()
    : Value(TypeArray)
    , m_data()
{
}

void ListValue::pushValue(PassOwnPtr<protocol::Value> value)
{
    ASSERT(value);
    m_data.append(value);
}

protocol::Value* ListValue::at(size_t index)
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < m_data.size());
    return m_data[index].get();
}

} // namespace protocol
} // namespace blink
